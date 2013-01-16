/*
 *
 * xbmcvc - a program for controlling XBMC with simple voice commands
 *
 * Copyright (C) Michal Kepien, 2012.
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
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Constants */
#define VERSION				"0.2"
#define USAGE_MESSAGE			"\n" \
					"Usage: xbmcvc [ -H host ] [ -P port ] [ -V ] [ -t ] [ -h ]\n" \
					"\n" \
					"    -H hostname  Hostname or IP address of the XBMC instance you want to control (default: localhost)\n" \
					"    -P port      Port number the XBMC instance you want to control is listening on (default: 8080)\n" \
					"    -D device    Name of ALSA device to capture speech from\n" \
					"    -V           Print version information and exit\n" \
					"    -t           Enable test mode - enter commands on stdin\n" \
					"    -h           Print this help message\n" \
					"\n"

#define MAX_ACTIONS			5
#define JSON_RPC_DEFAULT_HOST		"localhost"
#define JSON_RPC_DEFAULT_PORT		8080
#define JSON_RPC_URL			"http://%s:%s/jsonrpc"
#define JSON_RPC_POST			"{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"id\":1}"
#define JSON_RPC_POST_WITH_PARAMS	"{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":{%s},\"id\":1}"

/* Language model files */
#define MODEL_HMM			MODELDIR "/hmm/en_US/hub4wsj_sc_8k"
#define MODEL_LM			MODELDIR "/lm/en/xbmcvc.lm"
#define MODEL_DICT			MODELDIR "/lm/en/xbmcvc.dic"

/* Macros */
#define ARRAY_SIZE(array)		(sizeof(array) / sizeof(array[0]))
#define DIE(message)			{ printf("Fatal error at %s:%d: %s\n", __FILE__, __LINE__, message); exit(1); }

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
	int		dynamic_params;
} action_t;

/* Global configuration variables */
char*		config_json_rpc_host;
char*		config_json_rpc_port;
char*		config_alsa_device;
int		config_test_mode = 0;

/* Action database */
action_t**	actions = NULL;
int		actions_count = 0;
const char*	repeatable[] = { "DOWN", "LEFT", "NEXT", "PREVIOUS", "RIGHT", "UP" };
int		repeatable_size = ARRAY_SIZE(repeatable);
const char*	repeat_args[] = { "ALL:all", "ONE:one", "OFF:off" };
int		repeat_args_size = ARRAY_SIZE(repeat_args);
const char*	volume_args[] = { "TEN:10", "TWENTY:20", "THIRTY:30", "FOURTY:40", "FIFTY:50", "SIXTY:60", "SEVENTY:70", "EIGHTY:80", "NINETY:90", "MAX:100" };
int		volume_args_size = ARRAY_SIZE(volume_args);

/* Exit flag */
volatile int	exit_flag = 0;

/*---------------------------------------------------------------------------*/

int
in_array(const char* haystack[], int haystack_size, char *needle)
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

/* CURL callback for saving HTTP response to a pointer passed via userdata */
size_t
save_response_in_memory(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	curl_userdata_t *cud = (curl_userdata_t *) userdata;
	*cud->dst = realloc(*cud->dst, cud->dst_s + (size * nmemb) + 1);
	memcpy(*cud->dst + cud->dst_s, ptr, size * nmemb);
	cud->dst_s += size * nmemb;
	/* Null-terminate response for easier handling */
	*(*cud->dst + cud->dst_s) = '\0';
}

void
send_json_rpc_request(const char *method, const char *params, char **dst)
{

	CURL*		curl;
	CURLcode	result;
	char*		url;
	char*		post;
	char*		response = NULL;
	curl_userdata_t	cud;

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
		DIE("Error initializing libcurl\n");

	/* Set request options */
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, save_response_in_memory);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &cud);

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
	curl_easy_cleanup(curl);

}

void
append_param(char **current, char *append)
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


void
perform_actions(const char *hyp)
{

	int		i = 0;
	int		j = 0;
	int		k = 0;
	int		ls = 0;
	int		len;
	int		matched = 0;
	action_t*	action;
	action_t*	action_queued;
	action_t*	queue[MAX_ACTIONS];
	char*		action_string;
	char*		response = NULL;
	char*		player_id_offset = NULL;
	char*		player_id = NULL;
	char*		params_fmt;
	char*		param;
	int		expect_arg = 0;

	/* Get player ID via JSON-RPC */
	send_json_rpc_request("Player.GetActivePlayers", NULL, &response);

	/* XBMC did not respond - its webserver is probably not enabled */
	if (!response)
	{
		printf("ERROR: JSON-RPC query not answered - is XBMC's webserver turned on?\n");
		return;
	}

	player_id_offset = strstr(response, "\"playerid\":");
	if (player_id_offset)
	{
		/* Put response content after '"playerid":' in player_id */
		player_id = strdup(player_id_offset + 11);
		/* Increment iterator until a non-digit character is found in player_id */
		while (*(player_id + i) >= '0' && *(player_id + i) <= '9')
			i++;
		/* Insert null byte after last digit to get a string containing player ID */
		*(player_id + i) = '\0';
	}

	/* Reset iterator */
	i = 0;

	/* Prepare action queue from words in hypothesis */
	do { 
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
						if ( (action->needs_player_id && player_id) || !action->needs_player_id )
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
									params_fmt = strdup("\"playerid\":%s");
									action_queued->params = malloc(strlen(params_fmt) + strlen(player_id));
									sprintf(action_queued->params, params_fmt, player_id);
									free(params_fmt);
								}

								/* Fill action params if needed */
								if (action->params)
								{
									if (action->dynamic_params)
										expect_arg = 1;
									else
										append_param(&action_queued->params, action->params);
								}

							}
						}
						else
						{
							printf("WARNING: Player action %s ignored as there is no active player\n", action_string);
						}
						/* Stop searching for a matching action */
						matched = 1;
					}
					k++;
				}
				if (k == actions_count && !matched)
					printf("WARNING: Unknown action %s\n", action_string);
			}
			else
			{

				/* Don't look for an action but rather for an argument to last action */
				while (k < queue[j-1]->req_size && !matched)
				{
					/* If current word is a valid argument to last action... */
					if (strstr(queue[j-1]->req[k], action_string))
					{
						/* Get param value for current word */
						param = strchr(queue[j-1]->req[k], ':') + 1;
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

				/* If no valid argument was found, delete last action */
				if (!matched)
				{
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
	} while (*(hyp + i++) != '\0' && j < MAX_ACTIONS);

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
	free(player_id);
	free(response);

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
	while ((option = getopt(argc, argv, "H:P:D:Vth")) != -1 && !quit)
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

			case 'V':
				printf("xbmcvc " VERSION " (Git: " GITVERSION ")\n");
				quit = 1;
				break;

			/* Test mode */
			case 't':
				config_test_mode = 1;
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
	{
		free(config_json_rpc_host);
		free(config_json_rpc_port);
		free(config_alsa_device);
		exit(0);
	}

}

void
set_exit_flag(int signal)
{
	exit_flag = 1;
}

void
register_action(char* word, char* method, char* params, const char* req[], int req_size, int repeats, int needs_player_id, int dynamic_params)
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
	a->dynamic_params = dynamic_params;

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

	/* Player actions */
	register_action("NEXT", "Player.GoNext", NULL, NULL, 0, 1, 1, 0);
	register_action("PAUSE", "Player.PlayPause", NULL, NULL, 0, 1, 1, 0);
	register_action("PLAY", "Player.PlayPause", NULL, NULL, 0, 1, 1, 0);
	register_action("PREVIOUS", "Player.GoPrevious", NULL, NULL, 0, 1, 1, 0);
	register_action("REPEAT", "Player.Repeat", "\"state\":\"%s\"", repeat_args, repeat_args_size, 1, 1, 1);
	register_action("SHUFFLE", "Player.Shuffle", NULL, NULL, 0, 1, 1, 0);
	register_action("STOP", "Player.Stop", NULL, NULL, 0, 1, 1, 0);
	register_action("UNSHUFFLE", "Player.UnShuffle", NULL, NULL, 0, 1, 1, 0);

	/* Repeating actions */
	register_action("TWO", NULL, NULL, repeatable, repeatable_size, 2, 0, 0);
	register_action("THREE", NULL, NULL, repeatable, repeatable_size, 3, 0, 0);
	register_action("FOUR", NULL, NULL, repeatable, repeatable_size, 4, 0, 0);
	register_action("FIVE", NULL, NULL, repeatable, repeatable_size, 5, 0, 0);

}

void
cleanup_actions()
{
	int i;
	for (i=0; i<actions_count; i++)
	{
		free(actions[i]->word);
		free(actions[i]->method);
		free(actions[i]->params);
		free(actions[i]->req);
		free(actions[i]);
	}
	free(actions);
}

int
main(int argc, char *argv[])
{

	int		n;
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

	parse_options(argc, argv);

	printf("Initializing, please wait...\n");

	/* Check if language model files were properly installed */
	if (access(MODEL_HMM, R_OK) == -1)
	{
		printf("Hidden Markov acoustic model not found at %s. Please check your Pocketsphinx installation.\n", MODEL_HMM);
		exit(1);
	}

	if (access(MODEL_LM, R_OK) == -1 || access(MODEL_DICT, R_OK) == -1)
	{
		printf("xbmcvc language model not found. Please check if the following files are installed and readable:\n");
		printf("  %s\n  %s\n", MODEL_LM, MODEL_DICT);
		exit(1);
	}

	/* Setup action database */
	initialize_actions();

	if (config_test_mode)
	{
		printf("Test mode enabled - enter space-separated commands in ALL CAPS. Enter blank line to end.\n");
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
				perform_actions(hyp_test);
			}
		}
	}
	else
	{

		/* Suppress verbose messages from pocketsphinx */
		if (freopen("/dev/null", "w", stderr) == NULL)
			DIE("Failed to redirect stderr");

		/* Initialize pocketsphinx */
		config = cmd_ln_init(NULL, ps_args(), TRUE,
			"-hmm", MODEL_HMM,
			"-lm", MODEL_LM,
			"-dict", MODEL_DICT,
			NULL);
		if (config == NULL)
			DIE("Error creating pocketsphinx configuration");

		ps = ps_init(config);
		if (ps == NULL)
			DIE("Error initializing pocketsphinx");

		/* Open audio device for recording */
		if ((ad = ad_open_dev(config_alsa_device, 16000)) == NULL)
			DIE("Failed to open audio device");
		/* Initialize continous listening module */
		if ((cont = cont_ad_init(ad, ad_read)) == NULL)
			DIE("Failed to initialize voice activity detection\n");
		/* Start recording */
		if (ad_start_rec(ad) < 0)
			DIE("Failed to start recording\n");
		/* Calibrate voice detection */
		if (cont_ad_calib(cont) < 0)
			DIE("Failed to calibrate voice activity detection\n");

		/* Intercept SIGINT and SIGTERM for proper cleanup */
		signal(SIGINT, set_exit_flag);
		signal(SIGTERM, set_exit_flag);

		printf("Ready for listening!\n");

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
				DIE("Failed to read audio\n");

			/* Start collecting utterance data */
			if (ps_start_utt(ps, NULL) < 0)
				DIE("Failed to start utterance\n");

			if ((result = ps_process_raw(ps, adbuf, k, FALSE, FALSE)) < 0)
				DIE("Failed to process utterance data\n");

			/* Save timestamp for initial utterance samples */
			timestamp = cont->read_ts;

			/* Read the rest of utterance */
			for (;;)
			{

				if ((k = cont_ad_read(cont, adbuf, 4096)) < 0)
					DIE("Failed to read audio\n");

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
			printf("Heard: \"%s\"\n", hyp);
			/* Perform requested actions */
			perform_actions(hyp);

			/* Resume recording */
			if (ad_start_rec(ad) < 0)
				DIE("Failed to start recording\n");

		}

		/* Cleanup */
		cont_ad_close(cont);
		ad_close(ad);
		ps_free(ps);

	}

	cleanup_actions();

	free(config_json_rpc_host);
	free(config_json_rpc_port);
	free(config_alsa_device);

	return 0;

}

