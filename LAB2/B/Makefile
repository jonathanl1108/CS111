#NAME: Jonathan Liau
#EMAIL: jonathanliau747@gmail.com
#ID: 105182103

build:
	gcc -Wall -Wextra -g -o lab2_list -lpthread -lprofiler lab2_list.c SortedList.c

profile: clean build
	rm -f raw.gperf  profile.out
	LDPRELOAD=/lib64/libprofiler.so.0 CPUPROFILE=./raw.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	pprof --text ./lab2_list ./raw.gperf  > profile.out
	pprof --list=thread_worker ./lab2_list ./raw.gperf  >> profile.out
	rm -f raw.gperf 

clean:
	rm -rf lab2_list *.tar.gz 
cleancsv:
	rm -rf *.csv
dist: clean build tests graphs profile
	tar -czvf lab2b-105182103.tar.gz  lab2_list.c SortedList.h SortedList.c lab2b_list.csv lab2b_list.gp lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png Makefile README profile.out test.sh
graphs:
	gnuplot lab2b_list.gp

tests:
	rm -rf *.csv
	chmod +x ./test.sh
	./test.sh
