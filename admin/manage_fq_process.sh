#!/bin/sh
BIN_INSTALL_PATH=/ums/fq/exec
CONF_INSTALL_PATH=/ums/fq/conf/umsapp01

# csh▒▒ ▒▒▒ setenv LANG ko_KR.eucKR, AIX▒▒ ▒▒▒ ko_KR.IBM-eucKR
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
    echo "       check : check COUNT."
    echo "       bstart : background START ratio distributor."
    echo "       stop  : STOP ratio distributor."
    echo "       kill  : KILL ratio distributor immediately."
    exit
fi

if [ $1 = "check" ]
then
        echo "==========================================================."
        echo "Checking run shell."
        count=$(ps -ef | grep "run_fq"| grep -v grep | grep -v tail | grep -v vi | wc -l)
        if [ $count -eq 10 ]
        then
                echo -e "\t -> All run process is normal (count=$count)."
        else
                echo -e "\t -> Warning!!! -> Abnormal. Check process. Number of fq_run is $count."
        fi
        echo "==========================================================."
        echo "Checking start/stop shell."
        count=$(ps -ef | grep "fq_"| grep start | grep -v grep | grep -v tail | grep -v vi | wc -l)
        if [ $count -eq 9 ]
        then
                echo -e "\t -> All start/stop shell  process is normal (count=$count)."
        else
                echo -e "\t ->Warning!!! -> Abnormal. Check process. Number of fq_run is $count."
        fi
        echo "==========================================================."
        echo "Checking real application processes."
        count=$(ps -ef | grep "fq_"| grep ".conf" | grep -v grep| wc -l)
        if [ $count -eq 10 ]
        then
                echo -e "\t -> All fq application process is normal (count=$count)."
        else
                echo -e "\t -> Warning!!! -> Abnormal. Check process. Number of fq_run is $count."
        fi
        echo "==========================================================."
elif [ $1 = "stop" ]
then
        echo "We will stop all run shells."
        p_string_0=$(ps -ef | grep "run_fq" | grep -v grep | grep -v tail | awk '{print $2}')
        echo $p_string_0
        kill -9 $p_string_0
        sleep 1
        echo "==========================================================."
        echo "We will stop all start/stop shells."
        p_string_1=$(ps -ef | grep "fq_" | grep start |  grep -v grep | grep -v tail | awk '{print $2}')
        echo $p_string_1
        kill -9 $p_string_1
        sleep 1
        echo "==========================================================."
        echo "We will stop all application processes."
        p_string_2=$(ps -ef | grep "fq_" | grep ".conf" | grep -v grep | grep -v tail | awk '{print $2}')
        echo $p_string_2
        kill -9 $p_string_2
        echo "==========================================================."

elif [ $1 = "start" ]
then
        echo "==========================================================."
        echo "(1) File Queue Handler starting..."
        nohup /ums/fq/admin/run_fq_handler.sh &
        sleep 1
        echo "(2) File Queue Monitoring server starting..."
        nohup /ums/fq/admin/run_fq_mon_svr.sh &
        sleep 1
        echo "(3) File Queue webgate server starting..."
        nohup /ums/fq/admin/run_fq_webgate.sh &
        sleep 1
        echo "(4) File Queue Delete Hash server  starting..."
        nohup /ums/fq/admin/run_fq_DeleteHash.sh &
        sleep 1
#        echo "(5) File Queue Agent thread starting..."
#        nohup /ums/fq/admin/run_fq_agent.sh &
#        sleep 1
        echo "(6) File Queue Ratio Distributor for REAL(online)  starting..."
        nohup /ums/fq/admin/run_fq_dist_real.sh &
        sleep 1
        echo "(7) File Queue Ratio Distributor for BATCH  starting..."
        nohup /ums/fq/admin/run_fq_dist_bat.sh &
        sleep 1
        echo "(8) File Queue Alarm server starting..."
        nohup /ums/fq/admin/run_fq_alarm_svr.sh &
        sleep 1
        echo "(9) File Queue Process monitorig server startiing..."
        /ums/fq/admin/fq_pmon_svr.sh bstart
        echo "(10) File Queue Ratio Distributor for REAL 815(online)  starting..."
        nohup /ums/fq/admin/run_fq_dist_real_815.sh &
        sleep 1
        echo "(11) File Queue Ratio Distributor for REAL 816(online)  starting..."
        nohup /ums/fq/admin/run_fq_dist_real_816.sh &
        sleep 1
        echo "(13) File Queue Ratio Distributor for REAL 817(online)  starting..."
        nohup /ums/fq/admin/run_fq_dist_real_817.sh &

        echo "==========================================================."
fi

