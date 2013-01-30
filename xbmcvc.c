/*
 *
 * xbmcvc - a program for controlling XBMC with simple voice commands
 *
 * Copyright (C) Michal Kepien, 2012-2013.
 *
 * The listening code is heavily based on the sample continous listening
 * program (continous.c) provided along with pocketsphinx.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file LICENSE. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 */

/* CURL headers */
#include <curl/curl.h>

/* pocketsphinx headers */
#include <sphinxbase/ad.h>
#include <sphinxbase/cont_ad.h>
#include <pocketsphinx.h>

/* Other headers */
#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

/* Constants */
#define VERSION				"0.4"
#define USAGE_MESSAGE			"\n" \
					"Usage: xbmcvc [ -H host ] [ -P port ] [ -D device ] [ -l ] [ -n ] [ -t ] [ -V ] [ -h ]\n" \
					"\n" \
					"    -H hostname  Hostname or IP address of the XBMC instance you want to control (default: localhost)\n" \
					"    -P port      Port number the XBMC instance you want to control is listening on (default: 8080)\n" \
					"    -D device    Name of ALSA device to capture speech from\n" \
					"    -l           Disable locking/unlocking\n" \
					"    -n           Disable GUI notifications\n" \
					"    -t           Enable test mode - enter commands on stdin\n" \
					"    -V           Print version information and exit\n" \
					"    -h           Print this help message\n" \
					"\n"

#define COMMAND_UNLOCK			"X_B_M_C"
#define COMMAND_LOCK			"OKAY"
#define JSON_RPC_DEFAULT_HOST		"localhost"
#define JSON_RPC_DEFAULT_PORT		8080
#define JSON_RPC_URL			"http://%s:%s/jsonrpc"
#define JSON_RPC_POST			"{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"id\":1}"
#define JSON_RPC_POST_WITH_PARAMS	"{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":{%s},\"id\":1}"
#define MAX_ACTIONS			5
#define SPELLING_BUFFER_SIZE		256
#define XBMC_VERSION_EDEN		11
#define XBMC_VERSION_FRODO		12
#define XBMC_VERSION_MIN		XBMC_VERSION_EDEN
#define XBMC_VERSION_MAX		XBMC_VERSION_FRODO

/* Language model files */
#define MODEL_HMM			MODELDIR "/hmm/en_US/hub4wsj_sc_8k"
#define MODEL_LM			MODELDIR "/lm/en/xbmcvc/xbmcvc.lm"
#define MODEL_DICT			MODELDIR "/lm/en/xbmcvc/normal.dic"

/* Macros */
#define ARRAY_SIZE(array)		(sizeof(array) / sizeof(array[0]))

/* Modes of operation */
enum mode_t {
	MODE_NORMAL,
	MODE_SPELLING,
	MODE_NONE,
};

/* Structure passed to CURL callback */
typedef struct {
	char**	dst;	/* destination buffer */
	int	dst_s;	/* destination buffer size */
} curl_userdata_t;

/* Structure describing an action */
typedef struct {
	char*		word;
	char*		method;
	char*		params;
	const char**	req;
	int		req_size;
	int		repeats;
	int		needs_player_id;
	int		needs_argument;
} action_t;

/* Command to character mapping */
typedef struct {
	char*		string;
	int		character;
} cmap_t;

/* Names of modes of operation */
const char*	loglevels[] = { "EMERGENCY", "ALERT", "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG" };
const char*	modes[] = { "normal", "spelling" };

/* Global configuration variables */
char*		config_json_rpc_host;
char*		config_json_rpc_port;
char*		config_alsa_device;
int		config_locking = 1;
int		config_notifications = 1;
int		config_test_mode = 0;

/* Action database */
action_t**	actions = NULL;
int		actions_count = 0;
const char*	repeatable[] = { "DOWN", "LEFT", "NEXT", "PREVIOUS", "RIGHT", "UP" };
int		repeatable_size = ARRAY_SIZE(repeatable);
const char*	repeat_args[] = { "ALL:all", "ONE:one", "OFF:off", "cycle" };
int		repeat_args_size = ARRAY_SIZE(repeat_args);
const char*	volume_args[] = { "TEN:10", "TWENTY:20", "THIRTY:30", "FOURTY:40", "FIFTY:50", "SIXTY:60", "SEVENTY:70", "EIGHTY:80", "NINETY:90", "MAX:100" };
int		volume_args_size = ARRAY_SIZE(volume_args);

/* Command to character mapping database */
cmap_t**	cmap = NULL;
int		cmap_count = 0;

/* Miscellaneous variables */
int		locked = 1;
mode_t		mode = MODE_NORMAL;
char		spelling_buffer[SPELLING_BUFFER_SIZE];
int		spelling_case = 0;
int		xbmc_version;

/* Exit flag */
volatile int	exit_flag = 0;

/*---------------------------------------------------------------------------*/

void
set_exit_flag(int signal)
{
	exit_flag = 1;
}

void
cleanup(void)
{

	int i;

	/* Configuration */
	free(config_json_rpc_host);
	free(config_json_rpc_port);
	free(config_alsa_device);

	/* Actions database */
	for (i=0; i<actions_count; i++)
	{
		free(actions[i]->word);
		free(actions[i]->method);
		free(actions[i]->params);
		free(actions[i]->req);
		free(actions[i]);
	}
	free(actions);
	
	/* Command to character mapping */
	for (i=0; i<cmap_count; i++)
	{
		free(cmap[i]->string);
		free(cmap[i]);
	}
	free(cmap);

}

void
vprint_log(const int level, const char *format, va_list args)
{
	printf("%s: ", loglevels[level]);
	vprintf(format, args);
	printf("\n");
}

void
print_log(const int level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vprint_log(level, format, args);
	va_end(args);
}

void
die(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vprint_log(LOG_CRIT, format, args);
	va_end(args);
	exit(1);
}

void
parse_options(int argc, char *argv[])
{

	int option;
	int quit = 0;

	/* Initialize default values */
	config_json_rpc_host = malloc(strlen(JSON_RPC_DEFAULT_HOST) + 1);
	config_json_rpc_port = malloc(6);
	config_alsa_device = NULL;

	sprintf(config_json_rpc_host, "%s", JSON_RPC_DEFAULT_HOST);
	snprintf(config_json_rpc_port, 6, "%d", JSON_RPC_DEFAULT_PORT);

	/* Process command line options */
	while ((option = getopt(argc, argv, "H:P:D:lntVh")) != -1 && !quit)
	{
		switch(option)
		{

			/* XBMC host */
			case 'H':
				config_json_rpc_host = realloc(config_json_rpc_host, strlen(optarg) + 1);
				sprintf(config_json_rpc_host, "%s", optarg);
				break;

			/* XBMC port */
			case 'P':
				snprintf(config_json_rpc_port, 6, "%s", optarg);
				break;

			/* ALSA capture device */
			case 'D':
				config_alsa_device = realloc(config_alsa_device, strlen(optarg) + 1);
				sprintf(config_alsa_device, "%s", optarg);
				break;

			/* Locking */
			case 'l':
				config_locking = 0;
				break;

			/* Notifications */
			case 'n':
				config_notifications = 0;
				break;

			/* Test mode */
			case 't':
				config_test_mode = 1;
				break;

			/* Version information */
			case 'V':
				printf("xbmcvc " VERSION);
				if (strlen(GITVERSION) > 0)
					printf(" (Git: " GITVERSION ")");
				printf("\n");
				quit = 1;
				break;

			/* Help or unknown option */
			case 'h':
			default:
				printf(USAGE_MESSAGE);
				quit = 1;
				break;

		}
	}

	if (quit)
		exit(0);

}

int
in_array(const char* haystack[], const int haystack_size, const char *needle)
{
	int found = 0;
	int i = 0;
	while (i < haystack_size && !found)
	{
		if (strcmp(haystack[i++], needle) == 0)
			found = 1;
	}
	return found;
}

void
append_param(char **current, const char *append)
{
	if (*current)
	{
		*current = realloc(*current, strlen(*current) + strlen(append) + 2);
		strcat(*current, ",");
	}
	else
	{
		*current = malloc(strlen(append) + 1);
		**current = '\0';
	}
	strcat(*current, append);
}

/* CURL callback for saving HTTP response to a pointer passed via userdata */
size_t
save_response_in_memory(const char *ptr, const size_t size, const size_t nmemb, void *userdata)
{
	curl_userdata_t *cud = (curl_userdata_t *) userdata;
	*cud->dst = realloc(*cud->dst, cud->dst_s + (size * nmemb) + 1);
	memcpy(*cud->dst + cud->dst_s, ptr, size * nmemb);
	cud->dst_s += size * nmemb;
	/* Null-terminate response for easier handling */
	*(*cud->dst + cud->dst_s) = '\0';
	return size * nmemb;
}

int
send_json_rpc_request(const char *method, const char *params, char **dst)
{

	CURL*			curl;
	CURLcode		result;
	char*			url;
	char*			post;
	char*			response = NULL;
	curl_userdata_t		cud;
	struct curl_slist*	headers = NULL;

	/* Prepare JSON-RPC URL  */
	url = malloc(strlen(JSON_RPC_URL) + strlen(config_json_rpc_host) + strlen(config_json_rpc_port));
	sprintf(url, JSON_RPC_URL, config_json_rpc_host, config_json_rpc_port);

	/* Prepare POST data with or without parameters */
	if (params == NULL)
	{
		post = malloc(strlen(JSON_RPC_POST) + strlen(method));
		sprintf(post, JSON_RPC_POST, method);
	}
	else
	{
		post = malloc(strlen(JSON_RPC_POST_WITH_PARAMS) + strlen(method) + strlen(params));
		sprintf(post, JSON_RPC_POST_WITH_PARAMS, method, params);
	}

	/* Initialize userdata structure passed to callback */
	cud.dst = &response;
	cud.dst_s = 0;

	/* Initialize libcurl */
	if ((curl = curl_easy_init()) == NULL)
		die("Error initializing libcurl");

	/* Set request options */
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, save_response_in_memory);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &cud);

	/* Add proper Content-Type header */
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	/* Send JSON-RPC request */
	result = curl_easy_perform(curl);

	/* If caller provided a pointer, save response there (if it exists) */
	if (dst && response)
	{
		*dst = realloc(*dst, strlen(response) + 1);
		strcpy(*dst, response);
	}

	/* Cleanup */
	free(response);
	free(post);
	free(url);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	return (int) result;

}

void
send_gui_notification(const char *title, const char *message, const char *icon)
{

	const char *format = "\"title\":\"%s\",\"message\":\"%s\",\"image\":\"%s\"";
	char *params;

	if (xbmc_version >= XBMC_VERSION_FRODO && config_notifications)
	{
		params = malloc(strlen(format) + strlen(title) + strlen(message) + strlen(icon));
		sprintf(params, format, title, message, icon);
		send_json_rpc_request("GUI.ShowNotification", params, NULL);
		free(params);
	}

}

int
get_json_rpc_response_int(const char *method, const char *params, const char *param)
{

	char*		response = NULL;
	char*		result;
	char*		param_search = NULL;
	char*		param_string;
	char		param_value[4];
	unsigned int	i = 0;
	int		retval = -1;

	if (send_json_rpc_request(method, params, &response) != 0 || !response)
		return -2;

	result = strstr(response, "\"result\":");
	if (result)
	{
		result += strlen("\"result\":") + 1;
		param_search = malloc(strlen(param) + 4);
		sprintf(param_search, "\"%s\":", param);
		param_string = strstr(result, param_search);
		if (param_string)
		{
			memset(param_value, 0, sizeof(param_value));
			param_string += strlen(param) + 3;
			while(i < sizeof(param_value) && *param_string >= '0' && *param_string <= '9')
			{
				param_value[i] = *param_string;
				param_string++;
				i++;
			}
			if (i == 0)
				retval = -1;
			else
				retval = atoi(param_value);
		}
		free(param_search);
	}
	free(response);

	return retval;

}

void
register_action(const char* word, const char* method, const char* params, const char* req[], const int req_size, const int repeats, const int needs_player_id, const int needs_argument)
{

	/* Allocate memory for action structure */
	action_t* a = malloc(sizeof(action_t));

	/* Copy function arguments to structure fields */
	a->word = strdup(word);

	if (method)
		a->method = strdup(method);
	else
		a->method = NULL;

	if (params)
		a->params = strdup(params);
	else
		a->params = NULL;

	if (req)
	{
		a->req = calloc(req_size, sizeof(char *));
		memcpy(a->req, req, req_size * sizeof(char *));
	}
	else
	{
		a->req = NULL;
	}

	a->req_size = req_size;
	a->repeats = repeats;
	a->needs_player_id = needs_player_id;
	a->needs_argument = needs_argument;

	/* Expand action database */
	actions = realloc(actions, (actions_count + 1) * sizeof(action_t *));
	/* Add action to database */
	actions[actions_count] = a;
	actions_count++;

}

void
initialize_actions(void)
{

	/* General actions */
	register_action("BACK", "Input.Back", NULL, NULL, 0, 1, 0, 0);
	register_action("DOWN", "Input.Down", NULL, NULL, 0, 1, 0, 0);
	register_action("HOME", "Input.Home", NULL, NULL, 0, 1, 0, 0);
	register_action("LEFT", "Input.Left", NULL, NULL, 0, 1, 0, 0);
	register_action("MUTE", "Application.SetMute", "\"mute\": true", NULL, 0, 1, 0, 0);
	register_action("RIGHT", "Input.Right", NULL, NULL, 0, 1, 0, 0);
	register_action("SELECT", "Input.Select", NULL, NULL, 0, 1, 0, 0);
	register_action("UNMUTE", "Application.SetMute", "\"mute\": false", NULL, 0, 1, 0, 0);
	register_action("UP", "Input.Up", NULL, NULL, 0, 1, 0, 0);
	register_action("VOLUME", "Application.SetVolume", "\"volume\":%s", volume_args, volume_args_size, 1, 0, 1);

	/* Repeating actions */
	register_action("TWO", NULL, NULL, repeatable, repeatable_size, 2, 0, 0);
	register_action("THREE", NULL, NULL, repeatable, repeatable_size, 3, 0, 0);
	register_action("FOUR", NULL, NULL, repeatable, repeatable_size, 4, 0, 0);
	register_action("FIVE", NULL, NULL, repeatable, repeatable_size, 5, 0, 0);

	/* Version-dependent actions */
	switch(xbmc_version)
	{

		case XBMC_VERSION_EDEN:
			register_action("NEXT", "Player.GoNext", NULL, NULL, 0, 1, 1, 0);
			register_action("PAUSE", "Player.PlayPause", NULL, NULL, 0, 1, 1, 0);
			register_action("PLAY", "Player.PlayPause", NULL, NULL, 0, 1, 1, 0);
			register_action("PREVIOUS", "Player.GoPrevious", NULL, NULL, 0, 1, 1, 0);
			register_action("REPEAT", "Player.Repeat", "\"state\":\"%s\"", repeat_args, repeat_args_size - 1, 1, 1, 1);
			register_action("SHUFFLE", "Player.Shuffle", NULL, NULL, 0, 1, 1, 0);
			register_action("STOP", "Player.Stop", NULL, NULL, 0, 1, 1, 0);
			register_action("UNSHUFFLE", "Player.UnShuffle", NULL, NULL, 0, 1, 1, 0);
			break;

		case XBMC_VERSION_FRODO:
			/* Player actions */
			register_action("MENU", "Input.ShowOSD", NULL, NULL, 0, 1, 0, 0);
			register_action("NEXT", "Player.GoTo", "\"to\":\"next\"", NULL, 0, 1, 1, 0);
			register_action("PAUSE", "Player.SetSpeed", "\"speed\":0", NULL, 0, 1, 1, 0);
			register_action("PLAY", "Player.SetSpeed", "\"speed\":1", NULL, 0, 1, 1, 0);
			register_action("PREVIOUS", "Player.GoTo", "\"to\":\"previous\"", NULL, 0, 1, 1, 0);
			register_action("REPEAT", "Player.SetRepeat", "\"repeat\":\"%s\"", repeat_args, repeat_args_size, 1, 1, 0);
			register_action("SHUFFLE", "Player.SetShuffle", "\"shuffle\":true", NULL, 0, 1, 1, 0);
			register_action("STOP", "Player.Stop", NULL, NULL, 0, 1, 1, 0);
			register_action("UNSHUFFLE", "Player.SetShuffle", "\"shuffle\":false", NULL, 0, 1, 1, 0);
			/* Window actions */
			register_action("MUSIC", "GUI.ActivateWindow", "\"window\":\"music\"", NULL, 0, 1, 0, 0);
			register_action("PICTURES", "GUI.ActivateWindow", "\"window\":\"pictures\"", NULL, 0, 1, 0, 0);
			register_action("PROGRAMS", "GUI.ActivateWindow", "\"window\":\"programs\"", NULL, 0, 1, 0, 0);
			register_action("SETTINGS", "GUI.ActivateWindow", "\"window\":\"settings\"", NULL, 0, 1, 0, 0);
			register_action("T_V", "GUI.ActivateWindow", "\"window\":\"tv\"", NULL, 0, 1, 0, 0);
			register_action("VIDEOS", "GUI.ActivateWindow", "\"window\":\"videos\"", NULL, 0, 1, 0, 0);
			register_action("WEATHER", "GUI.ActivateWindow", "\"window\":\"weather\"", NULL, 0, 1, 0, 0);
			/* Other actions */
			register_action("CONTEXT", "Input.ContextMenu", NULL, NULL, 0, 1, 0, 0);
			break;

	}

}

void
perform_actions(const char *hyp)
{

	int		i = 0;
	int		j = 0;
	int		k = 0;
	int		ls = 0;
	int		len;
	int		matched = 0;
	int		player_id;
	action_t*	action;
	action_t*	action_queued;
	action_t*	queue[MAX_ACTIONS];
	char*		action_string;
	char*		response = NULL;
	char*		argument_search;
	char*		params_fmt;
	const char*		param;
	int		expect_arg = 0;

	/* Get player ID via JSON-RPC */
	player_id = get_json_rpc_response_int("Player.GetActivePlayers", NULL, "playerid");

	/* Prepare action queue from words in hypothesis */
	do
	{
		if (*(hyp + i) == ' ' || *(hyp + i) == '\0')
		{

			/* Extract single word */
			len = i - ls + 1;
			action_string = malloc(len);
			memset(action_string, 0, len);
			memcpy(action_string, hyp + ls, len - 1);

			/* Reset loop iterator and end flag */
			k = 0;
			matched = 0;

			/* Check if we're not expecting an argument to last action */
			if (!expect_arg)
			{
				while (k < actions_count && !matched)
				{
					/* Process next action */
					action = actions[k];
					/* Check if action word matches spoken word */
					if (strcmp(action->word, action_string) == 0)
					{
						/* Ignore action if it requires a player ID and we don't have a player ID */
						if ( (action->needs_player_id && player_id != -1) || !action->needs_player_id )
						{
							/* Is this a repeating action? */
							if (action->repeats > 1)
							{
								/* Repeating action has to be preceded by a repeatable action */
								if (j > 0 && in_array(action->req, action->req_size, queue[j-1]->word))
									/* Set number of repeats for preceding action */
									queue[j-1]->repeats = action->repeats;
							}
							else
							{

								/* Insert a copy of action into queue */
								action_queued = malloc(sizeof(action_t));
								memcpy(action_queued, action, sizeof(action_t));
								action_queued->params = NULL;
								queue[j++] = action_queued;

								/* Fill player ID if action needs it */
								if (action->needs_player_id)
								{
									/* Player ID can be max 1 char */
									action_queued->params = malloc(strlen("\"playerid\":") + 2);
									sprintf(action_queued->params, "\"playerid\":%d", player_id);
								}

								/* Fill action params if needed */
								if (action->params)
								{
									if (action->req_size > 0)
										expect_arg = 1;
									else
										append_param(&action_queued->params, action->params);
								}

							}
						}
						else
						{
							print_log(LOG_WARNING, "Player action %s ignored as there is no active player", action_string);
						}
						/* Stop searching for a matching action */
						matched = 1;
					}
					k++;
				}
				if (k == actions_count && !matched)
					print_log(LOG_WARNING, "Unknown action \"%s\"", action_string);
			}
			else
			{

				argument_search = malloc(strlen(action_string) + 2);
				sprintf(argument_search, "%s:", action_string);

				/* Don't look for an action but rather for an argument to last action;
				   if the argument is optional, ignore the last entry in argument table 
			 	   (required => search until [size]; not required => search until [size - 1]) */
				while (k < action->req_size - (1 - action->needs_argument) && !matched)
				{
					/* If current word is a valid argument to last action... */
					if (strstr(action->req[k], argument_search))
					{
						/* Get param value for current word */
						param = strchr(action->req[k], ':') + 1;
						/* Generate formatted param string */
						params_fmt = malloc(strlen(action->params) + strlen(param) + 1);
						sprintf(params_fmt, action->params, param);
						/* Add formatted param string to last action's params */
						append_param(&queue[j-1]->params, params_fmt);
						/* Cleanup */
						free(params_fmt);
						/* Stop searching for an argument */
						matched = 1;
					}
					k++;
				}

				free(argument_search);

				/* If no valid argument was found, delete last action */
				if (!matched)
				{
					print_log(LOG_WARNING, "%s is not a valid argument for %s - interpreting as action", action_string, queue[j-1]->word);
					free(queue[j-1]->params);
					free(queue[j-1]);
					j--;
					/* Current word is probably an action - process it again */
					i -= strlen(action_string) + 1;
				}

				/* Don't expect an argument any more */
				expect_arg = 0;

			}

			free(action_string);
			ls = i + 1;

		}
	}
	while (*(hyp + i++) != '\0' && j < MAX_ACTIONS);

	/* Check if the last command accepts an argument which was not given */
	if (expect_arg)
	{
		/* If the command requires an argument, discard last action */
		if (action->needs_argument)
		{
			print_log(LOG_WARNING, "Action %s requires an argument, none given - ignoring action", action->word);
			free(queue[j-1]->params);
			free(queue[j-1]);
			j--;
		}
		else
		/* If the command also works without an argument, process it with the default argument */
		{
			/* Get param value for default action */
			param = action->req[action->req_size-1];
			/* Generate formatted param string */
			params_fmt = malloc(strlen(action->params) + strlen(param) + 1);
			sprintf(params_fmt, action->params, param);
			/* Add formatted param string to last action's params */
			append_param(&queue[j-1]->params, params_fmt);
			/* Cleanup */
			free(params_fmt);
		}
	}

	/* Execute all actions from queue */
	for (i=0; i<j; i++)
	{
		/* Repeat each action the desired number of times in 200ms intervals */
		for (k=0; k<queue[i]->repeats; k++)
		{
			send_json_rpc_request(queue[i]->method, queue[i]->params, NULL);
			usleep(200000);
		}
		free(queue[i]->params);
		free(queue[i]);
	}

	/* Cleanup */
	free(response);

}

void
register_cmap(const char *string, const int character)
{

	cmap_t *mapping = malloc(sizeof(cmap_t));
	mapping->string = malloc(strlen(string) + 1);
	strcpy(mapping->string, string);
	mapping->character = character;

	cmap = realloc(cmap, sizeof(cmap_t *) * (cmap_count + 1));
	cmap[cmap_count] = mapping;
	cmap_count++;

}

void
initialize_cmap(void)
{

	/* Letters */
	register_cmap("ALPHA",		'a');
	register_cmap("BRAVO",		'b');
	register_cmap("CHARLIE",	'c');
	register_cmap("DELTA",		'd');
	register_cmap("ECHO",		'e');
	register_cmap("FOXTROT",	'f');
	register_cmap("GOLF",		'g');
	register_cmap("HOTEL",		'h');
	register_cmap("INDIA",		'i');
	register_cmap("JULIET",		'j');
	register_cmap("KILO",		'k');
	register_cmap("LIMA",		'l');
	register_cmap("MIKE",		'm');
	register_cmap("NOVEMBER",	'n');
	register_cmap("OSCAR",		'o');
	register_cmap("PAPA",		'p');
	register_cmap("QUEBEC",		'q');
	register_cmap("ROMEO",		'r');
	register_cmap("SIERRA",		's');
	register_cmap("TANGO",		't');
	register_cmap("UNIFORM",	'u');
	register_cmap("VICTOR",		'v');
	register_cmap("WHISKEY",	'w');
	register_cmap("X_RAY",		'x');
	register_cmap("YANKEE",		'y');
	register_cmap("ZULU",		'z');

	/* Digits */
	register_cmap("ZERO",		'0');
	register_cmap("ONE",		'1');
	register_cmap("TWO",		'2');
	register_cmap("THREE",		'3');
	register_cmap("FOUR",		'4');
	register_cmap("FIVE",		'5');
	register_cmap("SIX",		'6');
	register_cmap("SEVEN",		'7');
	register_cmap("EIGHT",		'8');
	register_cmap("NINE",		'9');

	/* Punctuation */
	register_cmap("COLON",		':');
	register_cmap("COMMA",		',');
	register_cmap("DOT",		'.');
	register_cmap("HYPHEN",		'-');
	register_cmap("SPACE",		' ');

}

int
find_cmap(const char *string)
{

	int found = 0;
	int i = 0;
	int retval = -1;

	while(i < cmap_count && !found)
	{
		if (strcmp(cmap[i]->string, string) == 0)
		{
			retval = cmap[i]->character;
			found = 1;
		}
		i++;
	}

	return retval;

}

void
perform_spelling(const char *hyp)
{

	int i = 0;
	int j = strlen(spelling_buffer);
	int ls = -1;
	int character;
	char *command;

	do
	{
		if (*(hyp + i) == ' ' || *(hyp + i) == '\0')
		{

			/* If spelling buffer is full, end processing */
			if (j == SPELLING_BUFFER_SIZE - 1)
				break;

			/* Extract a single command */
			command = malloc(i - ls);
			memset(command, 0, i - ls);
			memcpy(command, hyp + ls + 1, i - ls - 1);

			/* DELETE command is treated separately as it doesn't add characters to the buffer */
			if (strcmp("DELETE", command) == 0)
			{
				if (j > 0)
				{
					spelling_buffer[j - 1] = '\0';
					j--;
				}
			}
			else if (strcmp("LOWER", command) == 0)
			{
				spelling_case = 0;
			}
			else if (strcmp("UPPER", command) == 0)
			{
				spelling_case = 1;
			}
			else
			{
				/* Try to find a character matching the command */
				character = find_cmap(command);
				if (character != -1)
					/* If the command is valid, append the character mapped to it to the buffer */
					spelling_buffer[j++] = spelling_case ? toupper(character) : character;
				else if (strlen(command) > 0)
					/* If the command is invalid, print out a warning */
					print_log(LOG_WARNING, "Unknown spelling mode command \"%s\"", command);
			}

			ls = i;
			free(command);

		}
	}
	while (*(hyp + i++) != '\0');

}

int
process_hypothesis(const char *hyp)
{

	char*	hyp_new = strdup(hyp);
	char*	next_word;
	char*	params;
	int	retval = 0;

	if (config_locking)
	{
		/* If we are locked and... */
		if (locked)
		{
			/* ...the first command heard is the unlock command, unlock and continue */
			if (strstr(hyp, COMMAND_UNLOCK) == hyp)
			{
				locked = 0;
				print_log(LOG_INFO, "xbmcvc is now unlocked");
				/* Check if there are further commands after the unlock command */
				next_word = strchr(hyp, ' ');
				if (next_word)
				{
					/* If yes, remove the unlock command from the hypothesis */
					free(hyp_new);
					hyp_new = strdup(next_word + 1);
				}
				else
				{
					/* If not, send GUI notification confirming unlocking */
					params = malloc(strlen("Current mode: %s") + strlen(modes[mode]));
					sprintf(params, "Current mode: %s", modes[mode]);
					send_gui_notification("Voice recognition enabled", params, "warning");
					print_log(LOG_INFO, "Current mode: %s", modes[mode]);
					free(params);
					free(hyp_new);
					hyp_new = strdup("");
				}
			}
			/* ...the first command heard is not the unlock command, warn and ignore all commands */
			else
			{
				print_log(LOG_WARNING, "xbmcvc is locked and not processing commands, say " COMMAND_UNLOCK " to unlock");
			}
		}
		/* If we are unlocked and the only command heard is the lock command, lock */
		else if (strcmp(COMMAND_LOCK, hyp_new) == 0)
		{
			locked = 1;
			send_gui_notification("Voice recognition disabled", "Not listening for commands", "warning");
			print_log(LOG_INFO, "xbmcvc is now locked");
		}
	}

	/* If we are unlocked or we don't care about locking... */
	if ((config_locking && !locked) || !config_locking)
	{
		/* Check for mode-changing keywords */
		switch(mode)
		{

			case MODE_NORMAL:
				/* Change to spelling mode */
				if (strcmp("SPELL", hyp_new) == 0)
				{
					if (xbmc_version >= XBMC_VERSION_FRODO)
					{
						memset(spelling_buffer, 0, SPELLING_BUFFER_SIZE);
						send_json_rpc_request("Input.SendText", "\"text\":\"\",\"done\":false", NULL);
						spelling_case = 0;
						mode = MODE_SPELLING;
						retval = 1;
						print_log(LOG_INFO, "Changed to spelling mode");
						send_gui_notification("Voice recognition mode changed", "Current mode: spelling", "warning");
					}
					else
					{
						print_log(LOG_ERR, "Spelling mode not available before Frodo");
					}
				}
				else if (strlen(hyp_new) > 0)
				{
					/* Send GUI notification with the commands heard */
					send_gui_notification("Voice command heard", hyp_new, "info");
					/* Perform requested actions */
					perform_actions(hyp_new);
				}
				break;

			case MODE_SPELLING:
				/* Return to normal mode, accepting input */
				if (strcmp("ACCEPT", hyp_new) == 0)
				{
					send_json_rpc_request("Input.ExecuteAction", "\"action\":\"enter\"", NULL);
					send_gui_notification("Voice recognition mode changed", "Current mode: normal", "warning");
					mode = MODE_NORMAL;
					retval = 1;
					print_log(LOG_INFO, "Changed to normal mode");
				}
				/* Return to normal mode, rejecting input */
				else if (strcmp("CANCEL", hyp_new) == 0)
				{
					send_json_rpc_request("Input.ExecuteAction", "\"action\":\"previousmenu\"", NULL);
					send_gui_notification("Voice recognition mode changed", "Current mode: normal", "warning");
					mode = MODE_NORMAL;
					retval = 1;
					print_log(LOG_INFO, "Changed to normal mode");
				}
				/* Clear input */
				else if (strcmp("CLEAR", hyp_new) == 0)
				{
					memset(spelling_buffer, 0, SPELLING_BUFFER_SIZE);
					send_json_rpc_request("Input.SendText", "\"text\":\"\",\"done\":false", NULL);
				}
				/* Return to normal mode */
				else if (strcmp("NORMAL", hyp_new) == 0)
				{
					/* Send GUI notification and change mode */
					send_gui_notification("Voice recognition mode changed", "Current mode: normal", "warning");
					mode = MODE_NORMAL;
					retval = 1;
					print_log(LOG_INFO, "Changed to normal mode");
				}
				else
				{
					perform_spelling(hyp_new);
					params = malloc(strlen("\"text\":\"%s\",\"done\":false") + strlen(spelling_buffer));
					sprintf(params, "\"text\":\"%s\",\"done\":false", spelling_buffer);
					send_json_rpc_request("Input.SendText", params, NULL);
					free(params);
				}
				break;

		}
	}

	free(hyp_new);

	return retval;

}

int
main(int argc, char *argv[])
{

	int		i;
	char*		dict;
	char		hyp_test[255];
	cmd_ln_t*	config;
	ps_decoder_t*	ps;
	ad_rec_t*	ad;
	cont_ad_t*	cont;
	int16		adbuf[4096];
	int32		k;
	int32		timestamp;
	int32		result;
	const char*	hyp;

	/* Register a memory-freeing routine to run upon exiting */
	atexit(cleanup);

	/* Parse command line options */
	parse_options(argc, argv);

	/* Check if language model files were properly installed */
	if (access(MODEL_HMM, R_OK) == -1)
		die("Hidden Markov acoustic model not found at %s. Please check your Pocketsphinx installation.", MODEL_HMM);

	if (access(MODEL_LM, R_OK) == -1)
		die("xbmcvc language model not found at %s. Please check your Pocketsphinx installation.", MODEL_LM);

	for (i=0; i<MODE_NONE; i++)
	{
		dict = malloc(strlen(MODELDIR "/lm/en/xbmcvc/.dic") + strlen(modes[i]) + 1);
		sprintf(dict, MODELDIR "/lm/en/xbmcvc/%s.dic", modes[i]);
		if (access(dict, R_OK) == -1)
			die("xbmcvc dictionary %s not found. Please check your xbmcvc installation.", dict);
		free(dict);
	}

	print_log(LOG_INFO, "Initializing, please wait...");

	/* Check XBMC version */
	xbmc_version = get_json_rpc_response_int("Application.GetProperties", "\"properties\":[\"version\"]", "major");

	if (xbmc_version == -2)
		die("Unable to connect to XBMC running at %s:%s - aborting", config_json_rpc_host, config_json_rpc_port);
	else if (xbmc_version == -1)
		die("Unable to determine XBMC version running at %s:%s - aborting", config_json_rpc_host, config_json_rpc_port);
	else if (xbmc_version < XBMC_VERSION_MIN || xbmc_version > XBMC_VERSION_MAX)
		die("XBMC version %d, which is running at %s:%s, is unsupported - aborting", xbmc_version, config_json_rpc_host, config_json_rpc_port);

	/* Setup action database */
	initialize_actions();
	/* Setup command to character mapping database */
	initialize_cmap();

	if (config_test_mode)
	{
		print_log(LOG_INFO, "Test mode enabled - enter space-separated commands in ALL CAPS. Enter blank line to end.");
		for (;;)
		{
			if (fgets(hyp_test, 255, stdin) == NULL || hyp_test[0] == '\n')
			{
				break;
			}
			else
			{
				/* Trim newline from hypothesis */
				*(hyp_test + strlen(hyp_test) - 1) = '\0';
				/* Process hypothesis */
				process_hypothesis(hyp_test);
			}
		}
	}
	else
	{

		/* Suppress verbose messages from pocketsphinx */
		if (freopen("/dev/null", "w", stderr) == NULL)
			die("Failed to redirect stderr");

		/* Initialize pocketsphinx */
		config = cmd_ln_init(NULL, ps_args(), TRUE,
			"-hmm", MODEL_HMM,
			"-lm", MODEL_LM,
			"-dict", MODEL_DICT,
			NULL);
		if (config == NULL)
			die("Error creating pocketsphinx configuration");

		ps = ps_init(config);
		if (ps == NULL)
			die("Error initializing pocketsphinx");

		/* Open audio device for recording */
		if ((ad = ad_open_dev(config_alsa_device, 16000)) == NULL)
			die("Failed to open audio device");
		/* Initialize continous listening module */
		if ((cont = cont_ad_init(ad, ad_read)) == NULL)
			die("Failed to initialize voice activity detection");
		/* Start recording */
		if (ad_start_rec(ad) < 0)
			die("Failed to start recording");
		/* Calibrate voice detection */
		if (cont_ad_calib(cont) < 0)
			die("Failed to calibrate voice activity detection");

		/* Intercept SIGINT and SIGTERM for proper cleanup */
		signal(SIGINT, set_exit_flag);
		signal(SIGTERM, set_exit_flag);

		print_log(LOG_INFO, "Ready for listening!");

		/* Main listening loop */
		for (;;)
		{

			/* Wait until we get any samples */
			while ((k = cont_ad_read(cont, adbuf, 4096)) == 0)
			{
				if (usleep(100000) == -1)
					break;
			}

			/* Exit main loop if we were interrupted */
			if (exit_flag)
				break;

			if (k < 0)
				die("Failed to read audio");

			/* Start collecting utterance data */
			if (ps_start_utt(ps, NULL) < 0)
				die("Failed to start utterance");

			if ((result = ps_process_raw(ps, adbuf, k, FALSE, FALSE)) < 0)
				die("Failed to process utterance data");

			/* Save timestamp for initial utterance samples */
			timestamp = cont->read_ts;

			/* Read the rest of utterance */
			for (;;)
			{

				if ((k = cont_ad_read(cont, adbuf, 4096)) < 0)
					die("Failed to read audio");

				if (k == 0)
				{
					/* Has it been 500ms since we last read any samples? */
					if ((cont->read_ts - timestamp) > DEFAULT_SAMPLES_PER_SEC/8)
						/* YES - Break the listening loop */
						break;
					else
						/* NO - Wait a bit before reading further data */
						if (usleep(20000) == -1)
							break;
				}
				else
				{
					/* New samples received - update timestamp */
					timestamp = cont->read_ts;
					/* Process the samples received */
					result = ps_process_raw(ps, adbuf, k, FALSE, FALSE);
				}

			}

			/* Stop listening */
			ad_stop_rec(ad);
			/* Flush any samples remaining in buffer - they will not be processed */
			while (ad_read(ad, adbuf, 4096) >= 0);
			/* Reset continous listening module */
			cont_ad_reset(cont);
			/* End utterance */
			ps_end_utt(ps);

			/* Exit main loop if we were interrupted */
			if (exit_flag)
				break;

			/* Get hypothesis for utterance */
			hyp = ps_get_hyp(ps, NULL, NULL);
			/* Print hypothesis */
			print_log(LOG_INFO, "Heard: \"%s\"", hyp);
			/* Process hypothesis */
			if (process_hypothesis(hyp) == 1)
			{
				/* If process_hypothesis() returns 1, mode of operation has changed - load a proper dictionary */
				dict = malloc(strlen(MODELDIR "/lm/en/xbmcvc/.dic") + strlen(modes[mode]) + 1);
				sprintf(dict, MODELDIR "/lm/en/xbmcvc/%s.dic", modes[mode]);
				ps_load_dict(ps, dict, NULL, NULL);
				free(dict);
			}

			/* Resume recording */
			if (ad_start_rec(ad) < 0)
				die("Failed to start recording");

		}

		/* Cleanup */
		cont_ad_close(cont);
		ad_close(ad);
		ps_free(ps);

	}

	return 0;

}

