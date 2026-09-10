#!/bin/sh
# Compile and upload Nopeustesti to an Arduino Leonardo, then open the serial
# monitor. Usage: ./flash.sh [port]   (port is auto-detected if omitted)
set -e
cd "$(dirname "$0")"
FQBN=arduino:avr:leonardo
PORT=${1:-$(arduino-cli board list 2>/dev/null | awk '/arduino:avr:leonardo/ {print $1; exit}')}
if [ -z "$PORT" ]; then
  echo "No Leonardo found. Plug it in, or pass the port: ./flash.sh /dev/cu.usbmodemXXXX" >&2
  arduino-cli board list
  exit 1
fi
arduino-cli compile --fqbn "$FQBN" .
arduino-cli upload  --fqbn "$FQBN" --port "$PORT" .
echo "Uploaded. Opening serial monitor (Ctrl-C to quit)..."
sleep 2
arduino-cli monitor --port "$PORT" --config baudrate=115200
