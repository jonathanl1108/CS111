#!/bin/bash
#test1

echo "TEST: ./lab4b --scale=F --period=2  --log"
./lab4b --period=1 --scale=F --log="LOGFILE" <<-EOF
SCALE=F
PERIOD=1
START
STOP
LOG test
OFF
EOF
ret=$?
if [ $ret -ne 0 ]
then
	echo "Failed Standard test"
fi

 echo "Pass Standard test"
if [ ! -s LOGFILE ]
then
	echo "did not create a log file"
fi
rm -f LOGFILE

echo "Test2 compare Log Entries"

{ echo "PERIOD=1";sleep 3;echo "START";\
sleep 3;echo "SCALE=C"; echo "STOP"; sleep 2;echo "START";sleep 2;\
 echo "LOG test"; echo "OFF"; } | ./lab4b --period=2 --scale=F --log=LOGFILE2 
error=0
if [ ! -s LOGFILE2 ]
then
	echo "did not create a log file"
else
	for c in SCALE=C PERIOD=1 START STOP LOG OFF SHUTDOWN
		do
			grep "$c" LOGFILE2 > /dev/null
			if [ $? -ne 0 ]
			then
				let error+=1
				echo "DID NOT LOG $c command"
			else
				echo "    $c ... RECOGNIZED AND LOGGED"
			fi
		done
	rm -rf LOGFILE2
fi

if [ $error -gt 0 ]
then 
	echo "Test2 Failed"
else
	echo "PASS ALL TEST"
fi
