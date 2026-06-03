#!/bin/bash
# 为小内存 ECS 添加 2GB swap（幂等）
set -e
SWAP_FILE=/swapfile
SWAP_GB="${SWAP_GB:-2}"

if swapon --show | grep -q "$SWAP_FILE"; then
  echo "swap already active: $SWAP_FILE"
  swapon --show
  exit 0
fi

if [ -f "$SWAP_FILE" ]; then
  chmod 600 "$SWAP_FILE"
  mkswap "$SWAP_FILE" 2>/dev/null || true
  swapon "$SWAP_FILE"
else
  fallocate -l "${SWAP_GB}G" "$SWAP_FILE" 2>/dev/null || dd if=/dev/zero of="$SWAP_FILE" bs=1M count=$((SWAP_GB * 1024)) status=progress
  chmod 600 "$SWAP_FILE"
  mkswap "$SWAP_FILE"
  swapon "$SWAP_FILE"
fi

grep -q "$SWAP_FILE" /etc/fstab 2>/dev/null || echo "$SWAP_FILE none swap sw 0 0" >> /etc/fstab
echo "vm.swappiness=10" > /etc/sysctl.d/99-tracker-swap.conf
sysctl -p /etc/sysctl.d/99-tracker-swap.conf 2>/dev/null || true
swapon --show
free -h
