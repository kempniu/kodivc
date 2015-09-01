__PREFACE:__ This piece of software used to be called _xbmcvc_, because the software it was controlling was called XBMC back in the days. However, [XBMC was renamed to Kodi](http://kodi.tv/introducing-kodi-14/) in August 2014 and thus I decided to change the name of my program to _kodivc_, in an effort to steer away from legacy naming. In this document, unless explicitly stated otherwise, the names Kodi and XBMC (and their derivatives) can be used interchangeably. I am nevertheless sticking with Kodi to keep things up-to-date.

kodivc
======

_kodivc_ is a program for controlling [Kodi](http://kodi.tv/) with simple voice commands. It uses CMU Sphinx for speech recognition.

Requirements
------------

To use _kodivc_, the following libraries need to be installed on your system:

* _pocketsphinx_ (version 0.6 or newer) along with its prerequisite, _sphinxbase_ (both available at [CMU Sphinx downloads](http://cmusphinx.sourceforge.net/wiki/download/))
* _libcurl_ (shipped with cURL, should be present on most systems; if that's not your case, go to the [libcurl homepage](http://curl.haxx.se/libcurl/))

For _kodivc_ to work, you need a Kodi version which supports JSON-RPC API version 3 or higher, which means any pre-11.0 (pre-Eden) or newer version should work. Older releases (including Dharma) are not supported.

Installation
------------

### Using PPA ###

__NOTE:__ This installation method is only available for KodiBuntu, XBMCbuntu Gotham and Ubuntu versions 14.04 (Trusty Tahr) and newer.

You can install the latest version of _kodivc_ by accessing your Kodi box:

* using a local console (press _CTRL+ALT+F1_ to switch from graphical mode to text mode; press _CTRL+ALT+F7_ to return to graphical mode)

* remotely, using [SSH](http://kodi.wiki/view/Ssh)

and issuing the following commands:

    sudo add-apt-repository -y universe
    sudo add-apt-repository -y ppa:kempniu/kodivc
    sudo apt-get update
    sudo apt-get -y install kodivc

__NOTE:__ Official _pocketsphinx_ packages for Ubuntu 14.04 and newer are compiled with PulseAudio support enabled and ALSA support disabled. However, neither KodiBuntu nor XBMCbuntu use PulseAudio by default. To test _kodivc_ with KodiBuntu or XBMCbuntu Gotham, you have to install and start PulseAudio. Here is a quick-and-dirty way to do it (for your own good, __please don't do this on a system installed on a hard disk__):

    sudo rm -f /etc/apt/preferences.d/{libasound2-plugins,pulseaudio}.pref
    sudo apt-get -y install pulseaudio
    # for KodiBuntu
    DISPLAY=:0 sudo -u kodi pulseaudio &
    # for XBMCbuntu Gotham
    DISPLAY=:0 sudo -u xbmc pulseaudio &

If you install KodiBuntu / XBMCbuntu Gotham on a hard disk, you'll have to properly configure PulseAudio and then configure Kodi to use PulseAudio as its sound sink (this is outside the scope of this document, but the Kodi wiki has [some useful information](http://kodi.wiki/view/PulseAudio)).

### Using Git ###

    git clone https://github.com/kempniu/kodivc
    cd kodivc
    make
    make install

__NOTE:__ the user running _kodivc_ should be allowed to access your sound card. On the Gentoo distribution, for instance, this is achieved by adding the user to the _audio_ group.

### Configuring Kodi ###

_kodivc_ uses JSON-RPC via HTTP for passing commands to Kodi. In order for this to work, you need to go to the proper settings page in Kodi and turn on _Allow remote control via HTTP_ (_Allow control of Kodi via HTTP_ in Helix, _Allow control of XBMC via HTTP_ in Gotham and older versions):

* in Eden, you'll find it under _System_ -> _Settings_ -> _Network_ -> _Services_
* in Frodo and newer versions, you'll find it under _System_ -> _Settings_ -> _Services_ -> _Webserver_

If you want to control your Kodi instance from another machine, make sure you also turn on _Allow remote control by programs on other systems_ (_Allow programs on other systems to control Kodi_ in Helix, _Allow programs on other systems to control XBMC_ in Gotham and older versions):

* in Eden, you'll find it under _System_ -> _Settings_ -> _Network_ -> _Services_
* in Frodo and newer versions, you'll find it under _System_ -> _Settings_ -> _Services_ -> _Remote control_

__NOTE:__ for the above settings to be available in Gotham and newer versions, the current settings level has to be at least _Standard_.

Usage
-----

After a successful installation, you should be able to run _kodivc_ by executing:

    kodivc

At startup, _kodivc_ will initialize the speech recognition library and, after successfully self-calibrating to properly tell silence and speech apart, it will start listening to your commands - if everything went fine, you should get the _Ready for listening!_ message. From the moment you get that message just speak to the microphone (see below for valid commands) - that is all it takes to use the program.

If you are trying to control a Kodi instance installed on a different machine than the one you're running _kodivc_ on (or your Kodi listens on a non-standard port), pass the correct hostname and port number to _kodivc_ via the __-H__ and __-P__ command line switches, respectively. If you have set a username and a password for JSON-RPC in Kodi, pass them to _kodivc_ via the __-u__ and __-p__ command line switches, respectively.

By default, _kodivc_ will try to capture speech from the _default_ audio device. If that doesn't work for you, you can specify the audio device you want to capture speech from using the __-D__ command line switch.

By default, though only when controlling Kodi version 12 (Frodo) or newer, _kodivc_ will display GUI notifications when it hears commands or changes its mode of operation. This behavior can be disabled by using the __-n__ command line switch.

Please consult the usage message (run _kodivc_ with the __-h__ switch to view it) for an explanation of other command line switches.

Reading further, you'll come across the term "batch". _kodivc_ listens to commands in batches. A batch starts when you start speaking and ends once a long enough period of silence has been detected. Every batch is reported on the command line, with a line like:

    Heard: "COMMAND1 COMMAND2 ... COMMANDn"

### Running in daemon mode ###

_kodivc_ can also run in the background (in so called daemon mode) so that you don't have to have a terminal open to use it. To enable daemon mode, run _kodivc_ with the __-d__ command line switch. Note that when enabling the daemon mode, you'll almost certainly want to enable logging (using the __-L__ command line switch) to a file or to syslog (check the usage message for details) to be able to read the messages output by _kodivc_. To cleanly shutdown the daemon, send a SIGINT signal to it. Another command line option that comes in handy when using daemon mode is the __-r__ option which enables you to specify a file in which _kodivc_ will save its PID after starting.

### Locking ###

By default, _kodivc_ locks itself after initializing to prevent accidental usage. Say _"KODI"_ to unlock. This has to be the first command in a batch in order to work. Whatever you say afterwards will be executed immediately after unlocking. To lock _kodivc_, say _"OKAY"_. The locking/unlocking feature can be disabled using the __-l__ command line switch.

### Normal mode ###

_kodivc_ always starts in normal mode. This is the mode you'll probably use the most. Valid commands are:

#### Navigation commands ####

* _UPWARDS_
* _DOWNWARDS_
* _LEFT_
* _RIGHT_
* _SELECT_
* _BACK_
* _HOME_
* _CONTEXT_*
* _WEATHER_*
* _PICTURES_*
* _TV_*
* _VIDEOS_*
* _MUSIC_*
* _PROGRAMS_*
* _SETTINGS_*
* _FAVORITES_*

__NOTE:__ Commands marked with an asterisk (*) are only available in Kodi 12 (Frodo) onwards.

#### Player commands ####

* _PLAY_
* _PAUSE_
* _STOP_
* _MENU_*
* _PREVIOUS_
* _NEXT_
* _SHUFFLE_
* _UNSHUFFLE_
* _REPEAT_ [ _ALL_ | _ONE_ | _OFF_ ]

__NOTE:__ Commands marked with an asterisk (*) are only available in Kodi 12 (Frodo) onwards.

__NOTE:__ In Kodi 12 (Frodo) onwards you can also say REPEAT without an argument to cycle through available modes.

#### Volume commands ####

* _VOLUME_ [ _TEN_ | _TWENTY_ | _THIRTY_ | _FORTY_ | _FIFTY_ | _SIXTY_ | _SEVENTY_ | _EIGHTY_ | _NINETY_ | _MAX_ ]
* _MUTE_
* _UNMUTE_

#### Command repetition ####

Some commands can be executed multiple times by using one of the following multiplier commands:

* _TWO_
* _THREE_
* _FOUR_
* _FIVE_

Below is an exhaustive list of commands which can be used with a multiplier:

* _UPWARDS_
* _DOWNWARDS_
* _LEFT_
* _RIGHT_
* _NEXT_
* _PREVIOUS_

To use a multiplier, say it after the command, e.g. _"NEXT FOUR"_ will skip four items ahead, _"DOWNWARDS THREE RIGHT FIVE"_ will go down three times and then right five times etc.

### Spelling mode (Frodo onwards only) ###

You can use the spelling mode to input letters and digits, e.g. when performing a search, renaming a movie/album etc. - generally when Kodi displays the onscreen keyboard. Please note, though, that _kodivc_ will __not__ switch to the spelling mode automatically. To enable the spelling mode, say _"SPELL"_ while in normal mode. Switching back to normal mode is possible using three different commands:

* _ACCEPT_ - closes the onscreen keyboard, accepting the provided input (similar to pressing the ENTER key)
* _CANCEL_ - closes the onscreen keyboard, dismissing the provided input (similar to pressing the ESC key)
* _NORMAL_ - switches to normal mode without closing the onscreen keyboard; this is useful if you want to input special characters which don't have a _kodivc_ command counterpart

__NOTE:__ Every mode switching command has to be the **only** command in a batch in order to work.

__NOTE:__ The input field is automatically cleared whenever you enter the spelling mode. (Remember that when you initially spell some letters, then switch to normal mode to enter some fancy characters and then try to switch back to spelling mode.)

#### Letters ####

_kodivc_ uses the [NATO phonetic alphabet](http://en.wikipedia.org/wiki/NATO_phonetic_alphabet) for spelling. The reason for that is that single letter recognition is very inaccurate, which shouldn't come as much of a surprise (how often do you spell your surname over the phone and people at the other end get it wrong?). NATO phonetic alphabet usage enables _pocketsphinx_ to recognize the desired letters with much better accuracy. Here is a complete list of letter-related commands:

* _ALPHA_
* _BRAVO_
* _CHARLIE_
* _DELTA_
* _ECHO_
* _FOXTROT_
* _GOLF_
* _HOTEL_
* _INDIA_
* _JULIET_
* _KILO_
* _LIMA_
* _MIKE_
* _NOVEMBER_
* _OSCAR_
* _PAPA_
* _QUEBEC_
* _ROMEO_
* _SIERRA_
* _TANGO_
* _UNIFORM_
* _VICTOR_
* _WHISKEY_
* _X-RAY_
* _YANKEE_
* _ZULU_

#### Digits ####

* _ZERO_
* _ONE_
* _TWO_
* _THREE_
* _FOUR_
* _FIVE_
* _SIX_
* _SEVEN_
* _EIGHT_
* _NINE_

#### Other characters ####

* _COLON_
* _COMMA_
* _DOT_
* _HYPHEN_
* _SPACE_

#### Special commands ####

* _CLEAR_ - clears the input field; this has to be the __only__ command in a batch in order to work
* _LOWER_ - switch to lower case input
* _UPPER_ - switch to upper case input

Troubleshooting
---------------

*   If you get the _Ready for listening!_ message but nothing you say to the microphone results in a _Heard: "COMMAND"_ type of line appearing, try the test mode: start _kodivc_ with the __-t__ command line switch, then enter space-separated commands in ALL CAPS, ending a batch by pressing ENTER. Example:

        LEFT TWO SELECT<ENTER>

    You'll probably also want to disable locking for testing, so add the __-l__ switch to the command line as well.

    If your Kodi instance properly responds to commands entered in test mode, but doesn't react to anything you say, these are the most common causes of problems:

    * capturing from the wrong device,
    * capture levels set too low, too high (yes, overly high sensitivity is also a problem) or muted,
    * speaking too quietly.

*   If after starting _kodivc_ you see a series of odd lines among the normal ones, like below:

        Initializing, please wait...
         256x13 256x13 256x13
         256x13 256x13 256x13
        Ready for listening!

    just ignore them. It's a known issue with _pocketsphinx_ 0.6. These lines shouldn't appear when using _pocketsphinx_ 0.7 or newer.

*   If after starting _kodivc_ you get a _Failed to open audio device_ message, you are probably facing one of two issues:
    * you're using a version of _pocketsphinx_ which uses the PulseAudio backend, but PulseAudio server isn't started,
    * the user you're running _kodivc_ as is not allowed to access the audio device you're trying to capture speech from; to confirm, try running _kodivc_ as root - if it works fine, you're facing a permissions-related issue.

*   If after starting _kodivc_ you get an error saying _Failed to calibrate voice activity detection_, please check your mixer levels for capturing audio.

Language support
----------------

Currently _kodivc_ only supports voice commands spoken in English. Additional languages may be added in the future.

Caveats
-------

In normal mode, _kodivc_ will perform a maximum of 5 actions at a time to avoid confusion. You can say more voice commands than that, but only the first five recognized commands will be executed.

Feedback
--------

I'm always happy to hear feedback. If you encounter a problem with _kodivc_ or you want to share an idea for a new feature, feel free to [create an issue](https://github.com/kempniu/kodivc/issues) at GitHub. Here are some tips for creating a useful bug report:

*   Be as exhaustive as possible.

*   If possible, describe the steps required to reproduce the bug.

*   Include version information for _kodivc_, _sphinxbase_ and _pocketsphinx_ in the report. In KodiBuntu / XBMCbuntu / Ubuntu you can use the following command to retrieve the relevant information:

        dpkg -s kodivc libsphinxbase1 libpocketsphinx1 | grep '\(Package\|Version\)'

    If you built _kodivc_ from Git, please include the output of the following command in the report:

        kodivc -V

*   If available (must be enabled explicitly using the __-L__ command line switch), attach the log file to the report. If you didn't enable logging but you can still get hold of the messages output by _kodivc_ to the console before the crash, please save them to a file and attach that file to the report.

*   If you experienced a segfault, please generate a backtrace and attach it to the report. _gdb_ is required to do that. In KodiBuntu / XBMCbuntu / Ubuntu you can install _gdb_ by running the following command:

        sudo apt-get -y install gdb

    _kodivc_ generates core dumps by default, so all that is required to generate a backtrace is running the following command after _kodivc_ crashes:

        gdb --batch --se kodivc --core core -ex 'bt full' > kodivc-backtrace.txt 2>&1

    The backtrace will be written to file _kodivc-backtrace.txt_. Ensure your core dump is named _core_ - it differs between systems and thus the core dump may be placed in a file like _core.12345_ or similar - just supply the proper core dump file name to _gdb_ using the __--core__ option. Please note that if the segfault happened while _kodivc_ was running in daemon mode, the core dump will be placed in the _/tmp_ directory.

If you don't have a GitHub account and you don't really want to create one, feel free to post in the [_kodivc_ thread at the Kodi forums](http://forum.kodi.tv/showthread.php?tid=123621) or [catch me on Twitter](http://twitter.com/kempniu).

