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

const fileBtn = document.getElementById("fileBtn");
const fileInput = document.getElementById("fileInput");

let currentChatUser = null;
let typingTimeout;
let messageHistory = {};

const incomingCallModal = document.getElementById("incomingCallModal");
const callerNameSpan = document.getElementById("callerName");
const acceptCallBtn = document.getElementById("acceptCallBtn");
const rejectCallBtn = document.getElementById("rejectCallBtn");

let incomingCallFrom = null; 

ws.addEventListener("open", () => {
  console.log("✅ Connected to gateway");
  meUsername.innerText = username;

  ws.send(JSON.stringify({ action: "join_chat", username }));

  setTimeout(() => ws.send(JSON.stringify({ action: "list" })), 200);

  ws.send(JSON.stringify({
    action: "get_history",
    username: username
  }));
});

ws.addEventListener("close", () => {
  alert("⚠️ Mất kết nối tới server. Vui lòng tải lại trang!");
});


ws.addEventListener("message", (event) => {
  const data = JSON.parse(event.data);
  console.log("📩 Received:", data);

  switch (data.action) {
    
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

      if (!messageHistory[username]) messageHistory[username] = [];

      (data.chatHistory || []).forEach(msg => {
        const partner = msg.from === username ? msg.to : msg.from;
        if (!messageHistory[partner]) messageHistory[partner] = [];
        messageHistory[partner].push({ type: "text", from: msg.from, text: msg.message });
      });

      (data.fileHistory || []).forEach(f => {
        const partner = f.from === username ? f.to : f.from;
        if (!messageHistory[partner]) messageHistory[partner] = [];
        messageHistory[partner].push({ type: "file", from: f.from, filename: f.filename, url: `http://localhost:3001${f.path}` });
      });

      (data.callHistory || []).forEach(c => {
        const partner = c.from === username ? c.to : c.from;
        if (!messageHistory[partner]) messageHistory[partner] = [];
        messageHistory[partner].push({ type: "call", from: c.from, status: c.status });
      });

      if (currentChatUser) renderHistory(currentChatUser);
      break;

    case "INCOMING_CALL":
        console.log(`📞 Cuộc gọi đến từ ${data.from}`);
        incomingCallFrom = data.from; 
        callerNameSpan.innerText = incomingCallFrom; 
        incomingCallModal.className = 'call-modal-visible'; 
        if (!messageHistory[incomingCallFrom]) messageHistory[incomingCallFrom] = [];
        messageHistory[incomingCallFrom].push({ type: "call", from: data.from, status: "incoming" });
        break;

    case "CALL_ENDED":
        if (!messageHistory[data.from]) messageHistory[data.from] = [];
        messageHistory[data.from].push({ type: "call", from: data.from, status: "ended" });
        if (currentChatUser === data.from) renderHistory(currentChatUser);
        break;

    default:
      console.log("⚠️ Unhandled action:", data);
      break;
  }
});

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

fileBtn.onclick = () => {
  if (!currentChatUser) return alert("Chọn người để gửi file!");
  fileInput.click();
};

fileInput.onchange = async () => {
  
  const file = fileInput.files[0];
  if (!file) return;

  showHint(`⏳ Đang gửi ${file.name}...`);

  try {
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
      div.onclick = () => {                     
        currentChatUser = u;
        chatWith.textContent = "💬 Đang chat với: " + u;
        renderHistory(u); 
      };
      userList.appendChild(div);
    }
  });
}


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
  } else {
    wrapper.classList.add("left");
    avatar.textContent = from[0].toUpperCase();
  }

  bubble.innerHTML = `<b>${from}:</b> ${msg}`;
  wrapper.append(avatar, bubble);
  messages.appendChild(wrapper);
  wrapper.scrollIntoView({ behavior: "smooth" });

  bubble.animate([{ backgroundColor: "#e0f7ff" }, { backgroundColor: "transparent" }], {
    duration: 800,
  });
}


function appendFileMessage(from, filename, fileUrl) {
  const wrapper = document.createElement("div");
  wrapper.classList.add("message");

  const bubble = document.createElement("div");
  bubble.classList.add("bubble");

  const avatar = document.createElement("div");
  avatar.classList.add("avatar");

  const isImage = /\.(png|jpg|jpeg|gif)$/i.test(fileUrl);

  if (from === "Tôi") {
    wrapper.classList.add("right");
    avatar.textContent = "🧑";
  } else {
    wrapper.classList.add("left");
    avatar.textContent = from[0].toUpperCase();
  }

  bubble.innerHTML = `<b>${from}:</b><br>${
    isImage
      ? `<img src="${fileUrl}" alt="${filename}" style="max-width:150px;border-radius:10px;margin-top:5px;">`
      : `<a href="${fileUrl}" download="${filename}" target="_blank" style="color:#0078ff;text-decoration:none;">📎 ${filename}</a>`
  }`;

  wrapper.append(avatar, bubble);
  messages.appendChild(wrapper);
  wrapper.scrollIntoView({ behavior: "smooth" });
}


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


function showUserNotification(user, message) {
  const existing = document.querySelector(`.user-item[data-user="${user}"]`);
  if (existing) existing.style.background = "#e6f7ff";
  showHint(`${user}: ${message}`);
}

function renderHistory(user) {
  messages.innerHTML = "";
  const history = messageHistory[user];
  if (!history) return;

  history.forEach(item => {
    const fromDisplay = item.from === username ? "Tôi" : item.from;

    if (item.type === "text") {
      appendMessage(fromDisplay, item.text);
    } else if (item.type === "file") {
      appendFileMessage(fromDisplay, item.filename, item.url);
    } else if (item.type === "call") {
      const callText = `${fromDisplay} ${item.status === "accepted" ? "📞 gọi thành công" :
                        item.status === "rejected" ? "❌ từ chối cuộc gọi" : "📲 bắt đầu cuộc gọi"}`;
      appendMessage(fromDisplay, callText);
    }
  });
}





document.getElementById("voiceBtn").addEventListener("click", () => {
  
  const me = document.getElementById("me-username").innerText.trim();
  const to = currentChatUser;
  if (!to || to === "Chọn người để chat") {
    alert("❗ Hãy chọn một người để gọi thoại.");
    return;
  }

  const url = `voice_call.html?me=${encodeURIComponent(me)}&to=${encodeURIComponent(to)}&action=call`;
  
  window.open(url, "voiceCall", "width=400,height=300,resizable=yes");
});


acceptCallBtn.addEventListener("click", () => {
  if (!incomingCallFrom) return;

  const url = `voice_call.html?me=${encodeURIComponent(username)}&to=${encodeURIComponent(incomingCallFrom)}&action=accept`;
  window.open(url, "voiceCall", "width=400,height=300,resizable=yes");

  incomingCallModal.className = 'call-modal-hidden';
  incomingCallFrom = null;
});


rejectCallBtn.addEventListener("click", () => {
  if (!incomingCallFrom) return;
  
  ws.send(JSON.stringify({ 
      action: "REJECT_CALL", 
      from: incomingCallFrom, 
      to: username             
  }));

  incomingCallModal.className = 'call-modal-hidden';
  incomingCallFrom = null;
});
