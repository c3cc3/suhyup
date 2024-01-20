TestFQ -l /tmp/TestFQ.de.log -p /umsdata1/enmq -q K_ONL -a DE -b 65536 -u 10 -P 0 -o debug -x on & de_1=$!
TestFQ -l /tmp/TestFQ.en.log -p /umsdata1/enmq -q K_ONL -a EN -m 4092 -u 50 -P 0 -o error -t 1 & en_1=$!
TestFQ -l /tmp/TestFQ.de.log -p /umsdata1/enmq -q K_BAT -a DE -b 65536 -u 10 -P 0 -o debug -x on & de_2=$!
TestFQ -l /tmp/TestFQ.en.log -p /umsdata1/enmq -q K_BAT -a EN -m 4092 -u 50 -P 0 -o error -t 1 & en_2=$!
TestFQ -l /tmp/TestFQ.de.log -p /umsdata1/enmq -q C_ONL -a DE -b 65536 -u 10 -P 0 -o debug -x on & de_3=$!
TestFQ -l /tmp/TestFQ.en.log -p /umsdata1/enmq -q C_ONL -a EN -m 4092 -u 50 -P 0 -o error -t 1 & en_3=$!
TestFQ -l /tmp/TestFQ.de.log -p /umsdata1/enmq -q C_BAT -a DE -b 65536 -u 10 -P 0 -o debug -x on & de_4=$!
TestFQ -l /tmp/TestFQ.en.log -p /umsdata1/enmq -q C_BAT -a EN -m 4092 -u 50 -P 0 -o error -t 1 & en_4=$!

TestFQ -l /tmp/TestFQ.de.log -p /umsdata1/enmq -q GW-REAL-PREHIST -a DE -b 65536 -u 10 -P 0 -o debug -x on & de_5=$!
TestFQ -l /tmp/TestFQ.en.log -p /umsdata1/enmq -q GW-REAL-PREHIST -a EN -m 4092 -u 50 -P 0 -o error -t 1 & en_5=$!

TestFQ -l /tmp/TestFQ.de.log -p /umsdata1/enmq -q GW-BAT-PREHIST -a DE -b 65536 -u 10 -P 0 -o debug -x on & de_6=$!
TestFQ -l /tmp/TestFQ.en.log -p /umsdata1/enmq -q GW-BAT-PREHIST -a EN -m 4092 -u 50 -P 0 -o error -t 1 & en_6=$!

TestFQ -l /tmp/TestFQ.de.log -p /umsdata1/enmq -q GW-REAL-HIST-815 -a DE -b 65536 -u 10 -P 0 -o debug -x on & de_7=$!
TestFQ -l /tmp/TestFQ.en.log -p /umsdata1/enmq -q GW-REAL-HIST-815 -a EN -m 4092 -u 50 -P 0 -o error -t 1 & en_7=$!

TestFQ -l /tmp/TestFQ.de.log -p /umsdata1/enmq -q GW-REAL-HIST-816 -a DE -b 65536 -u 10 -P 0 -o debug -x on & de_8=$!
TestFQ -l /tmp/TestFQ.en.log -p /umsdata1/enmq -q GW-REAL-HIST-816 -a EN -m 4092 -u 50 -P 0 -o error -t 1 & en_8=$!

sleep 60
kill $en_1 $en_2 $en_3 $en_4 $en_5 $en_6 $en_7 $en_8
sleep 5
kill $de_1 $de_2 $de_3 $de_4 $de_5 $de_6 $de_7 $de_8
