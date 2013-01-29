xbmcvc
======

_xbmcvc_ is a program for controlling [XBMC](http://xbmc.org/) with simple voice commands. It uses CMU Sphinx for speech recognition.

Requirements
------------

To use _xbmcvc_, the following libraries need to be installed on your system:

* _pocketsphinx_ along with its prerequisite, _sphinxbase_ (http://cmusphinx.sourceforge.net/wiki/download/); _xbmcvc_ was tested with versions 0.6+
* _libcurl_ (shipped with cURL, should be present on most systems; if that's not your case, go to the [libcurl homepage](http://curl.haxx.se/libcurl/))

For _xbmcvc_ to work, you need an XBMC version which supports JSON-RPC API version 3 or higher, which means any pre-11.0 (pre-Eden) or newer version should work. Older releases (including Dharma) are not supported.

Installation
------------

### XBMCbuntu / Ubuntu (from PPA) ###

You can install the latest version of _xbmcvc_ by [accessing your XBMC box via SSH](http://wiki.xbmc.org/index.php?title=SSH) and issuing the following commands:

    sudo add-apt-repository -y ppa:dhuggins/cmusphinx
    sudo add-apt-repository -y ppa:kempniu/xbmcvc
    sudo perl -pi -e 's/\w+ main$/lucid main/;' /etc/apt/sources.list.d/{dhuggins-cmusphinx,kempniu-xbmcvc}-*.list
    sudo apt-get update
    sudo apt-get -y install xbmcvc

The third line is required for now as there are no _pocketsphinx_ packages published for neither Ubuntu 11.10 (which Eden XBMCbuntu is based on) nor Ubuntu 12.04 LTS (which Frodo XBMCbuntu is based on), so you have to force those systems to use packages built for Ubuntu 10.04 LTS.

### Other systems (from Git) ###

    git clone https://github.com/kempniu/xbmcvc
    cd xbmcvc
    make
    make install

__NOTE:__ the user running _xbmcvc_ should be allowed to access your sound card. On the Gentoo distribution, for instance, this is achieved by adding the user to the _audio_ group.

### Configuring XBMC ###

_xbmcvc_ uses JSON-RPC via HTTP for passing commands to XBMC. In order for this to work, you need to go to the proper settings page in XBMC and turn _Allow control of XBMC via HTTP_ on:

* in Eden, you'll find it under _System_ -> _Settings_ -> _Network_ -> _Services_
* in Frodo, you'll find it under _System_ -> _Settings_ -> _Services_ -> _Webserver_

If you want to control your XBMC instance from another machine, make sure you also turn _Allow programs on other systems to control XBMC_ on:

* in Eden, you'll find it under _System_ -> _Settings_ -> _Network_ -> _Services_
* in Frodo, you'll find it under _System_ -> _Settings_ -> _Services_ -> _Remote control_

Usage
-----

After a successful installation, you should be able to run _xbmcvc_ by executing:

    xbmcvc

At startup, _xbmcvc_ will initialize the speech recognition library and, after successfully self-calibrating to properly tell silence and speech apart, it will start listening to your commands - if everything went fine, you should get the _Ready for listening!_ message. From the moment you get that message just speak to the microphone (see below for valid commands) - that is all it takes to use the program.

If you are trying to control an XBMC instance installed on a different machine than the one you're running _xbmcvc_ on (or your XBMC listens on a non-standard port), pass the correct hostname and port number to _xbmcvc_ via the __-H__ and __-P__ command line switches, respectively.

By default, _xbmcvc_ will try to capture speech from the _default_ ALSA device. If that doesn't work for you, you can specify the ALSA device you want to capture speech from using the __-D__ command line switch.

By default, though only when controlling XBMC version 12 (Frodo) or newer, _xbmcvc_ will display GUI notifications when it hears commands or changes its mode of operation. This behavior can be disabled by using the __-n__ command line switch.

Please consult the usage message (run _xbmcvc_ with the __-h__ switch to view it) for an explanation of other command line switches.

Reading further, you'll come across the term "batch". _xbmcvc_ listens to commands in batches. A batch starts when you start speaking and ends once a long enough period of silence has been detected. Every batch is reported on the command line, with a line like:

    Heard: "COMMAND1 COMMAND2 ... COMMANDn"

### Locking ###

By default, _xbmcvc_ locks itself after initializing to prevent accidental usage. Say _"XBMC"_ to unlock. This has to be the first command in a batch in order to work. Whatever you say afterwards will be executed immediately after unlocking. To lock _xbmcvc_, say _"OKAY"_. The locking/unlocking feature can be disabled using the __-l__ command line switch.

### Normal mode ###

_xbmcvc_ always starts in normal mode. This is the mode you'll probably use the most. Valid commands are:

#### Navigation commands ####

* _UP_
* _DOWN_
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

_NOTE: Commands marked with an asterisk (*) are only available in XBMC 12 (Frodo) onwards._

#### Player commands ####

* _PLAY_
* _PAUSE_
* _STOP_
* _PREVIOUS_
* _NEXT_
* _SHUFFLE_
* _UNSHUFFLE_
* _REPEAT_ [_ALL_/_ONE_/_OFF_]

_NOTE: In XBMC 12 (Frodo) onwards you can also say REPEAT without an argument to cycle through available modes._

#### Volume commands ####

* _VOLUME_ [_TEN_/_TWENTY_/_THIRTY_/_FORTY_/_FIFTY_/_SIXTY_/_SEVENTY_/_EIGHTY_/_NINETY_/_MAX_]
* _MUTE_
* _UNMUTE_

#### Command repetition ####

Some commands can be executed multiple times by using one of the following multiplier commands:

* _TWO_
* _THREE_
* _FOUR_
* _FIVE_

Below is an exhaustive list of commands which can be used with a multiplier:

* _UP_
* _DOWN_
* _LEFT_
* _RIGHT_
* _NEXT_
* _PREVIOUS_

To use a multiplier, say it after the command, e.g. _"NEXT FOUR"_ will skip four items ahead, _"DOWN THREE RIGHT FIVE"_ will go down three times and then right five times etc.

### Spelling mode (Frodo onwards only) ###

You can use the spelling mode to input letters and digits, e.g. when performing a search, renaming a movie/album etc. - generally when XBMC displays the onscreen keyboard. Please note, though, that _xbmcvc_ will __not__ switch to the spelling mode automatically. To enable the spelling mode, say _"SPELL"_ while in normal mode. Switching back to normal mode is possible using three different commands:

* _ACCEPT_ - closes the onscreen keyboard, accepting the provided input (similar to pressing the ENTER key)
* _CANCEL_ - closes the onscreen keyboard, dismissing the provided input (similar to pressing the ESC key)
* _NORMAL_ - switches to normal mode without closing the onscreen keyboard; this is useful if you want to input special characters which don't have an _xbmcvc_ command counterpart

_NOTE: Every mode switching command has to be the __only__ command in a batch in order to work._

_NOTE: The input field is automatically cleared whenever you enter the spelling mode._ (Remember that when you initially spell some letters, then switch to normal mode to enter some fancy characters and then try to switch back to spelling mode.)

#### Letters ####

_xbmcvc_ uses the [NATO phonetic alphabet](http://en.wikipedia.org/wiki/NATO_phonetic_alphabet) for spelling. The reason for that is that single letter recognition is very inaccurate, which shouldn't come as much of a surprise (how often do you spell your surname over the phone and people at the other end get it wrong?). NATO phonetic alphabet usage enables _pocketsphinx_ to recognize the desired letters with much better accuracy. Here is a complete list of letter-related commands:

* ___A__LPHA_
* ___B__RAVO_
* ___C__HARLIE_
* ___D__ELTA_
* ___E__CHO_
* ___F__OXTROT_
* ___G__OLF_
* ___H__OTEL_
* ___I__NDIA_
* ___J__ULIET_
* ___K__ILO_
* ___L__IMA_
* ___M__IKE_
* ___N__OVEMBER_
* ___O__SCAR_
* ___P__APA_
* ___Q__UEBEC_
* ___R__OMEO_
* ___S__IERRA_
* ___T__ANGO_
* ___U__NIFORM_
* ___V__ICTOR_
* ___W__HISKEY_
* ___X__-RAY_
* ___Y__ANKEE_
* ___Z__ULU_
 
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

* _CLEAR_ - clears the input buffer; this has to be the __only__ command in a batch in order to work
* _LOWER_ - switch to lower case input
* _UPPER_ - switch to upper case input

Troubleshooting
---------------

*   If you get the _Ready for listening!_ message but nothing you say to the microphone results in a _Heard: "COMMAND"_ type of line appearing, try the test mode:

    1. Start _xbmcvc_ with the __-t__ command line switch.
    2. Enter space-separated commands in ALL CAPS, ending a batch by pressing ENTER. Example:

            LEFT TWO SELECT<ENTER>

    You'll probably also want to disable locking for testing, so add the __-l__ switch to the command line as well.

    If entering commands in test mode results in your XBMC instance properly responding to them, it means you're probably facing an ALSA issue. Most usual causes of problems are:

    * capturing from the wrong device,
    * capture levels set too low, too high (yes, overly high sensitivity is also a problem) or muted,
    * speaking too quietly.

*   If after starting _xbmcvc_ you see a series of odd lines like:

        Initializing, please wait...
         256x13 256x13 256x13
         256x13 256x13 256x13
        Ready for listening!

    just ignore them. It's a known issue with _pocketsphinx_ 0.6. These lines shouldn't appear when using _pocketsphinx_ 0.7 or newer.

*   If after starting _xbmcvc_ you get an error saying _Failed to calibrate voice activity detection_, please check your mixer levels for capturing audio.

Language support
----------------

Currently _xbmcvc_ only supports voice commands spoken in English. Additional languages may be added in the future.

Caveats
-------

In normal mode, _xbmcvc_ will perform a maximum of 5 actions at a time to avoid confusion. You can say more voice commands than that, but only the first five recognized commands will be executed.

Feedback
--------

I'm always happy to hear feedback. If you have a problem with _xbmcvc_ or you want to share an idea for a new feature, feel free to [create an issue](https://github.com/kempniu/xbmcvc/issues) at GitHub. You can also [catch me on Twitter](http://twitter.com/kempniu).

