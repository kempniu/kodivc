EXECUTABLE=xbmcvc
MODELDIR=`pkg-config --variable=modeldir pocketsphinx`
LIBS=`pkg-config --cflags --libs pocketsphinx sphinxbase` -lcurl
GITVERSION=`git log --oneline | cut -d' ' -f1`

all:
	gcc -o $(EXECUTABLE) $(EXECUTABLE).c -DGITVERSION=\"$(GITVERSION)\" -DMODELDIR=\"$(MODELDIR)\" $(LIBS)

install:
	install $(EXECUTABLE) /usr/bin; \
	install -m 0644 model/xbmcvc.* $(MODELDIR)/lm/en

