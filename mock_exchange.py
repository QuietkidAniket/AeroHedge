import socket
import struct
import time
import random

MULTICAST_GROUP = ('239.255.0.1', 9000)

# Create the UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# Set the time-to-live for messages to 1 so they do not go past the local network segment
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, struct.pack('b', 1))

print(f"Blasting market data to {MULTICAST_GROUP}...")

price = 100.0
instrument_id = 1
volume = 100

try:
    while True:
        timestamp = int(time.time() * 1e9) # Nanoseconds
        price += random.uniform(-0.5, 0.5) # Random walk
        
        # Struct Pack Format:
        # ! = Network Byte Order (Big-Endian) - *Wait, actually let's use standard native (=) 
        # because our C++ is doing a raw memory cast without ntoh/hton conversions for speed.
        # Q = uint64_t (8 bytes)
        # d = double (8 bytes)
        # I = uint32_t (4 bytes)
        # I = uint32_t (4 bytes)
        # Total = 24 bytes
        
        payload = struct.pack('=Q d I I', timestamp, price, instrument_id, volume)
        
        sock.sendto(payload, MULTICAST_GROUP)
        print(f"Sent: Tick(Price={price:.2f})")
        
        # Send a tick every 100 milliseconds
        time.sleep(0.1) 

except KeyboardInterrupt:
    print("\nExchange shut down.")
    sock.close()