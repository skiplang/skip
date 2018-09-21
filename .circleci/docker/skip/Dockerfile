FROM ubuntu:18.04

RUN apt-get update -y
RUN apt-get install -y \
  autoconf \
  ca-certificates \
  clang \
  clang-format \
  emacs-nox \
  g++ \
  g++-aarch64-linux-gnu \
  gcc-aarch64-linux-gnu \
  gdb \
  git \
  gzip \
  hhvm \
  lcov \
  libboost-all-dev \
  libdwarf-dev \
  libelf-dev \
  libevent-dev \
  libgflags-dev \
  libgoogle-glog-dev \
  libicu-dev \
  libssl-dev \
  libtool \
  libtool-bin \
  m4 \
  make \
  ninja-build \
  nodejs \
  pkg-config \
  qemu \
  qemu-user-static \
  rsync \
  software-properties-common \
  ssh \
  sudo \
  tar \
  time \
  wget

# Newer cmake (3.11 isn't default until cosmic)
RUN (cd /tmp ; wget https://cmake.org/files/v3.11/cmake-3.11.1-Linux-x86_64.sh ; chmod 755 ./cmake-3.11.1-Linux-x86_64.sh ; ./cmake-3.11.1-Linux-x86_64.sh --prefix=/usr --skip-license ; rm cmake-3.11.1-Linux-x86_64.sh)

# Setup user 'ci'
RUN useradd -m ci -p '$6$yoYlBnox$L9WVfSRz4KLUmQdsU9PpWI25T1B8NqL5/373ayYYOZCl80kuKQHrumWQXO3DyFAObRQeHA3Hhj0eUuLA5mcws1' -G sudo

# Cleanup
USER root
RUN apt-get clean
RUN rm -rf /var/lib/apt/lists/*

# Default user
USER ci
