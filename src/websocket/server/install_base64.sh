set -x
#sudo apt-get install libb64-dev
# 소스 코드 다운로드
wget https://github.com/aklomp/base64/archive/master.zip
unzip master.zip
cd base64-master

# 빌드
make
sudo make install
