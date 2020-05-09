#!/bin/bash

test $cel0 || cel0=out/cel0
cel0=`readlink -f $cel0`

if test ! -f $cel0;
then
    echo Cant find cel0 binary
    exit 1
fi
echo using $cel0
cd examples
for f in `ls *.cel`; do
    echo running $cel0 on $f
    result=`$cel0 < $f`
    expectation=`cat ${f%.*}.exp`
    if [ "$expectation" = "$result" ];
    then
	echo Pass.
    else
	echo Failed.
	echo Expected:
	echo --$expectation--
	echo Got:
	echo --$result--
    fi
done
