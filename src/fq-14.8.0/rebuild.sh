set -x
autoscan
# mv configure.scan configure.ac
mkdir -p m4
aclocal
# libtoolize
autoupdate
autoconf
autoheader
touch NEWS README AUTHORS ChangeLog
automake --add-missing
./configure --bindir=/ums/fq/bin --libdir=/ums/fq/lib --includedir=/ums/fq/include
make
make dist
#make install
#autoreconf -f -i
