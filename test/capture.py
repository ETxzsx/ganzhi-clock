import sys, time, serial
port = sys.argv[1] if len(sys.argv) > 1 else 'COM9'
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 12
s = serial.Serial(port, 115200, timeout=0.2)
s.dtr = False
s.rts = True
time.sleep(0.15)
s.rts = False
s.reset_input_buffer()
end = time.time() + secs
buf = b''
while time.time() < end:
    buf += s.read(4096)
s.close()
sys.stdout.buffer.write(buf)