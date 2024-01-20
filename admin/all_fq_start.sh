set -x
nohup /ums/fq/admin/run_fq_handler.sh &
nohup /ums/fq/admin/run_fq_mon_svr.sh &
nohup /ums/fq/admin/run_webgate.sh &
nohup /ums/fq/admin/run_DeleteHash.sh &
nohup /ums/fq/admin/run_dist_real.sh &
#nohup /ums/fq/admin/run_dist_bat.sh &
nohup /ums/fq/admin/run_fq_alarm_svr.sh &
