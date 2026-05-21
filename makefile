all: myshell looper Printers mypipe mypipeline

myshell: myshell.c LineParser.c
	gcc -g -Wall -m32 -o myshell myshell.c LineParser.c


looper: looper.c
	gcc -g -Wall -m32 -o looper looper.c

Printers: Printers.c
	gcc -g -Wall -m32 -o Printers Printers.c

mypipe: mypipe.c
	gcc -g -Wall -m32 -o mypipe mypipe.c

mypipeline: mypipeline.c
	gcc -g -Wall -m32 -o mypipeline mypipeline.c

.PHONY: clean

clean: 
	rm -f *.o myshell looper Printers mypipe mypipeline