OS := $(shell uname -s | sed -e 's/[^A-Za-z].*//')

all: AudioTrigger 


ifeq ($(OS),CYGWIN)
FMODLIB=../lib/libfmodex.a
else
FMODLIB=-Xlinker -rpath -Xlinker ../lib -L ../lib -lfmodex64
endif

AudioTriggersPlus: main.c
	gcc -Werror -Wall -O0 -g3 -o AudioTriggersPlus -I /usr/include/libxml2 main.c $(FMODLIB) -lpthread -lxml2

install:
ifeq ($(OS),CYGWIN)
	cp ../lib/fmodex.dll /usr/local/bin
else
endif

clean:
	rm -f *.o AudioTrigger
