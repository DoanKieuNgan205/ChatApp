// voice_gateway.js - Node.js trung gian giữa Web UI và C++ VoiceServer

import dgram from "dgram";
import WebSocket, { WebSocketServer } from "ws";

const UDP_SERVER_PORT = 6060;  // Cổng UDP VoiceServer.cpp
const WS_PORT = 8080;          // Cổng WebSocket cho frontend

const udpClient = dgram.createSocket("udp4");
const wss = new WebSocketServer({ port: WS_PORT });

let users = {}; // username -> ws
let activeCalls = {}; // from -> to


wss.on("connection", (ws) => {
    console.log("🌐 Web client connected.");

    ws.on("message", (msg) => {
        try {
            const data = JSON.parse(msg.toString());
            handleWebMessage(ws, data);
        } catch (e) {
            console.error("❌ Lỗi parse JSON:", e);
        }
    });

    ws.on("close", () => console.log("🔌 Web client disconnected."));
});

function handleWebMessage(ws, data) {
    switch (data.type) {
    
        case "REGISTER":
            users[data.username] = ws;
            sendUDP(`REGISTER:${data.username}`);
            break;

        case "CALL":
            activeCalls[data.from] = data.to; // lưu cặp cuộc gọi
            sendUDP(`CALL:${data.from}|TO:${data.to}`);
            break;

        case "ACCEPT_CALL":
            sendUDP(`ACCEPT_CALL:${data.to}|FROM:${data.from}`);
            break;

        case "REJECT_CALL":
            sendUDP(`REJECT_CALL:${data.to}|FROM:${data.from}`);
            break;

        case "AUDIO_DATA":
            // Gói dữ liệu nhị phân gửi sang C++ server
            const header = Buffer.from(`FROM:${data.from}|TO:${data.to}|`);
            const audioBuf = Buffer.from(Uint8Array.from(data.data));
            const packet = Buffer.concat([header, audioBuf]);
            udpClient.send(packet, UDP_SERVER_PORT, "127.0.0.1");
            break;
    }
}

// === Nhận dữ liệu từ C++ VoiceServer ===
/*udpClient.on("message", (msg) => {
    const text = msg.toString();

    if (text.startsWith("INCOMING_CALL:")) {
        const caller = text.split(":")[1];
        broadcast({ type: "INCOMING_CALL", from: caller });
    } else if (text.startsWith("CALL_ACCEPTED:")) {
        const user = text.split(":")[1];
        broadcast({ type: "CALL_ACCEPTED", from: user });
    } else if (text.startsWith("CALL_REJECTED:")) {
        const user = text.split(":")[1];
        broadcast({ type: "CALL_REJECTED", from: user });
    } else if (text.startsWith("FROM:")) {
        // FROM:<name>|DATA|<binary>
        const headerEnd = text.indexOf("|DATA|");
        const fromUser = text.substring(5, headerEnd);
        const headerLen = headerEnd + 6;
        const audioData = msg.slice(headerLen);
        broadcast({ type: "AUDIO_DATA", from: fromUser, data: Array.from(audioData) });
    }
});*/


udpClient.on("message", (msg) => {
    const text = msg.toString();

    if (text.startsWith("INCOMING_CALL:")) {
        // INCOMING_CALL:<caller>|TO:<target>
        const caller = text.split(":")[1];
        const target = activeCalls[caller]; // người bị gọi
        if (target && users[target] && users[target].readyState === WebSocket.OPEN) {
            users[target].send(JSON.stringify({ type: "INCOMING_CALL", from: caller }));
            console.log(`📞 Cuộc gọi từ ${caller} tới ${target}`);
        }
    }

    else if (text.startsWith("CALL_ACCEPTED:")) {
        // CALL_ACCEPTED:<user>|TO:<caller>
        const [fromPart, toPart] = text.split("|");
        const fromUser = fromPart.split(":")[1];
        const toUser = toPart.split(":")[1];
        sendToUser(toUser, { type: "CALL_ACCEPTED", from: fromUser });
        console.log(`✅ ${fromUser} chấp nhận cuộc gọi từ ${toUser}`);
    }

    else if (text.startsWith("CALL_REJECTED:")) {
        // CALL_REJECTED:<user>|TO:<caller>
        const [fromPart, toPart] = text.split("|");
        const fromUser = fromPart.split(":")[1];
        const toUser = toPart.split(":")[1];
        sendToUser(toUser, { type: "CALL_REJECTED", from: fromUser });
        console.log(`❌ ${fromUser} từ chối cuộc gọi từ ${toUser}`);
    }

    else if (text.startsWith("FROM:")) {
        const headerEnd = text.indexOf("|DATA|");
        const fromUser = text.substring(5, headerEnd);
        const headerLen = headerEnd + 6;
        const audioData = msg.slice(headerLen);
        const jsonMsg = { type: "AUDIO_DATA", from: fromUser, data: Array.from(audioData) };
        broadcast(jsonMsg); // vẫn broadcast để cả hai bên nghe được
    }
});

// Gửi riêng cho 1 user
function sendToUser(username, message) {
    const ws = users[username];
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(message));
    }
}


function broadcast(message) {
    const msgStr = JSON.stringify(message);
    for (const user in users) {
        if (users[user].readyState === WebSocket.OPEN) {
            users[user].send(msgStr);
        }
    }
}

function sendUDP(text) {
    const buf = Buffer.from(text);
    udpClient.send(buf, UDP_SERVER_PORT, "127.0.0.1");
}

console.log(`🚀 Voice Gateway WebSocket đang chạy tại ws://localhost:${WS_PORT}`);
