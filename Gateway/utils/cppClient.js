/*import net from "net";
import dgram from "dgram";
import { CONFIG } from "./config.js";

const udpSocket = dgram.createSocket("udp4");

export function createTcpClient(port = CONFIG.CHAT_PORT) {
  const client = new net.Socket();
  client.connect(port, CONFIG.TCP_HOST, () => {
    console.log(`[TCP] ✅ Connected to C++ server ${CONFIG.TCP_HOST}:${port}`);
  });
  client.on("error", err => console.error("[TCP] ❌", err.message));
  return client;
}

export function createLoginClient() {
  return createTcpClient(CONFIG.LOGIN_PORT);
}

export function sendUdpVoice(from, to, audioBuffer) {
  const header = `FROM:${from}|TO:${to}|`;
  const packet = Buffer.concat([Buffer.from(header), audioBuffer]);
  udpSocket.send(packet, CONFIG.UDP_PORT_CPP, CONFIG.TCP_HOST, err => {
    if (err) console.error("[UDP] ⚠️ Send error:", err.message);
  });
}

export function initUdpListener(handleVoicePacket) {
  udpSocket.on("message", handleVoicePacket);
  udpSocket.bind(CONFIG.UDP_PORT_GATEWAY, () => {
    console.log(`[UDP] 🎧 Gateway listening on ${CONFIG.UDP_PORT_GATEWAY}`);
  });
}*/
