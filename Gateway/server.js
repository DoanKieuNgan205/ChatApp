// ============================================
// IMPORTS TỔNG HỢP
// ============================================
import WebSocket, { WebSocketServer } from "ws";
import net from "net";
import fs from "fs";
import express from "express";
import multer from "multer";
import cors from "cors";
import path from "path";
import dgram from "dgram"; // 🟨 TỪ VOICE_GATEWAY

// ============================================
// CÁC HẰNG SỐ CỔNG
// ============================================
const WS_PORT = 3000;         // Cổng WebSocket chính (Chat + Voice)
const HTTP_PORT = 3001;       // Cổng HTTP (File Upload)
const TCP_HOST = "127.0.0.1";
const TCP_PORT_CHAT = 8888;   // Cổng C++ (Chat/Login)
const TCP_PORT_FILE = 9999;   // Cổng C++ (File)
const UDP_PORT_VOICE = 6060;  // 🟨 Cổng C++ UDP (Voice)
const UDP_HOST_CPP = "127.0.0.1"; // 🟨 Địa chỉ C++ UDP

// ============================================
// KHỞI TẠO CÁC SERVER
// ============================================

// 1. WebSocket Server (cho Chat + Voice Signaling)
const wss = new WebSocketServer({ port: WS_PORT });
console.log(`[Gateway] 🌐 WebSocket Server (Chat+Voice) lắng nghe tại ws://localhost:${WS_PORT}`);

// 2. HTTP Server (cho File)
const app = express();
app.use(cors());
app.use(express.json({ limit: "50mb", type: "application/json; charset=utf-8" }));
app.use(express.urlencoded({ extended: true, limit: "50mb" }));

// 3. UDP Client (nói chuyện với C++ Voice Server)
const udpClient = dgram.createSocket("udp4"); // 🟨 TỪ VOICE_GATEWAY

// ============================================
// BIẾN TRẠNG THÁI TOÀN CỤC (GLOBAL STATE)
// ============================================

// 💾 Danh sách client (WS) online DUY NHẤT
const clientsOnline = {}; // { username: ws }

// 💾 Bộ nhớ tạm lưu tin nhắn (text + file)
const chatHistory = {}; // { "userA_userB": [ ... ] }

// 💾 Bộ nhớ các cặp đang gọi
const activeCalls = {}; // { from: to } // 🟨 TỪ VOICE_GATEWAY

// ============================================
// 🚀 HÀM HỖ TRỢ (HELPER FUNCTIONS)
// ============================================

// 🔁 Gửi danh sách người dùng đang online (từ gateway.js)
function broadcastUserList() {
  const users = Object.keys(clientsOnline);
  const payload = JSON.stringify({ action: "online_list", users });
  for (const u in clientsOnline) {
    clientsOnline[u].send(payload);
  }
}

// 🗣️ Gửi tin nhắn cho một người dùng (từ voice_gateway.js)
function sendToUser(username, message) {
  const ws = clientsOnline[username]; // Dùng map clientsOnline duy nhất
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(message));
  }
}

// 📡 Gửi tin UDP tới C++ Voice Server (từ voice_gateway.js)
function sendUDP(text) {
  const buf = Buffer.from(text);
  udpClient.send(buf, UDP_PORT_VOICE, UDP_HOST_CPP);
}

// ============================================
// 🎧 LẮNG NGHE GÓI UDP TỪ C++ VOICE SERVER
// (Logic từ voice_gateway.js)
// ============================================

udpClient.on("message", (msg) => {
  const text = msg.toString();

  if (text.startsWith("INCOMING_CALL:")) {
    // INCOMING_CALL:<caller>|TO:<callee>
    const [callPart, toPart] = text.split("|TO:");
    const caller = callPart.split(":")[1];
    const callee = toPart;
    
    // Gửi thông báo cho người BỊ GỌI (callee)
    sendToUser(callee, { action: "INCOMING_CALL", from: caller });
    console.log(`[Gateway] 📞 Cuộc gọi từ ${caller} tới ${callee}`);
  }

  else if (text.startsWith("CALL_ACCEPTED:")) {
    // CALL_ACCEPTED:<callee>|TO:<caller>
    const [fromPart, toPart] = text.split("|TO:");
    const fromUser = fromPart.split(":")[1]; // người chấp nhận
    const toUser = toPart.split(":")[1]; // người gọi
    sendToUser(toUser, { action: "CALL_ACCEPTED", from: fromUser });
    console.log(`[Gateway] ✅ ${fromUser} chấp nhận cuộc gọi từ ${toUser}`);
  }

  else if (text.startsWith("CALL_REJECTED:")) {
    // REJECTED_CALL:<callee>|TO:<caller>
    const [fromPart, toPart] = text.split("|TO:");
    const fromUser = fromPart.split(":")[1]; // người từ chối
    const toUser = toPart.split(":")[1]; // người gọi
    sendToUser(toUser, { action: "CALL_REJECTED", from: fromUser });
    console.log(`[Gateway] ❌ ${fromUser} từ chối cuộc gọi từ ${toUser}`);
  }

  else if (text.startsWith("FROM:")) {
    // FROM:<name>|DATA|<binary>
    const headerEnd = text.indexOf("|DATA|");
    const fromUser = text.substring(5, headerEnd);
    const headerLen = headerEnd + 6;
    const audioData = msg.slice(headerLen);

    // Tìm người nhận
    const toUser = activeCalls[fromUser]; 
    if(toUser) {
        const jsonMsg = { 
            action: "AUDIO_DATA", // ❗ Đổi "type" thành "action"
            from: fromUser, 
            data: Array.from(audioData) 
        };
        // Gửi cho cả 2
        sendToUser(fromUser, jsonMsg);
        sendToUser(toUser, jsonMsg);
    }
  }
});

// ============================================
// 💬 XỬ LÝ KẾT NỐI WEBSOCKET TỪ CLIENT
// (Gộp logic của gateway.js và voice_gateway.js)
// ============================================
wss.on("connection", (ws) => {
  console.log("[Gateway] ✅ Frontend kết nối mới (WS 3000)");

  // Kết nối TCP tới C++ server (cho Login/Chat)
  const tcpClient = new net.Socket();
  tcpClient.connect(TCP_PORT_CHAT, TCP_HOST, () => {
    console.log(`[Gateway] 🔌 Đã kết nối tới Server C++ Chat (${TCP_PORT_CHAT})`);
  });

  // ============================
  // Khi nhận dữ liệu từ Web UI
  // ============================
   ws.on("message", (msg) => {
    try {
        // ❗ Lưu ý: Logic này giả định mọi tin nhắn là JSON
        // Kể cả AUDIO_DATA (được gửi dưới dạng mảng)
      const data = JSON.parse(msg.toString());
      console.log("[Gateway] ⬇️ Nhận từ Web:", data.action);

      switch (data.action) {
        
        // --- Actions của Chat (từ gateway.js) ---
        case "login":
        case "register":
        case "list":
        tcpClient.write(JSON.stringify(data));
        break;

        case "join_chat":
          clientsOnline[data.username] = ws;
          ws.username = data.username; // Gán username vào kết nối ws
          console.log(`[Gateway] 👤 ${data.username} đã tham gia chat`);
          broadcastUserList();

          // ✅ GỬI ĐĂNG KÝ UDP (từ logic voice_gateway)
          sendUDP(`REGISTER:${data.username}`);
          console.log(`[Gateway] 🎤 Đã đăng ký UDP cho ${data.username} tới C++`);
          break;

        case "private": 
          // (Code xử lý file/text chat từ gateway.js - giữ nguyên)
          // ... (giữ nguyên logic case "private" của bạn) ...
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
                const toClient = clientsOnline[data.to];
                if (toClient) {
                  toClient.send(JSON.stringify({
                    action: "private", from: data.from,
                    file: `/uploads/${path.basename(filePath)}`, filename
                  }));
                }
                setTimeout(() => {
                  fs.unlink(filePath, (err) => {
                    if (err) console.warn("[Gateway] ⚠️ Không thể xóa file tạm:", err.message);
                    else console.log(`[Gateway] 🧹 Đã xóa file tạm '${filename}'`);
                  });
                }, 500);
              });
              // ... (thêm 2 hàm xử lý lỗi fileStream và tcpFileClient)
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
              const toClient = clientsOnline[data.to];
              if (toClient) {
                toClient.send( JSON.stringify({
                    action: "private", from: data.from, message: data.message,
                  })
                );
              }
            }
          break;

        case "history_request":
          // (Code từ gateway.js - giữ nguyên)
          const key1 = `${data.username}_${data.with}`;
          const key2 = `${data.with}_${data.username}`;
          const key = chatHistory[key1] ? key1 : key2;
          const history = chatHistory[key] || [];
          ws.send(JSON.stringify({ action: "history_response", history }));
          break;
        
        // --- 🚀 Actions của Voice (MỚI - từ voice_gateway.js) ---
        // ❗ Đảm bảo client gửi "action" thay vì "type"
        case "CALL":
          activeCalls[data.from] = data.to; // lưu cặp cuộc gọi
          sendUDP(`CALL:${data.from}|TO:${data.to}`);
          console.log(`[Gateway] ${data.from} gọi ${data.to}`);
          break;

        case "ACCEPT_CALL":
          sendUDP(`ACCEPT_CALL:${data.to}|FROM:${data.from}`);
          // Cập nhật activeCalls
          activeCalls[data.from] = data.to;
          activeCalls[data.to] = data.from;
          console.log(`[Gateway] ${data.to} chấp nhận ${data.from}`);
          break;

        case "REJECT_CALL":
          sendUDP(`REJECT_CALL:${data.to}|FROM:${data.from}`);
          delete activeCalls[data.from]; // Xóa cặp gọi
          console.log(`[Gateway] ${data.to} từ chối ${data.from}`);
          break;

        case "AUDIO_DATA":
          // Gói dữ liệu nhị phân gửi sang C++ server
          const header = Buffer.from(`FROM:${data.from}|TO:${data.to}|`);
          // Client gửi data là mảng, chuyển lại thành Buffer
          const audioBuf = Buffer.from(Uint8Array.from(data.data));
          const packet = Buffer.concat([header, audioBuf]);
          udpClient.send(packet, UDP_PORT_VOICE, UDP_HOST_CPP);
          break;
        
        default:
          console.warn("[Gateway] ⚠️ Action không xác định:", data.action);
      }
    } catch (err) {
      console.error("[Gateway] ❌ Parse JSON lỗi:", err.message, msg.toString());
    }
  });

  // ==========================
  //  Khi nhận dữ liệu từ C++ Server (Chat/Login)
  // ==========================
  tcpClient.on("data", (chunk) => {
    // (Code từ gateway.js - giữ nguyên)
    const raw = chunk.toString().trim();
    console.log("[Gateway] 📩 Nhận từ Server C++ TCP:", raw);
    try {
      const data = JSON.parse(raw);
      // Gửi thẳng về cho client web đang kết nối này
      // (ví dụ: login_response, register_response)
      ws.send(JSON.stringify(data)); 
    } catch {
      console.warn("[Gateway] ⚠️ Không parse được JSON từ C++:", raw);
    }
  });

  // =============================
  // Khi Web client ngắt kết nối
  // =============================
  ws.on("close", () => {
    console.log("[Gateway] 📴 Web client ngắt kết nối");
    tcpClient.destroy(); // Đóng kết nối TCP tới C++ Chat

    if (ws.username && clientsOnline[ws.username]) {
      // 🚀 THÊM DÒNG NÀY
      sendUDP(`UNREGISTER:${ws.username}`);
      delete clientsOnline[ws.username];
      broadcastUserList();

      // 🚀 XỬ LÝ DỌN DẸP CUỘC GỌI
      const partner = activeCalls[ws.username];
      if(partner) {
          console.log(`[Gateway] 📞 ${ws.username} ngắt kết nối, kết thúc cuộc gọi với ${partner}`);
          sendToUser(partner, { action: "CALL_ENDED", from: ws.username });
          delete activeCalls[ws.username];
          delete activeCalls[partner];
      }
    }
  });

  tcpClient.on("error", (err) => {
    console.error("[Gateway] ❌ Lỗi TCP:", err.message);
  });
});


// =============================
// 🧱 HTTP Upload File
// (Code từ gateway.js - giữ nguyên)
// =============================
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

// 📤 Upload file
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

    // ✅ Gửi thông báo file tới người nhận (qua WebSocket)
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

// 📥 Endpoint tải file
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