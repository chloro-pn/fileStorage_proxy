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
echo "starting proxy."
sleep 1
./sserver.out > sserver.txt &
echo "starting sserver."
#./client.out > client.txt &
#echo "srarting client."
exit 0
