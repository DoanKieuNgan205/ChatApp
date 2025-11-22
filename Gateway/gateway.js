import WebSocket, { WebSocketServer } from "ws";
import net from "net";
import fs from "fs";
import express from "express";
import multer from "multer";
import cors from "cors";
import path from "path";

const WS_PORT = 3000;
const HTTP_PORT = 3001; 
const TCP_HOST = "127.0.0.1";
const TCP_PORT = 8888;
const TCP_PORT_FILE = 9999; 

const wss = new WebSocketServer({ port: WS_PORT });
console.log(`[Gateway] 🌐 WebSocket Server lắng nghe tại ws://localhost:${WS_PORT}`);
const app = express();

app.use(cors());
app.use(express.json({ limit: "50mb", type: "application/json; charset=utf-8" }));
app.use(express.urlencoded({ extended: true, limit: "50mb" }));

const upload = multer({ dest: "./uploads" }); 

const clientsOnline = {}; 
const chatHistory = {}; 

function broadcastUserList() {
  const users = Object.keys(clientsOnline);
  const payload = JSON.stringify({ action: "online_list", users });
  for (const u in clientsOnline) {
    clientsOnline[u].send(payload);
  }
}

wss.on("connection", (ws) => {
  console.log("[Gateway] ✅ Frontend kết nối mới");

  const tcpClient = new net.Socket();
  tcpClient.connect(TCP_PORT, TCP_HOST, () => {
    console.log(`[Gateway] 🔌 Đã kết nối tới Server C++ (${TCP_HOST}:${TCP_PORT})`);
  });

  ws.on("message", (msg) => {
    try {
      const data = JSON.parse(msg.toString());
      console.log("[Gateway] ⬇️ Nhận từ Web:", data);

      switch (data.action) {
        
        case "login":

          if (!data.username || !data.password) {
            console.warn("[Gateway] ⚠️ Bỏ qua login trống:", data);
            ws.send(JSON.stringify({
              action: "login_response",
              status: "fail",
              message: "Thiếu thông tin username/password",
            }));
          } else {
            tcpClient.write(JSON.stringify(data));
          }
          break;

        case "join_chat":
          clientsOnline[data.username] = ws;
          ws.username = data.username;
          console.log(`[Gateway] 👤 ${data.username} đã tham gia chat`);
          broadcastUserList();

          const registerMsg = `REGISTER:${data.username}`;
          udpSocket.send(registerMsg, UDP_PORT_CPP, UDP_HOST_CPP, (err) => {
            if (err)
              console.error(
                "[Gateway] ⚠️ Lỗi gửi đăng ký UDP:",
                err.message
              );
            else
              console.log(
                `[Gateway] 🎤 Đã đăng ký UDP cho ${data.username} tới C++`
              );
          });

          break;

        case "private": {      
          if (data.filepath) { 
            const filePath = data.filepath;
            const filename = data.filename;
            const tcpFileClient = new net.Socket();
            const fileStat = fs.statSync(data.filepath);
            const fileSize = fileStat.size;
            
            
            tcpFileClient.connect(TCP_PORT_FILE, TCP_HOST, () => {
              console.log(`[Gateway] 📦 Kết nối server file (${TCP_PORT_FILE})`);

              const header = JSON.stringify({
                action: "sendfile",
                from: data.from,
                to: data.to,
                filename: data.filename,
                size: fileSize,
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
                    action: "private",
                    from: data.from,
                    file: `/uploads/${path.basename(filePath)}`,
                    filename
                  }));
                }
                
                setTimeout(() => {
                  fs.unlink(filePath, (err) => {
                    if (err) console.warn("[Gateway] ⚠️ Không thể xóa file tạm:", err.message);
                    else console.log(`[Gateway] 🧹 Đã xóa file tạm '${filename}'`);
                  });
                }, 500);
              });

              tcpFileClient.on("error", (err) => {
                console.error("[Gateway] ❌ Lỗi gửi file:", err.message);
                tcpFileClient.destroy();
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
              
              const toClient = clientsOnline[data.to];
              if (toClient) {
                toClient.send(
                  JSON.stringify({
                    action: "private",
                    from: data.from,
                    message: data.message,
                  })
                );
              }
            }
            break; 
        }

        
        case "history_request": {
          const key1 = `${data.username}_${data.with}`;
          const key2 = `${data.with}_${data.username}`;
          const key = chatHistory[key1] ? key1 : key2;
          const history = chatHistory[key] || [];

          ws.send(JSON.stringify({
            action: "history_response",
            history,
          }));
          break;
        }


        
        case "register":
        case "list":
          tcpClient.write(JSON.stringify(data));
          break;

        default:
          console.warn("[Gateway] ⚠️ Action không xác định:", data.action);
      }
    } catch (err) {
      console.error("[Gateway] ❌ Parse JSON lỗi:", err.message);
    }
  });

  
  tcpClient.on("data", (chunk) => {
    const raw = chunk.toString().trim();
    console.log("[Gateway] 📩 Nhận từ Server C++:", raw);

    try {
      const data = JSON.parse(raw);

      if (data.message === "LOGIN_SUCCESS") {
        ws.send(
          JSON.stringify({
            action: "login_response",
            status: "success",
            message: data.message,
          })
        );
      } else if (data.message === "LOGIN_FAIL") {
        ws.send(
          JSON.stringify({
            action: "login_response",
            status: "fail",
            message: data.message,
          })
        );
      }
        
    else if (data.message === "REGISTER_SUCCESS") {
      console.log("[Gateway] 🟢 Đăng ký thành công từ C++ Server");
      ws.send(
        JSON.stringify({
          action: "register_response",
          status: "success",
          message: data.message,
        })
      );
    } else if (data.message === "REGISTER_FAIL") {
      console.log("[Gateway] 🔴 Đăng ký thất bại từ C++ Server");
      ws.send(
        JSON.stringify({
          action: "register_response",
          status: "fail",
          message: data.message,
        })
      );
    }

    } catch {
      console.warn("[Gateway] ⚠️ Không parse được JSON:", raw);
    }
  });


  ws.on("close", () => {
    console.log("[Gateway] 📴 Web client ngắt kết nối");
    if (ws.username && clientsOnline[ws.username]) {
      delete clientsOnline[ws.username];
      broadcastUserList();
    }
    tcpClient.destroy();
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

    
    const toClient = clientsOnline[to];
    if (toClient) {
      const utf8Name = Buffer.from(req.file.originalname, "latin1").toString("utf8");
    

      toClient.send(JSON.stringify({
        action: "private",
        from,
        file: `/download/${filename}`,
        filename: utf8Name, 
      }));

    }
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

app.listen(3001, () => {
  console.log("[Gateway] 🚀 HTTP server chạy tại http://localhost:3001");
});





