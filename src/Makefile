all: AudioTrigger

AudioTrigger: main.c
	gcc -Werror -Wall -O0 -g3 -o AudioTrigger main.c -L ../lib -lfmodex64 -lpthread


clean:
	rm -f *.o AudioTrigger
