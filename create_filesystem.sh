echo '============================================================================='
echo 'Creating EXT2 filesystem...'
echo '============================================================================='

mkdir -p ./rootfs/proc
mkdir -p ./rootfs/dev
mkdir -p ./rootfs/usr
echo "Hello Wolrd" >> ./rootfs/usr/hello.txt

mke2fs -L 'rootfs' -N 0 -d ./rootfs -b 4096 -m 5 -r 1 -t ext2 -v -F ./rootfs.img 32M
echo '============================================================================='
echo 'Done!'
echo '============================================================================='
