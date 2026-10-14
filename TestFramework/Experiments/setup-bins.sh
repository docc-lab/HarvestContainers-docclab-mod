#!/usr/bin/bash

build() {
  echo "[+] Build bins"
  cur=$(pwd)

  # clean first: a fresh clone gives the sources and a committed binary
  # the same mtime, so a plain make can skip the build and we would copy
  # the stale binary (this happened with Listener/listener).
  cd $cur/../../Listener && make clean && make
  cd $cur/../../Monitor && make clean && make
  cd $cur/../../Balancer && make clean && make

  cd $cur
}

copy() {
  echo "[+] Copy bins"
  cp ../../Monitor/qidlecpu.ko ../bin/
  cp ../../Listener/listener ../bin/
  cp ../../Balancer/balancer ../bin/
}

verify() {
  echo "[+] Verify bins"
  md5sum ../../Monitor/qidlecpu.ko 
  md5sum ../bin/qidlecpu.ko

  md5sum ../../Listener/listener 
  md5sum ../bin/listener

  md5sum ../../Balancer/balancer
  md5sum ../bin/balancer
}

build
copy
verify
