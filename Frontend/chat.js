const username = sessionStorage.getItem("chat_username");
if (!username) {
  alert("Vui lòng đăng nhập trước!");
  window.location.href = "login.html";
} 

const ws = new WebSocket("ws://localhost:3000");

const userList = document.getElementById("userList");
const messages = document.getElementById("messages");
const msgInput = document.getElementById("msgInput");
const sendBtn = document.getElementById("sendBtn");
const chatWith = document.getElementById("chatWith");
const meUsername = document.getElementById("me-username");


// ✅ thêm phần gửi file
const fileBtn = document.getElementById("fileBtn");
const fileInput = document.getElementById("fileInput");

let currentChatUser = null;
let typingTimeout;
// Bộ nhớ tạm tin nhắn
let messageHistory = {};

// Khi WebSocket mở kết nối
ws.addEventListener("open", () => {
  console.log("✅ Connected to gateway");
  meUsername.innerText = username;

  ws.send(JSON.stringify({ action: "join_chat", username }));

  // Lấy danh sách user online
  setTimeout(() => ws.send(JSON.stringify({ action: "list" })), 200);
});

// Nếu bị mất kết nối
ws.addEventListener("close", () => {
  alert("⚠️ Mất kết nối tới server. Vui lòng tải lại trang!");
});

// Nhận dữ liệu từ Gateway
ws.addEventListener("message", (event) => {
  const data = JSON.parse(event.data);
  console.log("📩 Received:", data);

  switch (data.action) {
    /*case "online_list":
      renderUserList(data.users);
      break;

    case "private":
      if (data.file) {
        if (data.from === currentChatUser || data.from === "Tôi")
          appendFileMessage(data.from, data.filename, data.file);
        else showUserNotification(data.from, "📎 Gửi file mới");
      } else if (data.from === currentChatUser) {
        appendMessage(data.from, data.message);
      } else {
        showUserNotification(data.from, "💬 Gửi tin nhắn mới");
      }
      break;

    case "broadcast":
      appendMessage("Broadcast", data.message);
      break;

    case "history_response":
      messages.innerHTML = "";
      data.history.forEach((msg) => {
        if (msg.file)
          appendFileMessage(msg.from === username ? "Tôi" : msg.from, msg.filename, msg.file);
        else appendMessage(msg.from === username ? "Tôi" : msg.from, msg.message);
      });
      break;

    default:
      console.log("⚠️ Unhandled action:", data);
      break;
  }*/
    case "online_list":
      renderUserList(data.users);
      break;

    case "private": {
      const fromUser = data.from === username ? "Tôi" : data.from;
      const partner = data.from === username ? data.to : data.from;
      if (!messageHistory[partner]) messageHistory[partner] = [];

      if (data.file) {
        const fileUrl = `http://localhost:3001${data.file}`;
        messageHistory[partner].push({ type: "file", filename: data.filename, url: fileUrl });

        if (partner === currentChatUser) {
          appendFileMessage(fromUser, data.filename, fileUrl);
        } else {
          showUserNotification(partner, "📎 Gửi file mới");
        }
      } else if (data.message) {
        messageHistory[partner].push({ type: "text", text: data.message });

        if (partner === currentChatUser) {
          appendMessage(fromUser, data.message);
        } else {
          showUserNotification(partner, "💬 Gửi tin nhắn mới");
        }
      }
      break;
    }

    case "broadcast":
      appendMessage("Broadcast", data.message);
      break;

    case "history_response":
      messages.innerHTML = "";
      data.history.forEach((msg) => {
        if (msg.file)
          appendFileMessage(msg.from === username ? "Tôi" : msg.from, msg.filename, msg.file);
        else appendMessage(msg.from === username ? "Tôi" : msg.from, msg.message);
      });
      break;

      // 🚀 THÊM CASE MỚI CHO VOICE
    case "INCOMING_CALL":
        console.log(`📞 Cuộc gọi đến từ ${data.from}`);
        if (confirm(`${data.from} đang gọi bạn. Chấp nhận không?`)) {
            // Nếu đồng ý, mở pop-up và truyền tín hiệu là "accept"
            const url = `voice_call.html?me=${encodeURIComponent(username)}&to=${encodeURIComponent(data.from)}&action=accept`;
            window.open(url, "voiceCall", "width=400,height=300");
        } else {
            // Nếu từ chối, gửi tin REJECT
            ws.send(JSON.stringify({ 
                action: "REJECT_CALL", 
                from: data.from, // người gọi
                to: username    // tôi là người bị gọi
            }));
        }
        break;

    default:
      console.log("⚠️ Unhandled action:", data);
      break;
  }
});

// Gửi tin nhắn
function sendMessage() {
  const msg = msgInput.value.trim();
  if (!msg) return;
  if (!currentChatUser) {
    showHint("💡 Hãy chọn một người để chat!");
    return;
  }

  ws.send(JSON.stringify({
    action: "private",
    from: username,
    to: currentChatUser,
    message: msg,
  }));

  appendMessage("Tôi", msg);
  msgInput.value = "";
}

// Sự kiện gửi
sendBtn.onclick = sendMessage;
msgInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !e.shiftKey) {
    e.preventDefault();
    sendMessage();
  } else {
    ws.send(JSON.stringify({
      action: "typing",
      from: username,
      to: currentChatUser
    }));
  }
});

// File upload
/*fileBtn.onclick = () => {
  if (!currentChatUser) {
    showHint("💡 Hãy chọn người để gửi file!");
    return;
  }
  fileInput.click();
};

fileInput.onchange = () => {
  const file = fileInput.files[0];
  if (!file) return;

  showHint(`⏳ Đang gửi ${file.name}...`);

  const reader = new FileReader();
  reader.onload = () => {
    const base64 = reader.result.split(",")[1];
    ws.send(JSON.stringify({
      action: "private",
      from: username,
      to: currentChatUser,
      file: base64,
      filename: file.name,
    }));

    appendFileMessage("Tôi", file.name, reader.result);
    console.log(`[Client] ✅ Gửi file ${file.name} tới ${currentChatUser}`);
    hideHint();
    fileInput.value = "";
  };
  reader.readAsDataURL(file);
};*/

fileBtn.onclick = () => {
  if (!currentChatUser) return alert("Chọn người để gửi file!");
  fileInput.click();
};

fileInput.onchange = async () => {
  
  const file = fileInput.files[0];
  if (!file) return;

  showHint(`⏳ Đang gửi ${file.name}...`);

  try {
    // Upload file thật tới Gateway
    const formData = new FormData();
    formData.append("file", file);
    formData.append("from", username);
    formData.append("to", currentChatUser);

    const res = await fetch("http://localhost:3001/upload", {
      method: "POST",
      body: formData,
    });

    const result = await res.json();
    console.log("📦 File upload result:", result);

    if (result.success) {
      const fileUrl = "http://localhost:3001" + result.previewUrl;

      // Gửi thông tin file qua WebSocket để hiển thị ở người nhận
      ws.send(
        JSON.stringify({
          action: "private",
          from: username,
          to: currentChatUser,
          file: fileUrl,
          filename: file.name,
        })
      );

      appendFileMessage("Tôi", file.name, fileUrl);
      hideHint();
    } else {
      alert("❌ Lỗi gửi file!");
    }
  } catch (err) {
    console.error("Upload error:", err);
    alert("⚠️ Lỗi kết nối tới server upload!");
  } finally {
    fileInput.value = "";
  }
};

// Hiển thị danh sách người dùng
function renderUserList(users) {
  userList.innerHTML = "";
  if (users.length === 1) {
    userList.innerHTML = "<p style='text-align:center;color:#888;'>Chưa có ai online 😔</p>";
    return;
  }

  users.forEach((u) => {
    if (u !== username) {
      const div = document.createElement("div");
      div.className = "user-item";
      div.textContent = u;

      /*div.onclick = () => {
        currentChatUser = u;
        chatWith.textContent = "💬 Đang chat với: " + u;
        messages.innerHTML = "";
        ws.send(JSON.stringify({
          action: "history_request",
          username,
          with: currentChatUser,
        }));
      };*/

      div.onclick = () => {
        currentChatUser = u;
        chatWith.textContent = "💬 Đang chat với: " + u;
        messages.innerHTML = "";

        if (messageHistory[u]) {
          for (const msg of messageHistory[u]) {
            if (msg.type === "text") appendMessage(u, msg.text);
            if (msg.type === "file") appendFileMessage(u, msg.filename, msg.url);
          }
        } else {
          ws.send(JSON.stringify({
            action: "history_request",
            username,
            with: currentChatUser,
          }));
        }
      };
      userList.appendChild(div);
    }
  });
}

// Tin nhắn text
function appendMessage(from, msg) {
  const wrapper = document.createElement("div");
  wrapper.classList.add("message");

  const bubble = document.createElement("div");
  bubble.classList.add("bubble");

  const avatar = document.createElement("div");
  avatar.classList.add("avatar");


  if (from === "Tôi") {
    wrapper.classList.add("right");
    avatar.textContent = "🧑";
    //bubble.innerHTML = `<b>${from}:</b> ${msg}`;
  } else {
    wrapper.classList.add("left");
    avatar.textContent = from[0].toUpperCase();
    //bubble.innerHTML = `<b>${from}:</b> ${msg}`;
  }

  bubble.innerHTML = `<b>${from}:</b> ${msg}`;
  wrapper.append(avatar, bubble);
  messages.appendChild(wrapper);
  wrapper.scrollIntoView({ behavior: "smooth" });

  // Hiệu ứng nổi bật khi nhận tin nhắn
  bubble.animate([{ backgroundColor: "#e0f7ff" }, { backgroundColor: "transparent" }], {
    duration: 800,
  });
}

// Tin nhắn file
function appendFileMessage(from, filename, fileUrl) {
  /*const wrapper = document.createElement("div");
  wrapper.classList.add("message");

  const bubble = document.createElement("div");
  bubble.classList.add("bubble");

  const avatar = document.createElement("div");
  avatar.classList.add("avatar");

  const isImage = base64data.startsWith("data:image");

  if (from === "Tôi") {
    wrapper.classList.add("right");
    avatar.textContent = "🧑";
  } else {
    wrapper.classList.add("left");
    avatar.textContent = from[0].toUpperCase();
  }

  bubble.innerHTML = `<b>${from}:</b><br>${
    isImage
      ? `<img src="${base64data}" alt="${filename}" style="max-width:150px;border-radius:10px;margin-top:5px;">`
      : `<a href="${base64data}" download="${filename}" style="color:#0078ff;">📎 ${filename}</a>`
  }`;

  wrapper.append(avatar, bubble);
  messages.appendChild(wrapper);
  wrapper.scrollIntoView({ behavior: "smooth" });*/

  const wrapper = document.createElement("div");
  wrapper.classList.add("message");

  const bubble = document.createElement("div");
  bubble.classList.add("bubble");

  const avatar = document.createElement("div");
  avatar.classList.add("avatar");

  /*const isImage =
    fileUrl.endsWith(".png") ||
    fileUrl.endsWith(".jpg") ||
    fileUrl.endsWith(".jpeg") ||
    fileUrl.endsWith(".gif");*/

  const isImage = /\.(png|jpg|jpeg|gif)$/i.test(fileUrl);

  if (from === "Tôi") {
    wrapper.classList.add("right");
    avatar.textContent = "🧑";
  } else {
    wrapper.classList.add("left");
    avatar.textContent = from[0].toUpperCase();
  }

  /*bubble.innerHTML = `<b>${from}:</b><br>${
    isImage
      ? `<img src="http://localhost:3001${fileUrl}" alt="${filename}" style="max-width:150px;border-radius:10px;margin-top:5px;">`
      : `<a href="http://localhost:3001${fileUrl}" download="${filename}" style="color:#0078ff;">📎 ${filename}</a>`
  }`;*/

  // ✅ hiển thị link tải đúng tên gốc
  bubble.innerHTML = `<b>${from}:</b><br>${
    isImage
      ? `<img src="${fileUrl}" alt="${filename}" style="max-width:150px;border-radius:10px;margin-top:5px;">`
      : `<a href="${fileUrl}" download="${filename}" target="_blank" style="color:#0078ff;text-decoration:none;">📎 ${filename}</a>`
  }`;

  wrapper.append(avatar, bubble);
  messages.appendChild(wrapper);
  wrapper.scrollIntoView({ behavior: "smooth" });
}

// Gợi ý UX
function showHint(text) {
  let hint = document.getElementById("hint-msg");
  if (!hint) {
    hint = document.createElement("div");
    hint.id = "hint-msg";
    hint.style.position = "fixed";
    hint.style.bottom = "20px";
    hint.style.left = "50%";
    hint.style.transform = "translateX(-50%)";
    hint.style.background = "#333";
    hint.style.color = "#fff";
    hint.style.padding = "8px 16px";
    hint.style.borderRadius = "12px";
    hint.style.fontSize = "14px";
    hint.style.opacity = "0.9";
    document.body.appendChild(hint);
  }
  hint.innerText = text;
  hint.style.display = "block";
}

function hideHint() {
  const hint = document.getElementById("hint-msg");
  if (hint) hint.style.display = "none";
}

// Thông báo khi có tin nhắn mới
function showUserNotification(user, message) {
  const existing = document.querySelector(`.user-item[data-user="${user}"]`);
  if (existing) existing.style.background = "#e6f7ff";
  showHint(`${user}: ${message}`);
}



// --- Voice Call Integration ---
document.getElementById("voiceBtn").addEventListener("click", () => {
  // Giả sử bạn đã có biến currentUser và chatWith
  const me = document.getElementById("me-username").innerText.trim();
  const to = currentChatUser;
  if (!to || to === "Chọn người để chat") {
    alert("❗ Hãy chọn một người để gọi thoại.");
    return;
  }

  // Mở trang voice_call.html và truyền tham số
  //const url = `voice_call.html?me=${encodeURIComponent(me)}&to=${encodeURIComponent(to)}`;
  const url = `voice_call.html?me=${encodeURIComponent(me)}&to=${encodeURIComponent(to)}&action=call`;
  //window.location.href = url;
  window.open(url, "voiceCall", "width=400,height=300,resizable=yes");
});


