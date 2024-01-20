set -x
FQ_Protocol_cli_txt -i 172.30.1.25 -I 127.0.0.1 -p 6666 -l /tmp/FQ_Protocol_cli_txt.log -P /home/pi/data -Q TST -c get -o error
