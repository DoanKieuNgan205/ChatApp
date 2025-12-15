/*import WebSocket, { WebSocketServer } from "ws";
import net from "net";
import fs from "fs";
import express from "express";
import multer from "multer";
import cors from "cors";
import dgram from "dgram";
import { fileURLToPath } from 'url';
import path from 'path';
import https from 'https';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const userTcpConnections = {}; 

const privateKey = fs.readFileSync(path.join(__dirname, 'key.pem'), 'utf8');
const certificate = fs.readFileSync(path.join(__dirname, 'cert.pem'), 'utf8');

const credentials = { key: privateKey, cert: certificate };

const WS_PORT = 3000; 
const HTTP_PORT = 3001; 
const TCP_HOST = "10.152.147.186";
const TCP_PORT_CHAT = 8888; 
const TCP_PORT_FILE = 9999; 
const UDP_PORT_VOICE = 6060; 
const UDP_HOST_CPP = "10.152.147.186"; 
const UDP_PORT_GATEWAY = 6061; 

const wss_https_server = https.createServer(credentials);
const wss = new WebSocketServer({ server: wss_https_server });

wss_https_server.listen(WS_PORT, () => {
    console.log(`[Gateway] 🔒 WebSocket Server (Chat+Voice) lắng nghe tại wss://10.152.147.186:${WS_PORT}`);
});

const app = express();
app.use(cors());
app.use(express.json({ limit: "50mb", type: "application/json; charset=utf-8" }));
app.use(express.urlencoded({ extended: true, limit: "50mb" }));
app.use(express.static(path.join(__dirname, '..', 'Frontend')));

const udpClient = dgram.createSocket("udp4"); 
const clientsOnline = {}; 
const userHistories = {}; 
const voiceClients = {}; 
const chatHistory = {}; 
const activeCalls = {}; 

function broadcastUserList() {
  const users = Object.keys(clientsOnline);
  const payload = JSON.stringify({ action: "online_list", users });
  for (const u in clientsOnline) {
    clientsOnline[u].send(payload);
  }
}

function sendToUser(username, message, clientMap) {
  const ws = clientMap[username]; 
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(message));
  }
}


function sendUDP(text) {
  const buf = Buffer.from(text);
  udpClient.send(buf, UDP_PORT_VOICE, UDP_HOST_CPP);
}

udpClient.on("message", (msg) => {
  
  const incCallMarker = Buffer.from("INCOMING_CALL:");
  const callAcceptedMarker = Buffer.from("CALL_ACCEPTED:");
  const callRejectedMarker = Buffer.from("CALL_REJECTED:");
  const dataMarker = Buffer.from("|DATA|");

  
  if (msg.indexOf(incCallMarker) === 0) {
    const text = msg.toString();
    const [callPart, toPart] = text.split("|TO:");
    const caller = callPart.split(":")[1];
    const callee = toPart;
    sendToUser(callee, { action: "INCOMING_CALL", from: caller }, clientsOnline);
    console.log(`[Gateway] 📞 Cuộc gọi từ ${caller} tới ${callee}`);
    return;
  }

  
  if (msg.indexOf(callAcceptedMarker) === 0) {
    const text = msg.toString();
    const [fromPart, toPart] = text.split("|TO:");
    const fromUser = fromPart.split(":")[1];
    const toUser = toPart;
    sendToUser(toUser, { action: "CALL_ACCEPTED", from: fromUser }, voiceClients);
    console.log(`[Gateway] ✅ ${fromUser} chấp nhận cuộc gọi từ ${toUser}`);
    return;
  }

  
  if (msg.indexOf(callRejectedMarker) === 0) {
    const text = msg.toString();
    const [fromPart, toPart] = text.split("|TO:");
    const fromUser = fromPart.split(":")[1];
    const toUser = toPart;
    sendToUser(toUser, { action: "CALL_REJECTED", from: fromUser }, voiceClients);
    console.log(`[Gateway] ❌ ${fromUser} từ chối cuộc gọi từ ${toUser}`);
    return;
  }

  
  const idx = msg.indexOf(dataMarker);
  if (idx !== -1) {
    const header = msg.slice(0, idx).toString(); 
    const fromUser = header.split(":")[1];
    const audioBuf = msg.slice(idx + dataMarker.length); 

    const toUser = activeCalls[fromUser];
    if (toUser) {
      const jsonMsg = {
        action: "AUDIO_DATA",
        from: fromUser,
        data: Array.from(audioBuf), 
      };
      sendToUser(fromUser, jsonMsg, voiceClients);
      sendToUser(toUser, jsonMsg, voiceClients);
    }
    return;
  }

  console.warn("[Gateway] ⚠️ UDP message không nhận diện:", msg.toString().slice(0, 200));
});


wss.on("connection", (ws) => {
  console.log("[Gateway] ✅ Frontend kết nối mới (WS 3000)");

  let tcpClient = null;
  let currentUsername = null;

  ws.on("message", (msg) => {
    try {
      const data = JSON.parse(msg.toString());
      console.log("[Gateway] ⬇️ Nhận từ Web:", data.action);

      switch (data.action) {
        case "login":
          currentUsername = data.username;
          ws.username = data.username;
          
          // Kiểm tra xem đã có TCP connection cho user này chưa
          if (userTcpConnections[data.username] && !userTcpConnections[data.username].destroyed) {
            console.log(`[Gateway] ♻️ Tái sử dụng TCP connection cho ${data.username}`);
            tcpClient = userTcpConnections[data.username];
            
            // Gửi login request qua connection cũ
            tcpClient.write(JSON.stringify(data));
          } else {
            // Tạo TCP connection mới
            console.log(`[Gateway] 🔌 Tạo TCP connection mới cho ${data.username}`);
            tcpClient = new net.Socket();
            
            tcpClient.connect(TCP_PORT_CHAT, TCP_HOST, () => {
              console.log(`[Gateway] 🔌 Đã kết nối tới Server C++ Chat (${TCP_PORT_CHAT})`);
              tcpClient.write(JSON.stringify(data));
            });

            // Lưu connection
            userTcpConnections[data.username] = tcpClient;

            // Setup data handler cho TCP connection
            let tcpBuffer = "";
            tcpClient.on("data", (chunk) => {
              tcpBuffer += chunk.toString();
              let boundary = tcpBuffer.indexOf('\n');
              
              while (boundary !== -1) {
                const message = tcpBuffer.substring(0, boundary).trim();
                tcpBuffer = tcpBuffer.substring(boundary + 1);
                
                if (message) {
                  console.log("[Gateway] 📩 Nhận từ C++:", message.substring(0, 200));
                  
                  try {
                    const parsed = JSON.parse(message);
                    
                    // Forward tới WebSocket client
                    if (ws.readyState === WebSocket.OPEN) {
                      ws.send(JSON.stringify(parsed));
                    }

                    if (parsed.action === "login_response" && parsed.status === "success") {
                      console.log(`[Gateway] ✅ Đăng nhập thành công: ${parsed.username}`);
                    }

                    if (parsed.action === "history_response") {
                      console.log(`[Gateway] 🗂️ Nhận lịch sử: ${parsed.chatHistory?.length || 0} messages`);
                    }
                  } catch (e) {
                    console.warn("[Gateway] ⚠️ Parse error:", e.message);
                  }
                }
                boundary = tcpBuffer.indexOf('\n');
              }
            });

            tcpClient.on("error", (err) => {
              console.error("[Gateway] ❌ TCP error:", err.message);
              delete userTcpConnections[data.username];
            });

            tcpClient.on("close", () => {
              console.log(`[Gateway] 🔌 TCP connection closed for ${data.username}`);
              if (userTcpConnections[data.username] === tcpClient) {
                delete userTcpConnections[data.username];
              }
            });
          }
          break;

        case "register":
          if (!tcpClient || tcpClient.destroyed) {
            tcpClient = new net.Socket();
            tcpClient.connect(TCP_PORT_CHAT, TCP_HOST, () => {
              tcpClient.write(JSON.stringify(data));
            });
          } else {
            tcpClient.write(JSON.stringify(data));
          }
          break;

        case "join_chat":
          currentUsername = data.username;
          clientsOnline[data.username] = ws;
          ws.username = data.username;
          ws.type = 'chat';
          console.log(`[Gateway] 👤 ${data.username} đã tham gia chat`);
          
          // Lấy TCP connection đã login
          tcpClient = userTcpConnections[data.username];
          if (!tcpClient || tcpClient.destroyed) {
            console.error(`[Gateway] ❌ KHÔNG TÌM THẤY TCP CONNECTION cho ${data.username}!`);
          } else {
            console.log(`[Gateway] ✅ Đã map WebSocket → TCP connection của ${data.username}`);
          }
          
          broadcastUserList();
          sendUDP(`REGISTER:${data.username}`);
          console.log(`[Gateway] 🎤 Đã đăng ký UDP cho ${data.username}`);
          break;

        case "list":
          if (!tcpClient || tcpClient.destroyed) {
            tcpClient = userTcpConnections[ws.username];
          }
          if (tcpClient && !tcpClient.destroyed) {
            tcpClient.write(JSON.stringify(data) + "\n");
          } else {
            console.error(`[Gateway] ❌ Không có TCP connection để gửi 'list'`);
          }
          break;

        case "get_history":
          const user1 = data.user1 || data.from || ws.username;
          const user2 = data.user2 || data.to;

          if (user1 && user2) {
            console.log(`[Gateway] 🗂️ Web yêu cầu lịch sử: ${user1} <-> ${user2}`);

            // Lấy TCP connection của user hiện tại
            if (!tcpClient || tcpClient.destroyed) {
              tcpClient = userTcpConnections[ws.username];
            }

            if (!tcpClient || tcpClient.destroyed) {
              console.error(`[Gateway] ❌ KHÔNG CÓ TCP CONNECTION cho ${ws.username}!`);
              ws.send(JSON.stringify({
                action: "error",
                message: "Mất kết nối đến server. Vui lòng đăng nhập lại."
              }));
            } else {
              const historyRequest = JSON.stringify({ 
                action: "get_history",
                user1: user1,
                user2: user2
              }) + "\n";
              
              console.log(`[Gateway] 📤 Gửi qua TCP của ${ws.username}:`, historyRequest.trim());
              tcpClient.write(historyRequest);
            }
          } else {
            console.warn(`[Gateway] ⚠️ get_history thiếu user1 hoặc user2`);
          }
          break;

        case "REGISTER_VOICE":
          voiceClients[data.username] = ws;
          ws.username = data.username;
          ws.type = 'voice';
          console.log(`[Gateway] 🎙️ ${data.username} đã kết nối voice`);
          sendUDP(`REGISTER:${data.username}`);
          break;

        case "private":
          if (data.message) {
            sendToUser(data.to, {
              action: "private",
              from: data.from,
              message: data.message
            }, clientsOnline);

            // Lưu vào database
            if (!tcpClient || tcpClient.destroyed) {
              tcpClient = userTcpConnections[ws.username];
            }
            
            if (tcpClient && !tcpClient.destroyed) {
              const saveMsg = JSON.stringify({
                action: "save_private_message",
                from: data.from,
                to: data.to,
                message: data.message
              }) + "\n";
              tcpClient.write(saveMsg);
              console.log(`[Gateway] 💾 Lưu tin nhắn: ${data.from} → ${data.to}`);
            }
          }
          break;

        case "CALL":
          activeCalls[data.from] = data.to;
          sendUDP(`CALL:${data.from}|TO:${data.to}`);
          console.log(`[Gateway] 📞 ${data.from} gọi ${data.to}`);
          break;

        case "ACCEPT_CALL":
          sendUDP(`ACCEPT_CALL:${data.to}|FROM:${data.from}`);
          activeCalls[data.from] = data.to;
          activeCalls[data.to] = data.from;
          console.log(`[Gateway] ✅ ${data.to} chấp nhận ${data.from}`);
          break;

        case "REJECT_CALL":
          sendUDP(`REJECT_CALL:${data.to}|FROM:${data.from}`);
          delete activeCalls[data.from];
          console.log(`[Gateway] ❌ ${data.to} từ chối ${data.from}`);
          break;

        case "AUDIO_DATA":
          const header = Buffer.from(`FROM:${data.from}|TO:${data.to}|DATA|`);
          const audioBuf = Buffer.from(Uint8Array.from(data.data));
          const packet = Buffer.concat([header, audioBuf]);
          udpClient.send(packet, UDP_PORT_VOICE, UDP_HOST_CPP);
          break;

        default:
          console.warn("[Gateway] ⚠️ Action không xác định:", data.action);
      }
    } catch (err) {
      console.error("[Gateway] ❌ Error:", err.message);
    }
  });

  ws.on("close", () => {
    console.log(`[Gateway] 📴 ${ws.username || 'Client'} ngắt kết nối`);

    if (ws.username) {
      if (ws.type === 'chat') {
        delete clientsOnline[ws.username];
        broadcastUserList();
      } else if (ws.type === 'voice') {
        delete voiceClients[ws.username];
      }

      sendUDP(`UNREGISTER:${ws.username}`);

      // KHÔNG đóng TCP connection ngay - để reconnect dùng lại
      // Sẽ tự đóng sau timeout hoặc khi user logout
    }
  });
});

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, '..', 'Frontend', 'login.html'));
});

const uploadDir = path.resolve("./uploads");
if (!fs.existsSync(uploadDir)) fs.mkdirSync(uploadDir);

const storage = multer.diskStorage({
  destination: uploadDir,
  filename: (req, file, cb) => {
    const originalName = Buffer.from(file.originalname, "latin1").toString("utf8");
    const safeName = Date.now() + "-" + originalName;
    cb(null, safeName);
  },
});
const uploadFixed = multer({ storage });

app.post("/upload", uploadFixed.single("file"), (req, res) => {
  try {
    const { from, to } = req.body;
    const { filename, path: filePath, size } = req.file;
    console.log(`[Gateway] 📤 Upload file từ ${from} → ${to}: ${filename} (${size} bytes)`);

    res.json({
      success: true,
      filename: req.file.originalname,
      previewUrl: `/download/${filename}`,
    });

    const utf8Name = Buffer.from(req.file.originalname, "latin1").toString("utf8");
    sendToUser(to, {
        action: "private",
        from,
        file: `/download/${filename}`,
        filename: utf8Name,
    });

  } catch (err) {
    console.error("[Gateway] ❌ Lỗi upload:", err.message);
    res.json({ success: false, message: err.message });
  }
});

    app.get("/download/:filename", (req, res) => {
    const filePath = path.join(uploadDir, req.params.filename);
    if (!fs.existsSync(filePath)) return res.status(404).send("File không tồn tại!");
    const originalName = req.params.filename.split("-").slice(1).join("-");
    res.setHeader("Content-Disposition",
    `attachment; filename*=UTF-8''${encodeURIComponent(originalName)}`
  );
  res.download(filePath);
});

app.use("/uploads", express.static(uploadDir));


const httpsServer = https.createServer(credentials, app);

httpsServer.listen(HTTP_PORT, () => {
  console.log(`[Gateway] 🔒 HTTPS server chạy tại https://10.152.147.186:${HTTP_PORT}`);
});*/

import WebSocket, { WebSocketServer } from "ws";
import net from "net";
import fs from "fs";
import express from "express";
import multer from "multer";
import cors from "cors";
import dgram from "dgram";
import { fileURLToPath } from 'url';
import path from 'path';
import https from 'https';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const userTcpConnections = {}; 

const privateKey = fs.readFileSync(path.join(__dirname, 'key.pem'), 'utf8');
const certificate = fs.readFileSync(path.join(__dirname, 'cert.pem'), 'utf8');

const credentials = { key: privateKey, cert: certificate };

const WS_PORT = 3000; 
const HTTP_PORT = 3001; 
const TCP_HOST = "10.10.49.115";
const TCP_PORT_CHAT = 8888; 
const TCP_PORT_FILE = 9999; 
const UDP_PORT_VOICE = 6060; 
const UDP_HOST_CPP = "10.10.49.115"; 
const UDP_PORT_GATEWAY = 6061; 

const wss_https_server = https.createServer(credentials);
const wss = new WebSocketServer({ server: wss_https_server });

wss_https_server.listen(WS_PORT, () => {
    console.log(`[Gateway] 🔒 WebSocket Server (Chat+Voice) lắng nghe tại wss://10.10.49.115:${WS_PORT}`);
});

const app = express();
app.use(cors());
app.use(express.json({ limit: "50mb", type: "application/json; charset=utf-8" }));
app.use(express.urlencoded({ extended: true, limit: "50mb" }));
app.use(express.static(path.join(__dirname, '..', 'Frontend')));

const udpClient = dgram.createSocket("udp4"); 
const clientsOnline = {}; 
const voiceClients = {}; 
const activeCalls = {}; 

const userWebSockets = new Map();

function broadcastUserList() {
  const users = Object.keys(clientsOnline);
  const payload = JSON.stringify({ action: "online_list", users });
  for (const u in clientsOnline) {
    clientsOnline[u].send(payload);
  }
}

function sendToUser(username, message, clientMap) {
  const ws = clientMap[username]; 
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(message));
  }
}

function sendUDP(text) {
  const buf = Buffer.from(text);
  udpClient.send(buf, UDP_PORT_VOICE, UDP_HOST_CPP);
}

udpClient.on("message", (msg) => {
  const incCallMarker = Buffer.from("INCOMING_CALL:");
  const callAcceptedMarker = Buffer.from("CALL_ACCEPTED:");
  const callRejectedMarker = Buffer.from("CALL_REJECTED:");
  const dataMarker = Buffer.from("|DATA|");

  if (msg.indexOf(incCallMarker) === 0) {
    const text = msg.toString();
    const [callPart, toPart] = text.split("|TO:");
    const caller = callPart.split(":")[1];
    const callee = toPart;
    sendToUser(callee, { action: "INCOMING_CALL", from: caller }, clientsOnline);
    console.log(`[Gateway] 📞 Cuộc gọi từ ${caller} tới ${callee}`);
    return;
  }

  if (msg.indexOf(callAcceptedMarker) === 0) {
    const text = msg.toString();
    const [fromPart, toPart] = text.split("|TO:");
    const fromUser = fromPart.split(":")[1];
    const toUser = toPart;
    sendToUser(toUser, { action: "CALL_ACCEPTED", from: fromUser }, voiceClients);
    console.log(`[Gateway] ✅ ${fromUser} chấp nhận cuộc gọi từ ${toUser}`);
    return;
  }

  if (msg.indexOf(callRejectedMarker) === 0) {
    const text = msg.toString();
    const [fromPart, toPart] = text.split("|TO:");
    const fromUser = fromPart.split(":")[1];
    const toUser = toPart;
    sendToUser(toUser, { action: "CALL_REJECTED", from: fromUser }, voiceClients);
    console.log(`[Gateway] ❌ ${fromUser} từ chối cuộc gọi từ ${toUser}`);
    return;
  }

  const idx = msg.indexOf(dataMarker);
  if (idx !== -1) {
    const header = msg.slice(0, idx).toString(); 
    const fromUser = header.split(":")[1];
    const audioBuf = msg.slice(idx + dataMarker.length); 

    const toUser = activeCalls[fromUser];
    if (toUser) {
      const jsonMsg = {
        action: "AUDIO_DATA",
        from: fromUser,
        data: Array.from(audioBuf), 
      };
      sendToUser(fromUser, jsonMsg, voiceClients);
      sendToUser(toUser, jsonMsg, voiceClients);
    }
    return;
  }

  console.warn("[Gateway] ⚠️ UDP message không nhận diện:", msg.toString().slice(0, 200));
});

function createTcpConnection(username) {
  console.log(`[Gateway] 🔌 Tạo TCP connection mới cho ${username}`);
  
  const tcpClient = new net.Socket();
  let tcpBuffer = "";

  tcpClient.connect(TCP_PORT_CHAT, TCP_HOST, () => {
    console.log(`[Gateway] 🔌 Đã kết nối tới Server C++ Chat (${TCP_PORT_CHAT})`);
  });

  tcpClient.on("data", (chunk) => {
    tcpBuffer += chunk.toString();
    let boundary = tcpBuffer.indexOf('\n');
    
    while (boundary !== -1) {
      const message = tcpBuffer.substring(0, boundary).trim();
      tcpBuffer = tcpBuffer.substring(boundary + 1);
      
      if (message) {
        console.log("[Gateway] 📩 Nhận từ C++:", message.substring(0, 200));
        
        try {
          const parsed = JSON.parse(message);
          
          const ws = userWebSockets.get(username);
          
          if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify(parsed));
            
            if (parsed.action === "login_response" && parsed.status === "success") {
              console.log(`[Gateway] ✅ Đăng nhập thành công: ${parsed.username}`);
            }
            
            if (parsed.action === "history_response") {
              console.log(`[Gateway] 📨 ĐÃ GỬI lịch sử về WebSocket: ${parsed.chatHistory?.length || 0} messages`);
            }
            
            if (parsed.action === "online_list") {
              console.log(`[Gateway] 👥 Gửi danh sách online: ${parsed.users?.length || 0} users`);
            }
          } else {
            console.warn(`[Gateway] ⚠️ WebSocket của ${username} không khả dụng để gửi response`);
          }
        } catch (e) {
          console.warn("[Gateway] ⚠️ Parse error:", e.message);
        }
      }
      boundary = tcpBuffer.indexOf('\n');
    }
  });

  tcpClient.on("error", (err) => {
    console.error(`[Gateway] ❌ TCP error cho ${username}:`, err.message);
    delete userTcpConnections[username];
  });

  tcpClient.on("close", () => {
    console.log(`[Gateway] 🔌 TCP connection closed for ${username}`);
    if (userTcpConnections[username] === tcpClient) {
      delete userTcpConnections[username];
    }
  });

  return tcpClient;
}

wss.on("connection", (ws) => {
  console.log("[Gateway] ✅ Frontend kết nối mới (WS 3000)");

  let currentUsername = null;

  ws.on("message", (msg) => {
    try {
      const data = JSON.parse(msg.toString());
      console.log("[Gateway] ⬇️ Nhận từ Web:", data.action);

      let tcpClient = null;

      switch (data.action) {
        case "login":
          currentUsername = data.username;
          ws.username = data.username;
          
          userWebSockets.set(data.username, ws);
          
          if (userTcpConnections[data.username] && !userTcpConnections[data.username].destroyed) {
            console.log(`[Gateway] ♻️ Tái sử dụng TCP connection cho ${data.username}`);
            tcpClient = userTcpConnections[data.username];
          } else {
            tcpClient = createTcpConnection(data.username);
            userTcpConnections[data.username] = tcpClient;
          }
          
          tcpClient.write(JSON.stringify(data) + "\n");
          break;

        case "register":
          tcpClient = userTcpConnections[ws.username];
          if (!tcpClient || tcpClient.destroyed) {
            tcpClient = createTcpConnection(ws.username);
            userTcpConnections[ws.username] = tcpClient;
          }
          tcpClient.write(JSON.stringify(data) + "\n");
          break;

        case "join_chat":
          currentUsername = data.username;
          clientsOnline[data.username] = ws;
          ws.username = data.username;
          ws.type = 'chat';
          
          userWebSockets.set(data.username, ws);
          
          console.log(`[Gateway] 👤 ${data.username} đã tham gia chat`);
          
          tcpClient = userTcpConnections[data.username];
          if (!tcpClient || tcpClient.destroyed) {
            console.error(`[Gateway] ❌ KHÔNG TÌM THẤY TCP CONNECTION cho ${data.username}!`);
          } else {
            console.log(`[Gateway] ✅ Đã map WebSocket → TCP connection của ${data.username}`);
          }
          
          broadcastUserList();
          sendUDP(`REGISTER:${data.username}`);
          console.log(`[Gateway] 🎤 Đã đăng ký UDP cho ${data.username}`);
          break;

        case "list":
          tcpClient = userTcpConnections[ws.username];
          if (tcpClient && !tcpClient.destroyed) {
            tcpClient.write(JSON.stringify(data) + "\n");
          } else {
            console.error(`[Gateway] ❌ Không có TCP connection để gửi 'list'`);
          }
          break;

        case "get_history":
          const user1 = data.user1 || data.from || ws.username;
          const user2 = data.user2 || data.to;

          if (user1 && user2) {
            console.log(`[Gateway] 🗂️ Web yêu cầu lịch sử: ${user1} <-> ${user2}`);

            tcpClient = userTcpConnections[ws.username];

            if (!tcpClient || tcpClient.destroyed) {
              console.error(`[Gateway] ❌ KHÔNG CÓ TCP CONNECTION cho ${ws.username}!`);
              ws.send(JSON.stringify({
                action: "error",
                message: "Mất kết nối đến server. Vui lòng đăng nhập lại."
              }));
            } else {
              const historyRequest = JSON.stringify({ 
                action: "get_history",
                user1: user1,
                user2: user2
              }) + "\n";
              
              console.log(`[Gateway] 📤 Gửi qua TCP của ${ws.username}:`, historyRequest.trim());
              tcpClient.write(historyRequest);
            }
          } else {
            console.warn(`[Gateway] ⚠️ get_history thiếu user1 hoặc user2`);
          }
          break;

        case "REGISTER_VOICE":
          voiceClients[data.username] = ws;
          ws.username = data.username;
          ws.type = 'voice';
          console.log(`[Gateway] 🎙️ ${data.username} đã kết nối voice`);
          sendUDP(`REGISTER:${data.username}`);
          break;

        case "private":
          if (data.message) {
            sendToUser(data.to, {
              action: "private",
              from: data.from,
              message: data.message
            }, clientsOnline);

            // Lưu vào database
            tcpClient = userTcpConnections[ws.username];
            
            if (tcpClient && !tcpClient.destroyed) {
              const saveMsg = JSON.stringify({
                action: "save_private_message",
                from: data.from,
                to: data.to,
                message: data.message
              }) + "\n";
              tcpClient.write(saveMsg);
              console.log(`[Gateway] 💾 Lưu tin nhắn: ${data.from} → ${data.to}`);
            }
          }
          break;

        case "CALL":
          activeCalls[data.from] = data.to;
          sendUDP(`CALL:${data.from}|TO:${data.to}`);
          console.log(`[Gateway] 📞 ${data.from} gọi ${data.to}`);
          break;

        case "ACCEPT_CALL":
          sendUDP(`ACCEPT_CALL:${data.to}|FROM:${data.from}`);
          activeCalls[data.from] = data.to;
          activeCalls[data.to] = data.from;
          console.log(`[Gateway] ✅ ${data.to} chấp nhận ${data.from}`);
          break;

        case "REJECT_CALL":
          sendUDP(`REJECT_CALL:${data.to}|FROM:${data.from}`);
          delete activeCalls[data.from];
          console.log(`[Gateway] ❌ ${data.to} từ chối ${data.from}`);
          break;

        case "AUDIO_DATA":
          const header = Buffer.from(`FROM:${data.from}|TO:${data.to}|DATA|`);
          const audioBuf = Buffer.from(Uint8Array.from(data.data));
          const packet = Buffer.concat([header, audioBuf]);
          udpClient.send(packet, UDP_PORT_VOICE, UDP_HOST_CPP);
          break;

        default:
          console.warn("[Gateway] ⚠️ Action không xác định:", data.action);
      }
    } catch (err) {
      console.error("[Gateway] ❌ Error:", err.message);
    }
  });

  ws.on("close", () => {
    console.log(`[Gateway] 📴 ${ws.username || 'Client'} ngắt kết nối`);

    if (ws.username) {
      if (ws.type === 'chat') {
        delete clientsOnline[ws.username];
        broadcastUserList();
      } else if (ws.type === 'voice') {
        delete voiceClients[ws.username];
      }

      sendUDP(`UNREGISTER:${ws.username}`);
      
      // ✅ XÓA MAPPING
      userWebSockets.delete(ws.username);
    }
  });
});

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, '..', 'Frontend', 'login.html'));
});

const uploadDir = path.resolve("./uploads");
if (!fs.existsSync(uploadDir)) fs.mkdirSync(uploadDir);

const storage = multer.diskStorage({
  destination: uploadDir,
  filename: (req, file, cb) => {
    const originalName = Buffer.from(file.originalname, "latin1").toString("utf8");
    const safeName = Date.now() + "-" + originalName;
    cb(null, safeName);
  },
});
const uploadFixed = multer({ storage });

// ✅ FIX: Thêm error handling cho upload
app.post("/upload", uploadFixed.single("file"), (req, res) => {
  try {
    if (!req.file) {
      return res.status(400).json({ 
        success: false, 
        message: "Không có file được upload" 
      });
    }

    const { from, to } = req.body;
    const { filename, size, originalname } = req.file; // ✅ Lấy originalname từ req.file
    
    console.log(`[Gateway] 📤 Upload file từ ${from} → ${to}: ${filename} (${size} bytes)`);

    // ✅ CONVERT UTF-8 cho tên file
    const utf8Name = Buffer.from(originalname, "latin1").toString("utf8");

    // ✅ GỬI THÔNG TIN FILE XUỐNG C++ SERVER ĐỂ LƯU VÀO DATABASE
    const tcpClient = userTcpConnections[from];
    if (tcpClient && !tcpClient.destroyed) {
      const saveFileMsg = JSON.stringify({
        action: "save_file",
        from: from,
        to: to,
        filename: utf8Name,
        size: size,
        filepath: `/download/${filename}`
      }) + "\n";
      
      tcpClient.write(saveFileMsg);
      console.log(`[Gateway] 💾 Đã gửi thông tin file xuống C++ Server: ${utf8Name}`);
    } else {
      console.error(`[Gateway] ⚠️ Không tìm thấy TCP connection cho ${from} - file sẽ không được lưu vào DB!`);
    }

    // ✅ Gửi response về client (frontend)
    res.json({
      success: true,
      filename: utf8Name,
      previewUrl: `/download/${filename}`,
    });

    // ✅ Gửi notification cho người nhận qua WebSocket
    sendToUser(to, {
      action: "private",
      from,
      file: `/download/${filename}`,
      filename: utf8Name,
    }, clientsOnline);

    console.log(`[Gateway] ✅ Upload thành công và đã thông báo cho ${to}`);

  } catch (err) {
    console.error("[Gateway] ❌ Lỗi upload:", err.message);
    res.status(500).json({ success: false, message: err.message });
  }
});

app.get("/download/:filename", (req, res) => {
  const filePath = path.join(uploadDir, req.params.filename);
  if (!fs.existsSync(filePath)) return res.status(404).send("File không tồn tại!");
  const originalName = req.params.filename.split("-").slice(1).join("-");
  res.setHeader("Content-Disposition",
    `attachment; filename*=UTF-8''${encodeURIComponent(originalName)}`
  );
  res.download(filePath);
});

app.use("/uploads", express.static(uploadDir));

const httpsServer = https.createServer(credentials, app);

httpsServer.listen(HTTP_PORT, () => {
  console.log(`[Gateway] 🔒 HTTPS server chạy tại https://10.10.49.115:${HTTP_PORT}`);
});