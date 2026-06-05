import socket
import struct
import time
import random
import threading

MULTICAST_GROUP = ('239.255.0.1', 9000)
TCP_PORT = 9999

# --- TCP Order Receiver (The Exchange matching engine simulator) ---
def order_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('127.0.0.1', TCP_PORT))
    server.listen(1)
    print(f"[Exchange] TCP Server listening for orders on port {TCP_PORT}...")
    
    while True:
        try:
            conn, addr = server.accept()
            print(f"[Exchange] Accepted connection from Gateway: {addr}")
            while True:
                # Our C++ OrderRequest struct is exactly 24 bytes (uint64, double, uint32, int32)
                data = conn.recv(24)
                if not data:
                    break
                
                if len(data) == 24:
                    order_id, price, inst_id, qty = struct.unpack('=Q d I i', data)
                    side = "BUY" if qty > 0 else "SELL"
                    print(f"   => [MATCHED] {side} {abs(qty)} shares @ ${price:.2f} (OrderID: {order_id})")
        except Exception as e:
            print(f"[Exchange] Connection error: {e}")

# Start the TCP server in a background thread so it runs concurrently with UDP
threading.Thread(target=order_server, daemon=True).start()


# --- UDP Market Data Publisher ---
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, struct.pack('b', 1))

print(f"[Exchange] Blasting UDP market data to {MULTICAST_GROUP}...")

price = 100.0
instrument_id = 1
volume = 100

try:
    while True:
        timestamp = int(time.time() * 1e9) 
        price += random.uniform(-0.5, 0.5) 
        
        payload = struct.pack('=Q d I I', timestamp, price, instrument_id, volume)
        sock.sendto(payload, MULTICAST_GROUP)
        
        # We'll print a smaller tick indicator to keep the console readable
        print(f"Sent Tick: ${price:.2f}", end='\r') 
        
        time.sleep(0.1) 

except KeyboardInterrupt:
    print("\n[Exchange] Shut down.")
    sock.close()