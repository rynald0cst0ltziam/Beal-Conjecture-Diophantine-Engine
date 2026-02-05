#!/bin/bash
# 🏛️ PROJECT GOLIATH - SINGLE TARGET RUNNER (300K SPRINT)
# Usage: bash goliath.sh X Y Z
# Uses: Hyper-Goliath C/AVX2 Engine

set -e

if [ "$#" -ne 3 ]; then
    echo "Usage: bash goliath.sh <x> <y> <z>"
    echo "Example: bash goliath.sh 4 5 6"
    exit 1
fi

X=$1
Y=$2
Z=$3

# Ensure we're in the right directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BINARY="$SCRIPT_DIR/hyper_goliath/build/hyper_goliath"

if [ ! -f "$BINARY" ]; then
    echo "Error: Hyper-Goliath binary not found at $BINARY"
    echo "Run: cd hyper_goliath && ./scripts/build.sh"
    exit 1
fi

# Create logs directory
mkdir -p "$SCRIPT_DIR/logs"

TIMESTAMP=$(date +%s)
LOG_FILE="$SCRIPT_DIR/logs/goliath_${X}_${Y}_${Z}_${TIMESTAMP}.jsonl"

echo "===================================================="
echo "    PROJECT GOLIATH - MISSION: ($X, $Y, $Z)"
echo "===================================================="
echo "Engine:    Hyper-Goliath C/AVX2"
echo "Range:     300,000 Bases"
echo "Log File:  $LOG_FILE"
echo "Started:   $(date)"
echo "----------------------------------------------------"

"$BINARY" \
    --x $X --y $Y --z $Z \
    --Amax 300000 --Bmax 300000 \
    --Cmax 1000000000 \
    --log "$LOG_FILE"

echo "===================================================="
echo "    MISSION COMPLETE: ($X, $Y, $Z)"
echo "===================================================="
echo "Completed: $(date)"
echo "Log: $LOG_FILE"
