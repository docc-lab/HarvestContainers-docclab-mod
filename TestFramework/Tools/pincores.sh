#!/usr/bin/env bash

# Usage: ./pincores.sh <cpuset> <node> <app>
# e.g. ./pincores.sh 0-7 clabcl0 xapian-primary

CPUSET=$1 #format = "low-high" i.e. low,high are inclusive
NODE=$2 # where container is running
MASTER="clabsvr" # k8s control-plane alias (node-0 on the Ice Lake cluster)
APP=$3

if [ -z $APP ]; then
  echo "[!] Usage: ./pincores.sh <cpuset> <node> <app>"
  echo "[!] e.g. ./pincores.sh 0-7 clabcl0 xapian-primary"
  exit 1
fi

PODFILE=${APP}_podinfo.txt
rm -f $PODFILE
if [ ! -f $PODFILE ]; then
  bash get_pod_info.sh $MASTER $APP
  sz=$(wc -c $PODFILE | awk '{print $1}')
  echo $sz
  if [ $sz -eq 1 ]; then
    echo "[!] error retrieving pod info for $APP."
    exit 1;
  fi
fi

POD_ID="pod$(cat $PODFILE | jq -r .metadata.uid)"
# Strip the runtime prefix (docker://, containerd://, cri-o://)
CONTAINER_ID=$(cat $PODFILE | jq -r .status.containerStatuses[0].containerID | sed 's|.*://||')

if [ ! -z $CPUSET ]; then
  echo "[+] Retrieve container info from master"

  echo "[+] Modify cpuset of container"
  # Locate the container cgroup: the cgroup v1 cpuset hierarchy, or cgroup v2
  # with either the cgroupfs (pod<uid>/<id>) or systemd
  # (pod<uid_>.slice/<runtime>-<id>.scope) driver
  ssh $NODE bash <<EOF
  POD_US=\$(echo ${POD_ID} | tr '-' '_')
  CTR_DIR=""
  for root in /sys/fs/cgroup/cpuset/kubepods /sys/fs/cgroup/kubepods /sys/fs/cgroup/kubepods.slice; do
    [ -d \$root ] || continue
    POD_DIR=\$(find \$root -maxdepth 4 -type d \( -name "*${POD_ID}*" -o -name "*\${POD_US}*" \) | head -n1)
    [ -n "\$POD_DIR" ] || continue
    CTR_DIR=\$(find \$POD_DIR -maxdepth 1 -type d -name "*${CONTAINER_ID}*" | head -n1)
    [ -n "\$CTR_DIR" ] && break
  done
  if [ -z "\$CTR_DIR" ]; then
    echo "[ERR]: cgroup for ${POD_ID}/${CONTAINER_ID} not found"
    exit 1
  fi
  echo $CPUSET | sudo tee \$CTR_DIR/cpuset.cpus
EOF

else
  echo "[ERR]: cpuset empty"
fi
