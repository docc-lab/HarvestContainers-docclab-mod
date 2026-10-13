#!/bin/bash

source ../Config/SYSTEM.sh

echo "[+] Scaling driver: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver)"
echo "[+] Disabling turbo boost"
# Disable turbo boost for intel-pstate driver
if [ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
  echo "1" | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
fi
# Disable turbo boost for acpi-cpufreq scaling driver
if [ -f /sys/devices/system/cpu/cpufreq/boost ]; then
  echo "0" | sudo tee /sys/devices/system/cpu/cpufreq/boost
fi
sleep 1
# 'userspace' is only offered by passive drivers (intel_cpufreq, acpi-cpufreq).
# intel_pstate in active mode only offers performance/powersave; there the
# frequency is pinned by setting scaling_min_freq == scaling_max_freq.
if grep -qw userspace /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors; then
  GOVERNOR="userspace"
else
  GOVERNOR="performance"
fi
echo "[+] Setting power governor to '${GOVERNOR}'"
echo "${GOVERNOR}" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
sleep 1
# Set max freq first if lowering, min first if raising, so min <= max always holds
CUR_MIN=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq)
if [ "${CPU_FREQ}" -ge "${CUR_MIN}" ]; then
  echo "[+] Setting MAX frequency to ${CPU_FREQ} kHz"
  echo "${CPU_FREQ}" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq
  echo "[+] Setting MIN frequency to ${CPU_FREQ} kHz"
  echo "${CPU_FREQ}" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq
else
  echo "[+] Setting MIN frequency to ${CPU_FREQ} kHz"
  echo "${CPU_FREQ}" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq
  echo "[+] Setting MAX frequency to ${CPU_FREQ} kHz"
  echo "${CPU_FREQ}" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq
fi
sleep 2
# Disable ondemand.service
echo "[+] Disabling 'ondemand' service"
sudo systemctl disable ondemand
# Check freqs
echo "[+] Getting current CPU freq stats (kHz)"
grep . /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq | sort -t: -k2 -n | awk -F: '{print $2}' | uniq -c
