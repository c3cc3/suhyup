#!/bin/sh

PROCESS=fq_handler_svr_json
APP_CONF=fq_handler_svr_json.conf
BIN_INSTALL_PATH=/home/ums/fq/exec
CONF_INSTALL_PATH=/home/ums/fq/conf

cp -f /home/ums/fq/bin/fq_handler_svr_json /home/ums/fq/exec/

echo $PROCESS

# csh일 경우 setenv LANG ko_KR.eucKR, AIX의 경우 ko_KR.IBM-eucKR
export LANG=ko_KR.euckr

if [ $# -eq 1 ]
then
    echo $1
elif [ $# -eq 2 ]
then
    echo $1
    echo $2
else
    echo "usage: uagent.sh start/stop/kill"
    echo "       start : START fq_handler_svr_json."
    echo "       bstart : background START fq_handler_svr_json."
    echo "       stop  : STOP fq_handler_svr_json."
    echo "       kill  : KILL fq_handler_svr_json immediately."
    exit
fi

if [ $1 = "start" ]
then

	count=$(ps -ef | grep "$PROCESS"| grep "$APP_CONF" | grep -v grep | grep -v tail | wc -l)
	if [ $count -eq 0 ]
	then
		$BIN_INSTALL_PATH/$PROCESS $CONF_INSTALL_PATH/$APP_CONF 
		ps -ef | grep "$PROCESS" | grep "$APP_CONF"
	else
		echo "$PROCESS is already running."
	fi
elif [ $1 = "bstart" ]
then
	count=$(ps -ef | grep "$PROCESS"| grep "$APP_CONF" | grep -v grep | grep -v tail | wc -l)
	if [ $count -eq 0 ]
	then
		nohup $BIN_INSTALL_PATH/$PROCESS $CONF_INSTALL_PATH/$APP_CONF  > /dev/null 2>&1 &
		ps -ef | grep "$PROCESS" | grep "$APP_CONF"
	else
		echo "$PROCESS is already running."
	fi
elif [ $1 = "stop" ]
then
	count=$(ps -ef | grep "$PROCESS" | grep "$APP_CONF" | grep -v grep | grep -v tail | wc -l)
	if [ $count -eq 1 ]
	then
		p_string=$(ps -ef | grep "$PROCESS" | grep "$APP_CONF" | grep -v grep | grep -v tail)
		pid=$(echo $p_string | awk '{print $2}')
	 	echo "kill pid : $pid"
	 	kill $pid
	else
		echo "$PROCESS is not running"	
	fi

elif [ $1 = "kill" ]
then
	count=$(ps -ef | grep "$PROCESS" | grep "$APP_CONF" | grep -v grep | grep -v tail | wc -l)
	if [ $count -eq 1 ]
	then
		p_string=$(ps -ef | grep "$PROCESS" | grep "$APP_CONF" | grep -v grep | grep -v tail)
		pid=$(echo $p_string | awk '{print $2}')
	 	kill -9 $pid
	else
		echo "$PROCESS is not running"	
	fi
fi
