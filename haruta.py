import socket

# minimal osc message builder
def make_osc(address):
    # address null-padded to multiple of 4
    buf = address.encode('utf-8') + b'\x00'
    while len(buf) % 4 != 0:
        buf += b'\x00'
    # typetag ',' null-padded to multiple of 4
    buf += b',\x00\x00\x00'
    return buf

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
dest = ("127.0.0.1", 8000)

print("rewinding to 0:00 and playing...")
# Action 1016:  Transport: Stop (halts playback if running)
# Action 40042: Transport: Go to start of project
# Action 1007:  Transport: Play (unconditional playback start)
sock.sendto(make_osc("/action/1016"), dest)
sock.sendto(make_osc("/action/40042"), dest)
sock.sendto(make_osc("/action/1007"), dest)