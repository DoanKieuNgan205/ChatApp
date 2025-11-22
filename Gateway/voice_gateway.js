import dgram from "dgram";
import WebSocket, { WebSocketServer } from "ws";

const UDP_SERVER_PORT = 6060;  
const WS_PORT = 8080;          

const udpClient = dgram.createSocket("udp4");
const wss = new WebSocketServer({ port: WS_PORT });

let users = {}; 
let activeCalls = {}; 


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
            activeCalls[data.from] = data.to; 
            sendUDP(`CALL:${data.from}|TO:${data.to}`);
            break;

        case "ACCEPT_CALL":
            sendUDP(`ACCEPT_CALL:${data.to}|FROM:${data.from}`);
            break;

        case "REJECT_CALL":
            sendUDP(`REJECT_CALL:${data.to}|FROM:${data.from}`);
            break;

        case "AUDIO_DATA":
            const header = Buffer.from(`FROM:${data.from}|TO:${data.to}|`);
            const audioBuf = Buffer.from(Uint8Array.from(data.data));
            const packet = Buffer.concat([header, audioBuf]);
            udpClient.send(packet, UDP_SERVER_PORT, "127.0.0.1");
            break;
    }
}

udpClient.on("message", (msg) => {
    const text = msg.toString();

    if (text.startsWith("INCOMING_CALL:")) {
        
        const caller = text.split(":")[1];
        const target = activeCalls[caller]; 
        if (target && users[target] && users[target].readyState === WebSocket.OPEN) {
            users[target].send(JSON.stringify({ type: "INCOMING_CALL", from: caller }));
            console.log(`📞 Cuộc gọi từ ${caller} tới ${target}`);
        }
    }

    else if (text.startsWith("CALL_ACCEPTED:")) {
        
        const [fromPart, toPart] = text.split("|");
        const fromUser = fromPart.split(":")[1];
        const toUser = toPart.split(":")[1];
        sendToUser(toUser, { type: "CALL_ACCEPTED", from: fromUser });
        console.log(`✅ ${fromUser} chấp nhận cuộc gọi từ ${toUser}`);
    }

    else if (text.startsWith("CALL_REJECTED:")) {
        
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
        broadcast(jsonMsg); 
    }
});


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
