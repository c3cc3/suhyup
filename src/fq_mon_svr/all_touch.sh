set -x
#TestFQ -l /tmp/TestFQ.en.log -p /home/ums/fq/enmq -q $1 -a EN -m 4092 -u 1000000 -P 0 -o error -t 1
TestFQ -l /tmp/TestFQ.de.log -p /home/ums/fq/enmq -q $1 -a DE -b 65536 -u 1000000 -P 0 -o error -x off
