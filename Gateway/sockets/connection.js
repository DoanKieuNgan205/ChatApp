import { createLoginClient, createTcpClient } from "../utils/cppClient.js";
import { handleChatMessage } from "./chatHandler.js";
import { handleVoiceData } from "./voiceHandler.js";

export function initWebSocket(wss, clientsOnline, udpSocket) {
  wss.on("connection", (ws) => {
    console.log("[WS] New frontend connected");

    let chatClient = null;
    let loginClient = createLoginClient();
    let currentUser = null;

    ws.on("message", (msg) => {
      const data = JSON.parse(msg.toString());
      console.log("[WS] ⬇️", data);

      switch (data.action) {
        case "login":
          loginClient.write(JSON.stringify(data));
          break;

        case "join_chat":
          // sau khi login thành công mới tạo chatClient
          if (!chatClient)
            chatClient = createTcpClient(); // port 8888
          ws.username = data.username;
          ws.chatClient = chatClient;
          clientsOnline[data.username] = ws;
          broadcastUserList(clientsOnline);
          break;

        case "private":
          if (chatClient) handleChatMessage(ws, clientsOnline, data);
          else console.warn("[WS] ⚠️ Chưa vào chat room!");
          break;

        case "voice_data":
          handleVoiceData(data);
          break;

        default:
          console.warn("[WS] Unknown action:", data.action);
      }
    });

    // Nhận phản hồi từ login
    loginClient.on("data", (chunk) => {
      const msg = chunk.toString().trim();
      console.log("[TCP→WS LOGIN]", msg);
      ws.send(msg);

      try {
        const response = JSON.parse(msg);
        // Nếu đăng nhập thành công → kết nối ChatServer
        if (response.action === "login_response" && response.status === "success") {
          currentUser = response.username || "guest";
          console.log(`[GATEWAY] ✅ ${currentUser} đăng nhập thành công, khởi tạo chatClient...`);

          chatClient = createTcpClient(); // kết nối ChatServer (port 8888)

          // Gửi gói join_chat tới ChatServer
          const joinMsg = JSON.stringify({
            action: "join_chat",
            username: currentUser
          });
          chatClient.write(joinMsg);

          // Cập nhật danh sách online
          ws.username = currentUser;
          clientsOnline[currentUser] = ws;
          broadcastUserList(clientsOnline);

          // Lắng nghe phản hồi chat
          chatClient.on("data", (chunk2) => {
            const chatMsg = chunk2.toString().trim();
            console.log("[TCP→WS CHAT]", chatMsg);
            ws.send(chatMsg);
          });
        }
      } catch (err) {
        console.error("[GATEWAY] ❌ Lỗi parse phản hồi login:", err.message);
      }
    });

    
    ws.on("close", () => {
      if (ws.username) delete clientsOnline[ws.username];
      broadcastUserList(clientsOnline);
      if (chatClient) chatClient.destroy();
      if (loginClient) loginClient.destroy();
      console.log("[WS] ❌ Client disconnected:", ws.username);
    });
  });
}

function broadcastUserList(clientsOnline) {
  const payload = JSON.stringify({
    action: "online_list",
    users: Object.keys(clientsOnline),
  });
  for (const u in clientsOnline) {
    clientsOnline[u].send(payload);
  }
}
