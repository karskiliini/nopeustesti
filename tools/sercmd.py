#!/usr/bin/env python3
"""Send one command to the board and print what it replies.

Usage:
  tools/sercmd.py [-p PORT] COMMAND [SECONDS]

  COMMAND   text line to send ("" to only listen)
  SECONDS   how long to listen for replies (default 2)
  PORT      serial port; auto-detected (first /dev/cu.usbmodem*) if omitted

Meant for tools/pintest, but works with the game sketch too (it prints its
pin map whenever the port is opened). No pyserial needed.
"""
import glob, os, select, sys, termios, time

args = sys.argv[1:]
port = None
if args[:1] == ["-p"]:
    port = args[1]; args = args[2:]
if port is None:
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("no /dev/cu.usbmodem* port found; plug in the board or pass -p PORT")
    port = ports[0]
cmd = args[0] if args else ""
secs = float(args[1]) if len(args) > 1 else 2

fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
attr = termios.tcgetattr(fd)
attr[0] = 0; attr[1] = 0; attr[3] = 0                  # raw mode
attr[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
attr[4] = attr[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, attr)
time.sleep(0.3)
if cmd:
    os.write(fd, (cmd + "\n").encode())
end = time.time() + secs
while time.time() < end:
    r, _, _ = select.select([fd], [], [], 0.2)
    if r:
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            continue
        sys.stdout.write(chunk.decode(errors="replace"))
        sys.stdout.flush()
os.close(fd)
