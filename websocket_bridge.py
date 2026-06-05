import asyncio
import websockets
import socket
import struct
import json

UDP_IP = "127.0.0.1"
UDP_PORT = 8080
WS_PORT = 8765

# Connected web clients
connected_clients = set()

async def udp_listener():
    loop = asyncio.get_running_loop()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    sock.setblocking(False)

    print(f"[Bridge] Listening for C++ UDP telemetry on {UDP_PORT}...")

    while True:
        try:
            # Receive the 40-byte struct from C++
            data, _ = await loop.sock_recv(sock, 1024)
            if len(data) == 40:
                # Unpack: Q (uint64), d (double), d (double), i (int32), i (int32), d (double)
                ts, price, delta, pos, pnl, latency = struct.unpack('=Q d d i i d', data)
                
                # Format as JSON for the browser
                payload = json.dumps({
                    "timestamp": ts,
                    "price": round(price, 2),
                    "delta": round(delta, 3),
                    "position": pos,
                    "pnl": pnl,
                    "latency": round(latency, 2)
                })

                # Broadcast to all connected web dashboards
                if connected_clients:
                    websockets.broadcast(connected_clients, payload)

        except Exception as e:
            print(f"Error parsing UDP: {e}")
            await asyncio.sleep(0.01)

async def ws_handler(websocket):
    print("[Bridge] Dashboard UI connected!")
    connected_clients.add(websocket)
    try:
        await websocket.wait_closed()
    finally:
        connected_clients.remove(websocket)
        print("[Bridge] Dashboard UI disconnected.")

async def main():
    # Start the WebSocket server
    async with websockets.serve(ws_handler, "localhost", WS_PORT):
        print(f"[Bridge] WebSocket server running on ws://localhost:{WS_PORT}")
        # Run the UDP listener concurrently
        await udp_listener()

if __name__ == "__main__":
    asyncio.run(main())