import fs from "fs";
import path from "path";
import net from "net";
import { CONFIG } from "../utils/config.js";

/**
 * Gửi tin nhắn hoặc file giữa các user qua Gateway → ChatServer (C++).
 * @param {WebSocket} ws - kết nối WebSocket hiện tại.
 * @param {Object} clientsOnline - danh sách user đang online.
 * @param {Object} data - dữ liệu tin nhắn gửi từ frontend.
 */
export function handleChatMessage(ws, clientsOnline, data) {
  const { from, to, message, filepath, filename } = data;

  // 🟦 Nếu có file → gửi tới FileServer (port 9999)
  if (filepath) {
    const tcpFileClient = new net.Socket();
    const fileSize = fs.statSync(filepath).size;

    tcpFileClient.connect(CONFIG.TCP_PORT_FILE, CONFIG.TCP_HOST, () => {
      const header = JSON.stringify({
        action: "sendfile",
        from,
        to,
        filename,
        size: fileSize,
      }) + "\n";
      tcpFileClient.write(header);
      fs.createReadStream(filepath).pipe(tcpFileClient);
    });

    tcpFileClient.on("close", () => {
      console.log(`[Chat] 📦 File '${filename}' sent from ${from} to ${to}`);
      const toClient = clientsOnline[to];
      if (toClient) {
        toClient.send(JSON.stringify({
          action: "private",
          from,
          file: `/uploads/${path.basename(filepath)}`,
          filename
        }));
      }
      fs.unlink(filepath, () => {});
    });

    tcpFileClient.on("error", (err) => {
      console.error("[FileServer] ❌ Error:", err.message);
    });

    return;
  }

  // 🟨 Nếu là tin nhắn text → gửi qua ChatServer (port 8888)
  if (message && ws.chatClient) {
    const msgPayload = JSON.stringify({
      action: "private",
      from,
      to,
      message
    }) + "\n";

    ws.chatClient.write(msgPayload);
    console.log(`[Chat] 💬 ${from} → ${to}: ${message}`);
  }
  // Nếu ChatServer chưa sẵn sàng → fallback gửi trực tiếp giữa frontend
  else if (message && !ws.chatClient) {
    const toClient = clientsOnline[to];
    if (toClient) {
      toClient.send(JSON.stringify({ action: "private", from, message }));
    } else {
      ws.send(JSON.stringify({
        action: "system",
        message: `⚠️ Người dùng '${to}' hiện không online.`,
      }));
    }
  }
}
