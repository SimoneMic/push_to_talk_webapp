# Generate web config
import socket
import os
import asyncio
import websockets

# Reliable LAN IP detection
def get_lan_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    finally:
        s.close()

ip = get_lan_ip()
ws_port = int(os.getenv("WS_PORT", "8000"))

ws_url = f"ws://{ip}:{ws_port}"
print(f"Generating config.js with WS_URL = {ws_url}")

with open("../webpage/config.js", "w") as f:
    f.write(f'window.APP_CONFIG = {{ WS_URL: "{ws_url}" }};')

# WebSocket server handler
async def handler(websocket):
    print("Client connected")
    with open("received_audio.webm", "wb") as f:
        async for message in websocket:
            f.write(message)

# Start server on all interfaces
async def main():
    async with websockets.serve(handler, "0.0.0.0", ws_port):
        print(f"Server listening on ws://0.0.0.0:{ws_port} (LAN IP: {ip})")
        await asyncio.Future()

asyncio.run(main())