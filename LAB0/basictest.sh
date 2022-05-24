#!/bin/bash

RED='\033[0;31m'
GRN='\033[0;92m'
DGRN='\033[0;32m'
NM='\033[0m'
BG='\033[1;31;42m'
SEGERR=139

let errors=0

echo "test for lab0 "> file1.txt
./lab0 --input=file1.txt > file1copy.txt
echo "... Test --input reuglar"
if [ $? -eq 0 ]
then
    echo -e "        ${GRN} open exisiting file ... OK${NM}"
else
    echo -e "        ${RED} open exisiting file ... FAILURE${NM}"
    let errors+=1
fi

echo "... Compare test"
cmp file1.txt file1copy.txt > /dev/null
if [ $? -eq 0 ]
then
    echo -e "        ${GRN} Files comparison ... OK${NM}"
else
    echo -e "        ${RED} Files comparison ... FAILURE${NM}"
    let errors+=1
fi

rm -rf *.txt

echo "... Direct specific input file to specfic output file test"
echo " Test is the way to debug " > input.txt
./lab0 --input=input.txt --output=output.txt
find . -name output.txt >/dev/null
if [ $? -eq 0 ]
then
    echo -e "        ${GRN} Found output file ... OK${NM}"
else
    echo -e "        ${RED} Found output file ... FAILURE${NM}"
    let errors+=1
fi
cmp input.txt output.txt
if [ $? -eq 0 ]
then
   echo -e "        ${GRN} Input Output Files comparison ... OK${NM}"
else
    echo -e "       ${RED} Input Output comparison ... FAILURE${NM}"
    let errors+=1
fi
rm -rf *.txt

echo "... Grep Unable To Open Input File Test"
./lab0 --input=dontexsist.txt 2>>ERRMSG
grep -q "Error occured when opening input file: dontexsist.txt, Exit code: 2, No such file or directory" ./ERRMSG
if [ $? -eq 0 ]
then
    echo -e "        ${GRN} Unable To Open Input File Err Msg ... OK${NM}"
else
    echo -e "        ${RED} Unable To Open Input File Err Msg ... FAILURE${NM}"
    let errors+=1
fi
rm -rf ERRMSG

echo "... Grep Unable To Create Output File Test"
touch dontexsist.txt
chmod u-w dontexsist.txt
echo "Test is the way to debug " > input.txt
./lab0 --input=input.txt --o=dontexsist.txt 2>>ERRMSG
grep -q "Error occured when creating output file: dontexsist.txt, Exit code: 13, Permission denied" ./ERRMSG
if [ $? -eq 0 ]
then
    echo -e "        ${GRN} Unable To Create Output File Err Msg ... OK${NM}"
else
    echo -e "        ${RED} Unable To Create Output File Err Msg ... FAILURE${NM}"
    let errors+=1
fi
rm -rf ERRMSG

echo "... Grep Unable To Create Output File Test With --input opt"
touch dontexsist2.txt
chmod u-w dontexsist2.txt
echo "   ...Test is the way to debug " > input.txt
./lab0 --input=input.txt --o=dontexsist2.txt 2>>ERRMSG
grep -q "Error occured when creating output file: dontexsist2.txt, Exit code: 13, Permission denied" ./ERRMSG
if [ $? -eq 0 ]
then
    echo -e "        ${GRN} Grep Unable To Create Output File Err Msg ... OK${NM}"
else
    echo -e "        ${RED} Grep Unable To Create Output File Err Msg ... FAILURE${NM}"
    let errors+=1
fi
rm -rf ERRMSG

echo "... Trap SegFault Test"
echo -n "        "
./lab0 --s 
if [ $? -eq $SEGERR ]
then
    echo -e "        ${GRN} Trap Segfault Error 139 ... OK${NM}"
else
    echo -e "        ${RED} Trap Segfault Error 139 ... FAILURE${NM}"
    let errors+=1
fi

echo "... Catch SegFault Test"
./lab0 --s --c 2> ERRMSG
grep -q "Catch Segmentation Fault SEGSEV number: 11" ./ERRMSG
if [ $? -eq 0 ]
then
    echo -e "        ${GRN} Catch Segfault Error and Print Err Msg ... OK${NM}"
else
    echo -e "        ${RED} Catch Segfault Error and Print Err Msg ... FAILURE${NM}"
    let errors+=1
fi
rm -rf ERRMSG

echo "... Invalid Option Test"
ERR="lab0: unrecognized option \`--badbad2'\\n
invalid option --?\\n
usage: lab0 [options]\\n
 --input=file name\\n
 --output=file name\\n
 --segfault\\n
 --catch"

./lab0 --badbad2 2>ERRMSG
grep -q "${ERR}" ./ERRMSG
if [ $? -eq 0 ]
then
    echo -e "        ${GRN} Invalid Option Test ... OK${NM}"
else
    echo -e "        ${RED} Invalid Option Test ... FAILURE${NM}"
    let errors+=1
fi
rm -rf ERRMSG
rm -rf *.txt

if [ $errors -ne 0 ]
then
    echo -e "${BG} $errors errors found${NM}"
else
    echo -e "${BG} Pass All Tests${NM}"
fi