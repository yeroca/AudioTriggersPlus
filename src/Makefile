all: AudioTrigger

AudioTrigger: main.c
	gcc -Werror -Wall -O0 -g3 -o AudioTrigger main.c -L ../lib -lfmodex64 -lpthread

parse_xml: parse_xml.c
	gcc -Werror -Wall -O0 -g3 -o parse_xml -I /usr/include/libxml2 -lxml2 parse_xml.c


clean:
	rm -f *.o AudioTrigger
