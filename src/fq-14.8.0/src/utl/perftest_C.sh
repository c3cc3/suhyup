set -x
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q TST -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_1=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q TST -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_1=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q TST1 -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_2=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q TST1 -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_2=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q KT -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_3=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q KT -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_3=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q SK -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_4=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q SK -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_4=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q SN -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_5=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q SN -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_5=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q LG -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_6=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q LG -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_6=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q PUSH -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_7=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q PUSH -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_7=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q RCS -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_8=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q RCS -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_8=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q KAKAO -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_9=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q KAKAO -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_9=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q REQUEST -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_10=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q REQUEST -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_10=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q CO_RESULT -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_11=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q CO_RESULT -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_11=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q LG_SM -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_12=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q LG_SM -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_12=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q LG_MM -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_13=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q LG_MM -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_13=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q GW-REAL-REQ -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_14=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q GW-REAL-REQ -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_14=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q GW-BAT-REQ -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_15=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q GW-BAT-REQ -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_15=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q CTRL -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_16=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q CTRL -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_16=$!
#
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q FQPM_ALARM -a DE -b 65536 -u 1500 -P 0 -o error -x on & de_17=$!
TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q FQPM_ALARM -a EN -m 2048 -u 1900 -P 0 -o error -t 1 & en_17=$!
#
TestSQ -l /tmp/TestSQ.de.log -p /home/ums/fq/enmq -q SHM_TST -a DE -b 8192 -u 5 -P 0 -o error & de_18=$!
TestSQ -l /tmp/TestSQ.en.log -p /home/ums/fq/enmq -q SHM_TST -a EN -m 2044 -u 10 -P 0 -o error & en_18=$!
#
TestSQ -l /tmp/TestSQ.de.log -p /home/ums/fq/enmq -q SHM_TST1 -a DE -b 8192 -u 5 -P 0 -o error & de_19=$!
TestSQ -l /tmp/TestSQ.en.log -p /home/ums/fq/enmq -q SHM_TST1 -a EN -m 2044 -u 10 -P 0 -o error & en_19=$!
#
#
sleep $1
kill $en_1 $en_2 $en_3 $en_4 $en_5 $en_6 $en_7 $en_8 $en_9 $en_10 $en_11 $en_12 $en_13 $en_14 $en_15 $en_16 $en_17 $en_18 $en_19
sleep 10
kill $de_1 $de_2 $de_3 $de_4 $de_5 $de_6 $de_7 $de_8 $de_9 $de_10 $de_11 $de_12 $de_13 $de_14 $de_15 $de_16 $de_17 $de_18 $de_19
