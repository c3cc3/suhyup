set -x
java TestDeQXA /home/ums/fq/enmq TST 0 & de_1=$!
java TestEnQ_loop /home/ums/fq/enmq TST 1 & en_1=$!
#
java TestDeQXA /home/ums/fq/enmq TST1 0 & de_2=$!
java TestEnQ_loop /home/ums/fq/enmq TST1 1 & en_2=$!
#
java TestDeQXA /home/ums/fq/enmq KT 0 & de_3=$!
java TestEnQ_loop /home/ums/fq/enmq KT 1 & en_3=$!
#
java TestDeQXA /home/ums/fq/enmq SK 0 & de_4=$!
java TestEnQ_loop /home/ums/fq/enmq SK 1 & en_4=$!
#
java TestDeQXA /home/ums/fq/enmq SN 0 & de_5=$!
java TestEnQ_loop /home/ums/fq/enmq SN 1 & en_5=$!
#
java TestDeQXA /home/ums/fq/enmq LG 0 & de_6=$!
java TestEnQ_loop /home/ums/fq/enmq LG 1 & en_6=$!
#
java TestDeQXA /home/ums/fq/enmq PUSH 0 & de_7=$!
java TestEnQ_loop /home/ums/fq/enmq PUSH 1 & en_7=$!
#
java TestDeQXA /home/ums/fq/enmq RCS 0 & de_8=$!
java TestEnQ_loop /home/ums/fq/enmq RCS 1 & en_8=$!
#
java TestDeQXA /home/ums/fq/enmq KAKAO 0 & de_9=$!
java TestEnQ_loop /home/ums/fq/enmq KAKAO 1 & en_9=$!
#
java TestDeQXA /home/ums/fq/enmq REQUEST 0 & de_10=$!
java TestEnQ_loop /home/ums/fq/enmq REQUEST 1 & en_10=$!
#
java TestDeQXA /home/ums/fq/enmq CO_RESULT 0 & de_11=$!
java TestEnQ_loop /home/ums/fq/enmq CO_RESULT 1 & en_11=$!
#
java TestDeQXA /home/ums/fq/enmq LG_SM 0 & de_12=$!
java TestEnQ_loop /home/ums/fq/enmq LG_SM 1 & en_12=$!
#
java TestDeQXA /home/ums/fq/enmq LG_MM 0 & de_13=$!
java TestEnQ_loop /home/ums/fq/enmq LG_MM 1 & en_13=$!
#
java TestDeQXA /home/ums/fq/enmq GW-REAL-REQ 0 & de_14=$!
java TestEnQ_loop /home/ums/fq/enmq GW-REAL-REQ 1 & en_14=$!
#
java TestDeQXA /home/ums/fq/enmq GW-BAT-REQ 0 & de_15=$!
java TestEnQ_loop /home/ums/fq/enmq GW-BAT-REQ 1 & en_15=$!
#
#
sleep 300
kill $en_1 $en_2 $en_3 $en_4 $en_5 $en_6 $en_7 $en_8 $en_9 $en_10 $en_11 $en_12 $en_13 $en_14 $en_15
sleep 10
kill $de_1 $de_2 $de_3 $de_4 $de_5 $de_6 $de_7 $de_8 $de_9 $de_19 $de_11 $de_12 $de_13 $de_14 $de_15
