xbmcvc
======

_xbmcvc_ is a program for controlling XBMC (http://xbmc.org/) with simple voice commands. It uses CMU Sphinx for speech recognition.

Requirements
------------

To use _xbmcvc_, the following libraries need to be installed on your system:

* _pocketsphinx_ along with its prerequisite, _sphinxbase_ (http://cmusphinx.sourceforge.net/wiki/download/); _xbmcvc_ was tested with versions 0.6+
* _libcurl_ (shipped with cURL, should be present on most systems; if that's not your case, go to http://curl.haxx.se/libcurl/)

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

At startup, _xbmcvc_ will initialize the speech recognition library and, after successfully self-calibrating to properly tell silence and speech apart, it will start listening to your commands. If you get an error saying _Failed to calibrate voice activity detection_, please check your mixer levels for capturing audio.

By default, _xbmcvc_ locks itself after initializing to prevent accidental usage. Say _"XBMC"_ to the microphone to unlock. This has to be the first command heard in order to work. To lock _xbmcvc_, say _"OK"_. The locking/unlocking feature can be disabled using the __-l__ command line switch.

After unlocking (or disabling locking), simply speak the commands (see below) to the microphone - _xbmcvc_ will translate them to JSON-RPC requests which it will then send to the desired XBMC instance. If you are trying to control an XBMC instance installed on a different machine than the one you're running _xbmcvc_ on (or your XBMC listens on a non-standard port), pass the correct hostname and port number to _xbmcvc_ via the __-H__ and __-P__ command line switches, respectively.

_xbmcvc_ recognizes a limited set of commands listed in file __model/xbmcvc.vocab__. You'll find they are divided into sections. Each section corresponds to one mode of operation. The default command set is for the "normal" mode, which you'll probably use the most - these commands are supposed to be self-explanatory. The other mode is the "spelling" mode, which you can use to input letters and digits (e.g. when performing a search or renaming a movie/album etc.). These commands might be surprising as _xbmcvc_ uses the [NATO phonetic alphabet](http://en.wikipedia.org/wiki/NATO_phonetic_alphabet) for spelling. The reason for that is that single letter recognition is very inaccurate, which shouldn't come as much of a surprise (how often do you spell your surname over the phone and people at the other end get it wrong?). NATO phonetic alphabet usage enables _pocketsphinx_ to recognize the desired letters with much better accuracy.

To switch between different modes of operation, use the following voice commands:

* [normal] => SPELL => [spell] => DONE / BACK / NORMAL => [normal]

Note that every mode switching command has to be the only command heard in a batch (one batch = one "Heard: COMMAND1 COMMAND2" type of line in _xbmcvc_ output) in order to be recognized.

Please consult the usage message (run _xbmcvc_ with the __-h__ switch to view it) for command line switches explanation.

Troubleshooting
---------------

If you get the _Ready for listening!_ message but nothing you say to the microphone results in a _Heard: COMMAND_ type of line appearing, try the test mode:

1. Start _xbmcvc_ with the -t command line switch.
2. Enter space-separated commands in ALL CAPS, confirming each batch by pressing ENTER. Example:

        LEFT TWO SELECT<ENTER>

You'll probably also want to disable locking for testing, so add the __-l__ switch to the command line as well.

If entering commands in test mode results in your XBMC instance properly responding to them, it means you're probably facing an ALSA issue. Most usual causes of problems are:

* capturing from the wrong device,
* capture levels set too low, too high (yes, overly high sensitivity is also a problem) or muted,
* speaking too quietly.

If capturing from the default ALSA device doesn't work for you, you can specify the ALSA device you want to capture speech from using the __-D__ command line switch.

Language support
----------------

Currently _xbmcvc_ only supports voice commands spoken in English. Additional languages may be added in the future.

Caveats
-------

In "normal" mode, _xbmcvc_ will perform a maximum of 5 actions at a time to avoid confusion. You can say more voice commands than that, but only the first five recognized commands will be executed.

Feedback
--------

I'm always happy to hear feedback. If you have a problem with _xbmcvc_ or you want to share an idea for a new feature, feel free to [create an issue](https://github.com/kempniu/xbmcvc/issues) at GitHub. You can also [catch me on Twitter](http://twitter.com/kempniu).

