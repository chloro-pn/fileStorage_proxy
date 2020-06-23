#!/bin/sh

dir_exe=examples
ulimit -c unlimited
echo "$0"
cd $(dirname $0)
cd $dir_exe
if [ $? -ne 0 ]; then
	echo "cannot change dir to $direxe"
	exit 1
fi

./proxy.out ../conf/proxy.conf > proxy.txt &
echo "start proxy succ."
./sserver.out > sserver.txt &
echo "start sserver succ."
./client.out > client.txt &
echo "srart client succ."
exit 0
