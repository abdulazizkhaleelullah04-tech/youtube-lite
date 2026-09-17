CC ?= gcc
CFLAGS += $(shell pkg-config --cflags gtk4 webkitgtk-6.0 webkitgtk-web-process-extension-6.0) -O2 -Wall -Wextra
LIBS += $(shell pkg-config --libs gtk4 webkitgtk-6.0)
EXT_LIBS += $(shell pkg-config --libs webkitgtk-web-process-extension-6.0)

all: youtube-lite webext/libyoutube-lite-webextension.so

youtube-lite: src/main.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

webext/libyoutube-lite-webextension.so: src/web_extension.c
	mkdir -p webext
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< $(EXT_LIBS)

clean:
	rm -f youtube-lite webext/libyoutube-lite-webextension.so
