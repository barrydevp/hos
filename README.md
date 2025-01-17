# H(obby) operating system

Hobby or hope?

# Prerequisites

For Ubuntu(Debian)-Linux only!

```bash

### RUST
#
# install rust nightly and some components
rustup toolchain install nightly

# setup nightly feature by default
# cd into project and run
rustup override set nightly

# add rust-src
rustup component add rust-src

### GCC cross compiler, according to https://wiki.osdev.org/GCC_Cross_Compiler
#
# base package
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo

# let's build your cross compiler gcc
# your storage
mkdir -p $HOME/opt/cross
mkdir -p $HOME/opt/src

# specify your target, below example is "i686-elf"
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

# go to sources
cd $HOME/opt/src

# binutils
curl -O https://ftp.gnu.org/gnu/binutils/binutils-2.43.tar.xz
tar xf binutils-2.43.tar.xz

mkdir build-binutils
cd build-binutils
../binutils-2.43/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
make install

# gcc (binutils must completed first)
curl -O https://ftp.gnu.org/gnu/gcc/gcc-14.2.0/gcc-14.2.0.tar.xz
tar xf gcc-14.2.0.tar.xz

# The $PREFIX/bin dir _must_ be in the PATH. We did that above.
which -- $TARGET-as || echo $TARGET-as is not in the PATH

mkdir build-gcc
cd build-gcc
../gcc-14.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers --disable-hosted-libstdcxx
make all-gcc
make all-target-libgcc
make all-target-libstdc++-v3
make install-gcc
make install-target-libgcc
make install-target-libstdc++-v3

# gdb
curl -O https://ftp.gnu.org/gnu/gdb/gdb-14.2.tar.xz
tar xf gdb-14.2.tar.xz

mkdir build-gdb
cd build-gdb
../gdb-14.2/configure --target=$TARGET --prefix="$PREFIX" --disable-werror
make all-gdb
make install-gdb

# grub toolchains
sudo apt install grub-pc-bin xorriso mtools

```

## Debugging

### Tips

- https://student.cs.uwaterloo.ca/~cs350/common/gdb.html
- https://github.com/jtsiomb/pcboot/blob/master/gdbnotes


## Resources

[OS-dev](https://wiki.osdev.org/Creating_an_Operating_System)

[Rust-kernel](https://os.phil-opp.com)

[OS Development Series BrokenThorn](http://www.brokenthorn.com/Resources/OSDevIndex.html)

[Operating system from 0 to 1](https://github.com/tuhdo/os01)

[Dreamos82 Osdev-Notes](https://github.com/dreamos82/Osdev-Notes)

### Some other Kernel - OS and Credits

[mentos](https://github.com/mentos-team/MentOS) <- mainly base on this code base

[toaruos](https://github.com/klange/toaruos)

[mquy/mos](https://github.com/MQuy/mos)

[Dreamos64](https://github.com/dreamos82/Dreamos64)
