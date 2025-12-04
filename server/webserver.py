import socket
import os
import asyncio
import yarp
import websockets
import av
import numpy as np

yarp.Network.init()

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


# YARP audio port
yarp_port = yarp.BufferedPortSound()
yarp_port.open("/webserver/audio:o")

if not yarp.Network.connect("/webserver/audio:o",
                            "/SpeechTranscription_nws/audio:i"):
    print("Warning: could not connect YARP ports.")


# ---------- AUDIO DECODER (WebM/Opus → PCM) ----------
class StreamingDecoder:
    """Incrementally decodes WebM/Opus chunks into PCM frames."""
    def __init__(self):
        self.codec = av.CodecContext.create("opus", "r")

    def decode_chunk(self, chunk: bytes):
        """Decodes a raw WebM/Opus chunk and yields PCM frames (float32)."""
        pkt = av.Packet(chunk)
        frames = self.codec.decode(pkt)
        # Each frame is already PCM float samples
        return frames


decoder = StreamingDecoder()


# ---------- WEBSOCKET HANDLER ----------
async def handler(websocket):
    print("Client connected")

    async for message in websocket:

        # Browser starts a new recording
        if isinstance(message, str) and message == "__NEW_RECORDING__":
            print("New recording session started")
            continue

        # Browser ends recording
        if isinstance(message, str) and message == "__STOP_RECORDING__":
            print("Recording stopped")
            continue

        # Binary → decode → send to YARP
        if isinstance(message, bytes):
            data = np.frombuffer(message, dtype=np.float32)
            num_samples = len(data)

            sound = yarp_port.prepare()
            sound.resize(num_samples, 1)

            for i in range(num_samples):
                pcm = int(max(-1, min(1, data[i])) * 32767)
                sound.set(pcm, i, 0)

            yarp_port.write()



# ---------- WEBSOCKET SERVER ----------
async def main():
    async with websockets.serve(handler, "0.0.0.0", ws_port):
        print(f"Server running on wss://0.0.0.0:{ws_port} (LAN {ip})")
        await asyncio.Future()

asyncio.run(main())
