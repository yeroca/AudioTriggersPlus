all: AudioTrigger test_fmod parse_xml xpath1

AudioTrigger: main.c
	gcc -Werror -Wall -O0 -g3 -o AudioTrigger -I /usr/include/libxml2 main.c -L ../lib -lfmodex64 -lpthread -lxml2

test_fmod: test_fmod.c
	gcc -Werror -Wall -O0 -g3 -o test_fmod test_fmod.c -L ../lib -lfmodex64 -lpthread

parse_xml: parse_xml.c
	gcc -Werror -Wall -O0 -g3 -o parse_xml -I /usr/include/libxml2 parse_xml.c -lxml2

xpath1: xpath1.c
	gcc -Werror -Wall -O0 -g3 -o xpath1 -I /usr/include/libxml2 -lxml2 xpath1.c

clean:
	rm -f *.o AudioTrigger test_fmod
