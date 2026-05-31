#!/bin/bash
# Convenience script to update the current FPS value that the daemon reads.

FPS_FILE="${FPS_FILE:-/run/hwmon-daemon/current_fps}"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <fps>"
    echo "Example: $0 144"
    exit 1
fi

sudo mkdir -p "$(dirname "$FPS_FILE")"
echo "$1" | sudo tee "$FPS_FILE" > /dev/null
echo "Set FPS to $1 (file: $FPS_FILE)"
