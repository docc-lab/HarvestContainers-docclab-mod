#!/bin/bash

# CLOUDLAB: original 2-node c6420 setup (clabsvr/clabcl1 on 192.168.10.0/24)
# ICELAKE:  5-node Xeon Silver 4314 cluster from the dsb_k8s_perf harvest-k612
#           profile; clabsvr -> node-0, clabcl1 -> node-4 (see
#           Docs/07_icelake_hc_actuator_plan.md)
ENV_TYPE="ICELAKE"


if [ "${ENV_TYPE}" == "CLOUDLAB" ]
then
  WORKING_DIR="${HOME}/HarvestContainers/TestFramework"
  CPULIST="2,4,6,8,10,12,14,16"
  SECONDARY_CPULIST="18"
  MONITOR_BINDCPU="24"
  BALANCER_BINDCPU="26"
  LISTENER_BINDCPU="28"
  CPU_FREQ="2600000"
  SERVER_IP="192.168.10.10"
  CLIENT_IP="192.168.10.11"
fi

if [ "${ENV_TYPE}" == "ICELAKE" ]
then
  WORKING_DIR="${HOME}/HarvestContainers/TestFramework"
  # Assumes socket 1 = CPUs 16-31 with nosmt; check `lscpu -e` on the
  # harvest node and adjust. CPU 0 / socket 0 are left for housekeeping+IRQs.
  CPULIST="17,18,19,20,21,22,23,24"
  SECONDARY_CPULIST="25"
  MONITOR_BINDCPU="31"
  BALANCER_BINDCPU="30"
  LISTENER_BINDCPU="29"
  # Matches the 2.4 GHz frequency floor used in the dsb_hd_counter runs
  CPU_FREQ="2400000"
  SERVER_NODE="node-0"
  CLIENT_NODE="node-4"
  SERVER_IP=$(getent ahostsv4 ${SERVER_NODE} | awk 'NR==1 {print $1}')
  CLIENT_IP=$(getent ahostsv4 ${CLIENT_NODE} | awk 'NR==1 {print $1}')
  SERVER_IP=${SERVER_IP:-10.10.1.1}
  CLIENT_IP=${CLIENT_IP:-10.10.1.5}
fi

MIN_SECONDARY_CORES="0"
TARGET_IDLE_CORES="1"

BIN_PATH="${WORKING_DIR}/bin"
BALANCER_BINARY="balancer"
IDLECPU_BINARY="qidlecpu.ko"
LOGGER_BINARY="cpulogger.ko"
LISTENER_BINARY="listener"

BULLY_CNTR="cpubully"
LS_CNTR="ls"
TERASORT_CNTR="terasort"

EXPERIMENT_DIR="${WORKING_DIR}/Experiments"
