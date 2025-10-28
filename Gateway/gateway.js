import WebSocket, { WebSocketServer } from "ws";
import net from "net";
import fs from "fs";
import express from "express";
import multer from "multer";
import cors from "cors";
import path from "path";

const WS_PORT = 3000;
const HTTP_PORT = 3001; // ✅ Đổi tên cổng HTTP
const TCP_HOST = "127.0.0.1";
// Cổng server C++ (Chat/Login)
const TCP_PORT = 8888;
// Cổng server C++ (File)
const TCP_PORT_FILE = 9999; // ✅ cổng file riêng


const wss = new WebSocketServer({ port: WS_PORT });
console.log(`[Gateway] 🌐 WebSocket Server lắng nghe tại ws://localhost:${WS_PORT}`);

const app = express();

app.use(cors());
app.use(express.json({ limit: "50mb", type: "application/json; charset=utf-8" }));
app.use(express.urlencoded({ extended: true, limit: "50mb" }));


const upload = multer({ dest: "./uploads" }); // thư mục lưu tạm


// 💾 Danh sách client frontend đang online
const clientsOnline = {}; // { username: ws }

// 💾 Bộ nhớ tạm lưu tin nhắn (text + file)
const chatHistory = {}; // { "userA_userB": [ {from,to,message,file,filename} ] }

// 🔁 Hàm gửi danh sách người dùng đang online
function broadcastUserList() {
  const users = Object.keys(clientsOnline);
  const payload = JSON.stringify({ action: "online_list", users });
  for (const u in clientsOnline) {
    clientsOnline[u].send(payload);
  }
}


// ============================================
// Xử lý kết nối từ WebSocket (frontend)
// ============================================
wss.on("connection", (ws) => {
  console.log("[Gateway] ✅ Frontend kết nối mới");

  // Kết nối TCP tới C++ server
  const tcpClient = new net.Socket();
  tcpClient.connect(TCP_PORT, TCP_HOST, () => {
    console.log(`[Gateway] 🔌 Đã kết nối tới Server C++ (${TCP_HOST}:${TCP_PORT})`);
  });

  // ============================
  // Khi nhận dữ liệu từ Web UI
  // ============================
  ws.on("message", (msg) => {
    try {
      const data = JSON.parse(msg.toString());
      console.log("[Gateway] ⬇️ Nhận từ Web:", data);

      switch (data.action) {
        // 🔹 Gửi đăng nhập sang C++ server
        case "login":
          //tcpClient.write(JSON.stringify(data));

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

        // 🔹 Khi frontend xác nhận login thành công và join vào chat
        case "join_chat":
          clientsOnline[data.username] = ws;
          ws.username = data.username;
          console.log(`[Gateway] 👤 ${data.username} đã tham gia chat`);
          broadcastUserList();

          // ✅ 4. Gửi đăng ký UDP cho user này
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

        // 🔹 Chat riêng giữa 2 user (text hoặc file)
        case "private": {      
          if (data.filepath) {  // frontend gửi đường dẫn file thật
            const filePath = data.filepath;
            const filename = data.filename;
            const tcpFileClient = new net.Socket();
            const fileStat = fs.statSync(data.filepath);
            const fileSize = fileStat.size;
            
            
            tcpFileClient.connect(TCP_PORT_FILE, TCP_HOST, () => {
              console.log(`[Gateway] 📦 Kết nối server file (${TCP_PORT_FILE})`);

              // Gửi header JSON
              const header = JSON.stringify({
                action: "sendfile",
                from: data.from,
                to: data.to,
                filename: data.filename,
                size: fileSize,
              }) + "\n";
              tcpFileClient.write(header);

              // Pipe file binary thật
              const fileStream = fs.createReadStream(data.filepath);
              fileStream.pipe(tcpFileClient, { end: false });

              fileStream.on("end", () => {
                console.log(`[Gateway] ✅ Gửi xong file '${filename}'`);
                tcpFileClient.end(); // ✅ chỉ đóng sau khi gửi xong
                // 🔹 Gửi thông báo file cho người nhận qua WebSocket
                const toClient = clientsOnline[data.to];
                if (toClient) {
                  toClient.send(JSON.stringify({
                    action: "private",
                    from: data.from,
                    file: `/uploads/${path.basename(filePath)}`,
                    filename
                  }));
                }
                //setTimeout(() => fs.unlinkSync(filePath), 500); // xóa file tạm sau 0.5s
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
              // Tin nhắn text
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
            break; // ✅ Thêm dòng này
        }

        // 🔹 Khi frontend yêu cầu lịch sử chat giữa 2 người
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


        // 🔹 Các hành động khác gửi qua TCP
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

  // ==========================
  //  Khi nhận dữ liệu từ C++ Server
  // ==========================
  tcpClient.on("data", (chunk) => {
    const raw = chunk.toString().trim();
    console.log("[Gateway] 📩 Nhận từ Server C++:", raw);

    try {
      const data = JSON.parse(raw);

      // ✅ Trả kết quả login về frontend
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
          // ✅ Trả kết quả đăng ký về frontend
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

  // =============================
  // Khi Web client ngắt kết nối
  // =============================
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


// =============================
// 🧱 HTTP Upload File
// =============================
const uploadDir = path.resolve("./uploads");
if (!fs.existsSync(uploadDir)) fs.mkdirSync(uploadDir);

const storage = multer.diskStorage({
  destination: uploadDir,
  filename: (req, file, cb) => {
    // Giữ timestamp để tránh trùng tên nhưng vẫn chứa tên gốc
    //const safeName = Date.now() + "-" + file.originalname;
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

    // ✅ Gửi phản hồi cho frontend (link HTTP thật)
    res.json({
      success: true,
      filename: req.file.originalname,
      previewUrl: `/download/${filename}`,
    });

    // ✅ Gửi thông báo file tới người nhận (qua WebSocket)
    const toClient = clientsOnline[to];
    if (toClient) {
      const utf8Name = Buffer.from(req.file.originalname, "latin1").toString("utf8");
    

      toClient.send(JSON.stringify({
        action: "private",
        from,
        file: `/download/${filename}`,
        filename: utf8Name, // ✅ tên chuẩn UTF-8
      }));

    }
  } catch (err) {
    console.error("[Gateway] ❌ Lỗi upload:", err.message);
    res.json({ success: false, message: err.message });
  }
});

// 📥 Endpoint tải file: giữ nguyên tên thật
app.get("/download/:filename", (req, res) => {
  const filePath = path.join(uploadDir, req.params.filename);
  if (!fs.existsSync(filePath)) return res.status(404).send("File không tồn tại!");

  // 🧠 Giữ nguyên tên thật (bỏ timestamp)
  const originalName = req.params.filename.split("-").slice(1).join("-");
  //res.download(filePath, originalName);
  res.setHeader("Content-Disposition",
    `attachment; filename*=UTF-8''${encodeURIComponent(originalName)}`
  );
  res.download(filePath);

});

// ✅ Cho phép truy cập /uploads trực tiếp (dùng khi xem ảnh)
app.use("/uploads", express.static(uploadDir));

app.listen(3001, () => {
  console.log("[Gateway] 🚀 HTTP server chạy tại http://localhost:3001");
});





