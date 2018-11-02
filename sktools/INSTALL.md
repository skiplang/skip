## Ubuntu/Debian:

Install the dependencies:

```
$ sudo apt-get install clang-6.0 libgoogle-glog-dev libpcre++-dev libboost-all-dev libevent-dev
```

Install SKIP:

```
$ cd skip-Linux-0.8 && ./install.sh
```


## OS X

If Homebrew is not already installed
```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```

Install the dependencies:

```
$ brew install git-lfs ninja dwarfutils libelf boost libevent gflags glog jemalloc node autoconf automake pkg-config libtool
```

Install SKIP:

```
$ cd skip-Darwin-0.8 && ./install.sh
```
