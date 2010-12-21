all: playsound mytail

playsound: main.c
	gcc -o playsound main.c -L ../lib -lfmodex64
	
mytail: mytail.c
	gcc -o mytail mytail.c

clean:
	rm -f *.o playsound
