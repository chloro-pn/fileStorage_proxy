#!/bin/sh

echo "stopping proxy"

cd $(dirname $0)

PROXY_PIDS=`ps -ef | grep proxy.out` | grep -v grep | awk "print $2"`

ret=0
for PID in $PROXY_PIDS
do
	cmd="kill -9 $PID"
	echo $cmd
	$cmd
	let ret=ret+$?
done

if[ $ret -ne 0 ]
then
	echo "can't stop proxy."
	exit 1
else
	echo "proxy stoped."
	exit 0
f
