#NAME: Jonathan Liau
#EMAIL: jonathanl1108@g.ucla.edu
#ID: 105182103

lab0:
	gcc -Wall -Wextra -g -o lab0 lab0.c;

check:
	chmod +x ./basictest.sh
	./basictest.sh

clean:
	rm -rf lab0 *.tar.gz *.txt
dist:
	tar -czvf lab0-105182103.tar.gz lab0.c Makefile backtrace.png \
	breakpoint.png README basictest.sh

