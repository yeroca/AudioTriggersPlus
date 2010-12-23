all: playsound

playsound: main.c
	gcc -O0 -g3 -o playsound main.c -L ../lib -lfmodex64 -lpthread

clean:
	rm -f *.o playsound
