import WebSocket, { WebSocketServer } from "ws";
import net from "net";
import fs from "fs";
import express from "express";
import multer from "multer";
import cors from "cors";
import dgram from "dgram";
import { fileURLToPath } from 'url';
import path from 'path';


const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);


const WS_PORT = 3000; 
const HTTP_PORT = 3001; 
const TCP_HOST = "127.0.0.1";
const TCP_PORT_CHAT = 8888; 
const TCP_PORT_FILE = 9999; 
const UDP_PORT_VOICE = 6060; 
const UDP_HOST_CPP = "127.0.0.1"; 
const UDP_PORT_GATEWAY = 6061; 


const wss = new WebSocketServer({ port: WS_PORT });
console.log(`[Gateway] 🌐 WebSocket Server (Chat+Voice) lắng nghe tại ws://localhost:${WS_PORT}`);

const app = express();
app.use(cors());
app.use(express.json({ limit: "50mb", type: "application/json; charset=utf-8" }));
app.use(express.urlencoded({ extended: true, limit: "50mb" }));

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

  const tcpClient = new net.Socket();
  tcpClient.connect(TCP_PORT_CHAT, TCP_HOST, () => {
    console.log(`[Gateway] 🔌 Đã kết nối tới Server C++ Chat (${TCP_PORT_CHAT})`);
  });

   ws.on("message", (msg) => {
    try {
      const data = JSON.parse(msg.toString());
      console.log("[Gateway] ⬇️ Nhận từ Web:", data.action);

      switch (data.action) {
        case "login":
          ws.username = data.username; 
          if (tcpClient.destroyed) {
            console.log("[Gateway] 🔄 TCP tới C++ đã đóng, khởi tạo lại...");
            tcpClient.connect(TCP_PORT_CHAT, TCP_HOST, () => {
              console.log("[Gateway] 🔌 Kết nối lại C++ thành công, gửi login...");
              tcpClient.write(JSON.stringify(data));
            });
            return;
          }

          tcpClient.write(JSON.stringify(data));
          break;

        case "register":
        case "list":
          tcpClient.write(JSON.stringify(data));
          break;

        case "join_chat":
          clientsOnline[data.username] = ws;
          ws.username = data.username;
          ws.type = 'chat';
          console.log(`[Gateway] 👤 ${data.username} đã tham gia chat`);
          broadcastUserList();

          sendUDP(`REGISTER:${data.username}`);
          console.log(`[Gateway] 🎤 Đã đăng ký UDP cho ${data.username} tới C++`);
          break;

        case "REGISTER_VOICE":
          voiceClients[data.username] = ws; 
          ws.username = data.username;
          ws.type = 'voice';
          console.log(`[Gateway] 🎙️ ${data.username} đã kết nối voice pop-up`);

          sendUDP(`REGISTER:${data.username}`);
          console.log(`[Gateway] 🎤 Đã gửi REGISTER:${data.username} tới C++ (UDP)`);
          break;

        case "private": 
          if (data.filepath) { 
            const filePath = data.filepath;
            const filename = data.filename;
            const tcpFileClient = new net.Socket();
            const fileStat = fs.statSync(data.filepath);
            const fileSize = fileStat.size;

            tcpFileClient.connect(TCP_PORT_FILE, TCP_HOST, () => {
              console.log(`[Gateway] 📦 Kết nối server file (${TCP_PORT_FILE})`);
              const header = JSON.stringify({
                action: "sendfile", from: data.from, to: data.to,
                filename: data.filename, size: fileSize,
              }) + "\n";
              tcpFileClient.write(header);
              const fileStream = fs.createReadStream(data.filepath);
              fileStream.pipe(tcpFileClient, { end: false });
              fileStream.on("end", () => {
                console.log(`[Gateway] ✅ Gửi xong file '${filename}'`);
                tcpFileClient.end(); 

                sendToUser(data.to, {
                    action: "private", from: data.from,
                    file: `/uploads/${path.basename(filePath)}`, filename
                }, clientsOnline);

                setTimeout(() => {
                  fs.unlink(filePath, (err) => {
                    if (err) console.warn("[Gateway] ⚠️ Không thể xóa file tạm:", err.message);
                    else console.log(`[Gateway] 🧹 Đã xóa file tạm '${filename}'`);
                  });
                }, 500);
              });
              
              fileStream.on("error", (err) => {
                console.error("[Gateway] ❌ Lỗi đọc file:", err.message);
                tcpFileClient.destroy();
              });
            });
            tcpFileClient.on("error", (err) => {
              console.error("[Gateway] ❌ Lỗi TCP file:", err.message);
            });
          } 
          else if (data.message) {
              sendToUser(data.to, {
                  action: "private", from: data.from, message: data.message,
              }, clientsOnline);
            }
          break;

        case "history_request":
          
          console.log(`[Gateway] 🗂️ Web yêu cầu lịch sử của ${data.username}`);
          tcpClient.write(JSON.stringify({ action: "get_history", username: data.username }) + "\n");

          break;
        
        case "CALL":
          activeCalls[data.from] = data.to; 
          sendUDP(`CALL:${data.from}|TO:${data.to}`);
          console.log(`[Gateway] ${data.from} gọi ${data.to}`);
          break;

        case "ACCEPT_CALL":
          sendUDP(`ACCEPT_CALL:${data.to}|FROM:${data.from}`);
          activeCalls[data.from] = data.to;
          activeCalls[data.to] = data.from;
          console.log(`[Gateway] ${data.to} chấp nhận ${data.from}`);
          break;

        case "REJECT_CALL":
          sendUDP(`REJECT_CALL:${data.to}|FROM:${data.from}`);
          delete activeCalls[data.from]; 
          console.log(`[Gateway] ${data.to} từ chối ${data.from}`);
          break;

        case "AUDIO_DATA":
          const header = Buffer.from(`FROM:${data.from}|TO:${data.to}|DATA|`);
          const audioBuf = Buffer.from(Uint8Array.from(data.data));
          const packet = Buffer.concat([header, audioBuf]);
          udpClient.send(packet, UDP_PORT_VOICE, UDP_HOST_CPP);

          console.log(`[Gateway] 🔊 Gửi AUDIO_DATA từ ${data.from} tới ${data.to} (${audioBuf.length} bytes)`);
          break;
        
          default:
          console.warn("[Gateway] ⚠️ Action không xác định:", data.action);
        }
      } catch (err) {
        console.error("[Gateway] ❌ Parse JSON lỗi:", err.message, msg.toString());
    }
  });

  let tcpBuffer = "";

  tcpClient.on("data", (chunk) => {
      tcpBuffer += chunk.toString();       
      let boundary = tcpBuffer.indexOf('\n');
      while (boundary !== -1) {
        const message = tcpBuffer.substring(0, boundary).trim();
        tcpBuffer = tcpBuffer.substring(boundary + 1);
        if (message) {
            console.log("[Gateway] 📩 Nhận 1 tin từ C++:", message);
            try {
              const data = JSON.parse(message);
              if (ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify(data));
              }

              if (data.action === "login_response" && data.status === "success") {
                ws.username = data.username; 
                console.log(`[Gateway] ✅ Đăng nhập thành công: ${ws.username}`);
                tcpClient.write(JSON.stringify({
                    action: "get_history",
                    username: ws.username
                }) + "\n");

                console.log(`[Gateway] 📜 Gửi yêu cầu lịch sử cho ${ws.username}`);
              }

              if (data.action === "history_response") {
                console.log(`[Gateway] 🗂️ Nhận lịch sử từ C++ (chat/file/call)`);

                const username = data.username;
                if (username) {
                  userHistories[username] = {
                    chat: data.chatHistory || [],
                    files: data.fileHistory || [],
                    calls: data.callHistory || []
                  };
                }

                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({
                      action: "history_response",
                      chatHistory: data.chatHistory || [],
                      fileHistory: data.fileHistory || [],
                      callHistory: data.callHistory || []
                    }));
                  }
                }


                if (data.action === "history") {
                  console.log("[Gateway] 🔹 Nhận 1 dòng lịch sử chat từ C++:", data.message);

                  ws.send(JSON.stringify({
                      action: "history",
                      message: data.message
                  }));
                  continue;
              }

              if (data.action === "file_history") {
                  console.log("[Gateway] 🔹 Nhận lịch sử file từ C++:", data.message);

                  ws.send(JSON.stringify({
                      action: "file_history",
                      message: data.message
                  }));
                  continue;
              }

              if (data.action === "call_history") {
                  console.log("[Gateway] 🔹 Nhận lịch sử voice call từ C++:", data.message);

                  ws.send(JSON.stringify({
                      action: "call_history",
                      message: data.message
                  }));
                  continue;
              }

            } catch (e) {
              console.warn("[Gateway] ⚠️ Bỏ qua tin nhắn C++ không phải JSON:", message);
            }
          }
      boundary = tcpBuffer.indexOf('\n');
      }
    });

  
  ws.on("close", () => {
  console.log(`[Gateway] 📴 ${ws.username || 'Client'} ngắt kết nối`); 
    tcpClient.destroy(); 

    if (ws.username) { 
      if (ws.type === 'chat') {
        delete clientsOnline[ws.username];
        broadcastUserList(); 
      } else if (ws.type === 'voice') {
        delete voiceClients[ws.username];
      }

      sendUDP(`UNREGISTER:${ws.username}`);

      const partner = activeCalls[ws.username];
      if(partner) {
          console.log(`[Gateway] 📞 ${ws.username} ngắt kết nối, kết thúc cuộc gọi với ${partner}`);
          sendToUser(partner, { action: "CALL_ENDED", from: ws.username }, voiceClients);
          delete activeCalls[ws.username];
          delete activeCalls[partner];    
      }
    }
});


  tcpClient.on("error", (err) => {
    console.error("[Gateway] ❌ Lỗi TCP:", err.message);
  });
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

app.listen(HTTP_PORT, () => {
  console.log(`[Gateway] 🚀 HTTP server chạy tại http://localhost:${HTTP_PORT}`);
});