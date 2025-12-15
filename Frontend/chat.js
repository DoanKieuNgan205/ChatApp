/*const username = sessionStorage.getItem("chat_username");
if (!username) {
  alert("Vui lòng đăng nhập trước!");
  window.location.href = "login.html";
} 

const ws = new WebSocket("wss://10.152.147.186:3000");

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
        const fileUrl = `https://10.152.147.186:3001${data.file}`;
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
  console.log("📜 Nhận lịch sử:", data);
  
  // C++ gửi user1 và user2, không phải username/to
  const user1 = data.user1;
  const user2 = data.user2;
  
  // Xác định partner (người chat với mình)
  const partner = (user1 === username) ? user2 : user1;
  
  console.log("Partner:", partner, "Current chat:", currentChatUser);
  
  // Chỉ xử lý nếu đang chat với đúng người này
  if (partner === currentChatUser) {
    // Xóa lịch sử cũ
    messageHistory[partner] = [];
    
    // Parse chat history
    // Format từ C++: "[Chat] admin -> ngan: message [timestamp]"
    (data.chatHistory || []).forEach(msg => {
      // Parse string format: "[Chat] sender -> receiver: message [timestamp]"
      const match = msg.match(/\[Chat\] (.+?) (->|<-) (.+?): (.+?) \[(.+?)\]/);
      
      if (match) {
        const sender = match[1].trim();
        const receiver = match[3].trim();
        const message = match[4].trim();
        const timestamp = match[5].trim();
        
        messageHistory[partner].push({
          type: "text",
          from: sender,
          text: message,
          timestamp: timestamp
        });
      } else {
        // Fallback - nếu format không khớp, hiển thị raw
        console.warn("Cannot parse message:", msg);
        messageHistory[partner].push({
          type: "text",
          from: "System",
          text: msg
        });
      }
    });
    
    // Parse file history
    // Format: "[File] sender -> receiver: filename [timestamp]"
    (data.fileHistory || []).forEach(msg => {
      const match = msg.match(/\[File\] (.+?) -> (.+?): (.+?) \[(.+?)\]/);
      
      if (match) {
        const sender = match[1].trim();
        const filename = match[3].trim();
        const timestamp = match[4].trim();
        
        messageHistory[partner].push({
          type: "file",
          from: sender,
          filename: filename,
          url: `https://10.152.147.186:3001/uploads/${filename}`,
          timestamp: timestamp
        });
      }
    });
    
    // Parse call history
    // Format: "[Call] caller -> receiver: status [timestamp]"
    (data.callHistory || []).forEach(msg => {
      const match = msg.match(/\[Call\] (.+?) -> (.+?): (.+?) \[(.+?)\]/);
      
      if (match) {
        const caller = match[1].trim();
        const status = match[3].trim();
        const timestamp = match[4].trim();
        
        messageHistory[partner].push({
          type: "call",
          from: caller,
          status: status,
          timestamp: timestamp
        });
      }
    });
    
    console.log("📝 Parsed history:", messageHistory[partner]);
    
    // Render lịch sử
    renderHistory(partner);
  } else {
    console.log("⚠️ Received history for different user, ignoring");
  }
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

    const res = await fetch("https://10.152.147.186:3001/upload", {
      method: "POST",
      body: formData,
    });

    const result = await res.json();
    console.log("📦 File upload result:", result);

    if (result.success) {
      const fileUrl = "https://10.152.147.186:3001" + result.previewUrl;

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
  
        messages.innerHTML = `<div style="text-align:center; color:#999; padding:20px;">Đang tải lịch sử với ${u}...</div>`;
        ws.send(JSON.stringify({ 
          action: "get_history", 
          from: username,
          to: u 
        }));
        // renderHistory(u); 
      }
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
});*/

const username = sessionStorage.getItem("chat_username");
if (!username) {
  alert("Vui lòng đăng nhập trước!");
  window.location.href = "login.html";
} 

const ws = new WebSocket("wss://10.10.49.115:3000");

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

    /*case "private": {
      const fromUser = data.from === username ? "Tôi" : data.from;
      const partner = data.from === username ? data.to : data.from;
      if (!messageHistory[partner]) messageHistory[partner] = [];

      if (data.file) {
        const fileUrl = data.file.startsWith('http') ? data.file : `https://10.10.49.115:3001${data.file}`;
        messageHistory[partner].push({ 
          type: "file", 
          from: data.from,
          filename: data.filename, 
          url: fileUrl 
        });

        if (partner === currentChatUser) {
          appendFileMessage(fromUser, data.filename, fileUrl);
        } else {
          showUserNotification(partner, "📎 Gửi file mới");
        }
      } else if (data.message) {
        messageHistory[partner].push({ 
          type: "text", 
          from: data.from,
          text: data.message 
        });

        if (partner === currentChatUser) {
          appendMessage(fromUser, data.message);
        } else {
          showUserNotification(partner, "💬 Gửi tin nhắn mới");
        }
      }
      break;
    }*/

    case "private": {
      const fromUser = data.from === username ? "Tôi" : data.from;
      const partner = data.from === username ? data.to : data.from;
      if (!messageHistory[partner]) messageHistory[partner] = [];

      if (data.file) {
        const fileUrl = data.file.startsWith('http') ? data.file : `https://10.10.49.115:3001${data.file}`;
        
        const currentTimestamp = new Date().toISOString().replace('T', ' ').substring(0, 19);
        
        messageHistory[partner].push({ 
          type: "file", 
          from: data.from,
          filename: data.filename, 
          url: fileUrl,
          timestamp: currentTimestamp  
        });

        if (partner === currentChatUser) {
          appendFileMessage(fromUser, data.filename, fileUrl, currentTimestamp);  
        } else {
          showUserNotification(partner, "📎 Gửi file mới");
        }
      } else if (data.message) {
        const currentTimestamp = new Date().toISOString().replace('T', ' ').substring(0, 19);
        
        messageHistory[partner].push({ 
          type: "text", 
          from: data.from,
          text: data.message,
          timestamp: currentTimestamp  
        });

        if (partner === currentChatUser) {
          appendMessage(fromUser, data.message, currentTimestamp);  
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
      console.log("📜 Nhận lịch sử:", data);
      console.log("📦 Full data:", JSON.stringify(data, null, 2));
      
      const user1 = data.user1;
      const user2 = data.user2;
      const partner = (user1 === username) ? user2 : user1;
      
      console.log("Partner:", partner, "Current chat:", currentChatUser);
      
      if (partner === currentChatUser) {
        messageHistory[partner] = [];
        
        (data.chatHistory || []).forEach((msg, index) => {
          console.log(`[${index}] Parsing:`, msg);
          
          const match = msg.match(/\[Chat\] ([^\s]+) (->|<-) ([^\s]+): (.+) \[([^\]]+)\]$/);
          
          if (match) {
            const sender = match[1].trim();
            const direction = match[2]; 
            const receiver = match[3].trim();
            const message = match[4].trim();
            const timestamp = match[5].trim();
            
            console.log(`  ✓ Sender: ${sender}, Direction: ${direction}, Receiver: ${receiver}`);
            console.log(`  ✓ Message: "${message}", Time: ${timestamp}`);
            const actualSender = direction === '<-' ? receiver : sender;
            
            messageHistory[partner].push({
              type: "text",
              from: actualSender,
              text: message,
              timestamp: timestamp
            });
          } else {
            console.warn(`  ✗ Cannot parse message:`, msg);
          }
        });
 
        (data.fileHistory || []).forEach((msg, index) => {
          console.log(`[File ${index}] Parsing:`, msg);
          
          const match = msg.match(/\[File\] ([^\s]+) (->|<-) ([^\s]+): (.+) \[([^\]]+)\]$/);
          
          if (match) {
            const sender = match[1].trim();
            const direction = match[2];
            const receiver = match[3].trim();
            const filename = match[4].trim();
            const timestamp = match[5].trim();
            
            const actualSender = direction === '<-' ? receiver : sender;
            
            console.log(`  ✓ File from ${actualSender}: ${filename}`);
            
            messageHistory[partner].push({
              type: "file",
              from: actualSender,
              filename: filename,
              url: `https://10.10.49.115:3001/uploads/${filename}`,
              timestamp: timestamp
            });
          } else {
            console.warn(`  ✗ Cannot parse file:`, msg);
          }
        });

        (data.callHistory || []).forEach((msg, index) => {
          console.log(`[Call ${index}] Parsing:`, msg);
          
          const match = msg.match(/\[Call\] ([^\s]+) -> ([^\s]+): (.+) \[([^\]]+)\]$/);
          
          if (match) {
            const caller = match[1].trim();
            const receiver = match[2].trim();
            const status = match[3].trim();
            const timestamp = match[4].trim();
            
            console.log(`  ✓ Call from ${caller}: ${status}`);
            
            messageHistory[partner].push({
              type: "call",
              from: caller,
              status: status,
              timestamp: timestamp
            });
          } else {
            console.warn(`  ✗ Cannot parse call:`, msg);
          }
        });
        
        console.log("📝 Parsed history:", messageHistory[partner]);
        
        renderHistory(partner);
      } else {
        console.log("⚠️ Received history for different user, ignoring");
      }
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

/*function sendMessage() {
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

  if (!messageHistory[currentChatUser]) messageHistory[currentChatUser] = [];
  messageHistory[currentChatUser].push({
    type: "text",
    from: username,
    text: msg
  });

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
});*/

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

  if (!messageHistory[currentChatUser]) messageHistory[currentChatUser] = [];
  
  const currentTimestamp = new Date().toISOString().replace('T', ' ').substring(0, 19);
  
  messageHistory[currentChatUser].push({
    type: "text",
    from: username,
    text: msg,
    timestamp: currentTimestamp  
  });

  appendMessage("Tôi", msg, currentTimestamp);  
  msgInput.value = "";
}

fileBtn.onclick = () => {
  if (!currentChatUser) return alert("Chọn người để gửi file!");
  fileInput.click();
};

/*fileInput.onchange = async () => {
  const file = fileInput.files[0];
  if (!file) return;

  showHint(`⏳ Đang gửi ${file.name}...`);

  try {
    const formData = new FormData();
    formData.append("file", file);
    formData.append("from", username);
    formData.append("to", currentChatUser);

    const res = await fetch("https://10.10.49.115:3001/upload", {
      method: "POST",
      body: formData,
    });

    const result = await res.json();
    console.log("📦 File upload result:", result);

    if (result.success) {
      const fileUrl = result.previewUrl.startsWith('http') 
        ? result.previewUrl 
        : `https://10.10.49.115:3001${result.previewUrl}`;

      if (!messageHistory[currentChatUser]) messageHistory[currentChatUser] = [];
      messageHistory[currentChatUser].push({
        type: "file",
        from: username,
        filename: file.name,
        url: fileUrl
      });

      appendFileMessage("Tôi", file.name, fileUrl);
      hideHint();
    } else {
      alert("❌ Lỗi gửi file: " + (result.message || "Unknown error"));
    }
  } catch (err) {
    console.error("Upload error:", err);
    alert("⚠️ Lỗi kết nối tới server upload!\n" + err.message);
  } finally {
    fileInput.value = "";
  }
};*/

fileInput.onchange = async () => {
  const file = fileInput.files[0];
  if (!file) return;

  showHint(`⏳ Đang gửi ${file.name}...`);

  try {
    const formData = new FormData();
    formData.append("file", file);
    formData.append("from", username);
    formData.append("to", currentChatUser);

    const res = await fetch("https://10.10.49.115:3001/upload", {
      method: "POST",
      body: formData,
    });

    const result = await res.json();
    console.log("📦 File upload result:", result);

    if (result.success) {
      const fileUrl = result.previewUrl.startsWith('http') 
        ? result.previewUrl 
        : `https://10.10.49.115:3001${result.previewUrl}`;

      if (!messageHistory[currentChatUser]) messageHistory[currentChatUser] = [];
      
      const currentTimestamp = new Date().toISOString().replace('T', ' ').substring(0, 19);
      
      messageHistory[currentChatUser].push({
        type: "file",
        from: username,
        filename: file.name,
        url: fileUrl,
        timestamp: currentTimestamp  
      });

      appendFileMessage("Tôi", file.name, fileUrl, currentTimestamp);  
      hideHint();
    } else {
      alert("❌ Lỗi gửi file: " + (result.message || "Unknown error"));
    }
  } catch (err) {
    console.error("Upload error:", err);
    alert("⚠️ Lỗi kết nối tới server upload!\n" + err.message);
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
  
        messages.innerHTML = `<div style="text-align:center; color:#999; padding:20px;">Đang tải lịch sử với ${u}...</div>`;
      
        ws.send(JSON.stringify({ 
          action: "get_history", 
          user1: username,
          user2: u 
        }));
      };
      
      userList.appendChild(div);
    }
  });
}

const style = document.createElement('style');
style.textContent = `
    @keyframes fadeIn {
        from {
            opacity: 0;
            transform: translateY(10px);
        }
        to {
            opacity: 1;
            transform: translateY(0);
        }
    }
    
    .bubble {
        transition: all 0.2s ease;
    }
`;
document.head.appendChild(style);


/*function appendMessage(from, msg) {
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
}*/

function appendMessage(from, msg, timestamp = null) {
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
  
  if (timestamp) {
    bubble.title = formatTimestamp(timestamp);
    bubble.style.cursor = 'help';
    
    bubble.onmouseenter = function() {
      this.style.transform = 'scale(1.02)';
      this.style.boxShadow = '0 4px 8px rgba(0,0,0,0.2)';
    };
    bubble.onmouseleave = function() {
      this.style.transform = 'scale(1)';
      this.style.boxShadow = 'none';
    };
  }
  
  wrapper.append(avatar, bubble);
  messages.appendChild(wrapper);
  wrapper.scrollIntoView({ behavior: "smooth" });

  bubble.animate([{ backgroundColor: "#e0f7ff" }, { backgroundColor: "transparent" }], {
    duration: 800,
  });
}

function appendFileMessage(from, filename, fileUrl, timestamp = null) {
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

  if (timestamp) {
    bubble.title = formatTimestamp(timestamp);
    bubble.style.cursor = 'help';
    
    bubble.onmouseenter = function() {
      this.style.transform = 'scale(1.02)';
      this.style.boxShadow = '0 4px 8px rgba(0,0,0,0.2)';
    };
    bubble.onmouseleave = function() {
      this.style.transform = 'scale(1)';
      this.style.boxShadow = 'none';
    };
  }

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

/*function renderHistory(user) {
  messages.innerHTML = "";
  const history = messageHistory[user];
  
  if (!history || history.length === 0) {
    messages.innerHTML = `<div style="text-align:center; color:#999; padding:20px;">Chưa có tin nhắn nào 📭</div>`;
    return;
  }

  history.forEach(item => {
    const fromDisplay = item.from === username ? "Tôi" : item.from;

    if (item.type === "text") {
      appendMessage(fromDisplay, item.text);
    } else if (item.type === "file") {
      appendFileMessage(fromDisplay, item.filename, item.url);
    } else if (item.type === "call") {
      const callText = `${fromDisplay} ${
        item.status === "accepted" ? "📞 gọi thành công" :
        item.status === "rejected" ? "❌ từ chối cuộc gọi" : 
        item.status === "incoming" ? "📲 bắt đầu cuộc gọi" :
        "📞 " + item.status
      }`;
      appendMessage(fromDisplay, callText);
    }
  });
}*/

function renderHistory(user) {
  messages.innerHTML = "";
  const history = messageHistory[user];
  
  if (!history || history.length === 0) {
    messages.innerHTML = `<div style="text-align:center; color:#999; padding:20px;">Chưa có tin nhắn nào 📭</div>`;
    return;
  }

  const sortedHistory = [...history].sort((a, b) => {
    if (!a.timestamp || !b.timestamp) return 0;
    return new Date(a.timestamp) - new Date(b.timestamp);
  });

  sortedHistory.forEach(item => {
    const fromDisplay = item.from === username ? "Tôi" : item.from;

    if (item.type === "text") {
      appendMessage(fromDisplay, item.text, item.timestamp);
    } 
    else if (item.type === "file") {
      appendFileMessage(fromDisplay, item.filename, item.url, item.timestamp);
    } 
    else if (item.type === "call") {
      const callText = `${fromDisplay} ${
        item.status === "accepted" ? "📞 gọi thành công" :
        item.status === "rejected" ? "❌ từ chối cuộc gọi" : 
        item.status === "incoming" ? "📲 bắt đầu cuộc gọi" :
        "📞 " + item.status
      }`;
      appendMessage(fromDisplay, callText, item.timestamp);
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

function parseHistoryMessage(rawMessage) {
    const chatPattern = /\[Chat\]\s+(.+?)\s+->\s+(.+?):\s+(.+?)\s+\[(.+?)\]/;
    const match = rawMessage.match(chatPattern);
    
    if (!match) {
        console.warn("Cannot parse message:", rawMessage);
        return null;
    }
    
    return {
        sender: match[1],
        receiver: match[2],
        message: match[3],
        timestamp: match[4] 
    };
}
function formatTimestamp(timestamp) {
    const date = new Date(timestamp.replace(' ', 'T')); 
    
    if (isNaN(date.getTime())) {
        return timestamp; 
    }
    
    const now = new Date();
    const today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
    const messageDate = new Date(date.getFullYear(), date.getMonth(), date.getDate());
    
    const timeStr = date.toLocaleTimeString('vi-VN', { 
        hour: '2-digit', 
        minute: '2-digit',
        second: '2-digit'
    });
    
    if (messageDate.getTime() === today.getTime()) {
        return `Hôm nay lúc ${timeStr}`;
    }
    const yesterday = new Date(today);
    yesterday.setDate(yesterday.getDate() - 1);
    if (messageDate.getTime() === yesterday.getTime()) {
        return `Hôm qua lúc ${timeStr}`;
    }
    
    const weekAgo = new Date(today);
    weekAgo.setDate(weekAgo.getDate() - 7);
    if (messageDate.getTime() > weekAgo.getTime()) {
        const dayNames = ['Chủ nhật', 'Thứ hai', 'Thứ ba', 'Thứ tư', 'Thứ năm', 'Thứ sáu', 'Thứ bảy'];
        return `${dayNames[date.getDay()]} lúc ${timeStr}`;
    }
    
    const dateStr = date.toLocaleDateString('vi-VN', {
        day: '2-digit',
        month: '2-digit',
        year: 'numeric'
    });
    return `${dateStr} lúc ${timeStr}`;
}

/*function displayMessage(sender, message, timestamp, isOwn = false) {
    const messageDiv = document.createElement('div');
    messageDiv.classList.add('message-container');
    messageDiv.style.cssText = `
        display: flex;
        justify-content: ${isOwn ? 'flex-end' : 'flex-start'};
        margin-bottom: 15px;
        align-items: flex-end;
        animation: fadeIn 0.3s ease-in;
    `;
    
    // Avatar
    const avatar = document.createElement('div');
    avatar.classList.add('avatar');
    avatar.style.cssText = `
        width: 35px;
        height: 35px;
        border-radius: 50%;
        background: ${isOwn ? '#10b981' : '#3b82f6'};
        display: flex;
        align-items: center;
        justify-content: center;
        color: white;
        font-weight: bold;
        font-size: 14px;
        margin: ${isOwn ? '0 0 0 10px' : '0 10px 0 0'};
        flex-shrink: 0;
        box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    `;
    avatar.textContent = sender.charAt(0).toUpperCase();
    
    // Content wrapper
    const contentDiv = document.createElement('div');
    contentDiv.style.cssText = `
        max-width: 60%;
        display: flex;
        flex-direction: column;
        align-items: ${isOwn ? 'flex-end' : 'flex-start'};
    `;
    
    // Sender name (chỉ hiện khi không phải tin nhắn của mình)
    if (!isOwn) {
        const senderName = document.createElement('div');
        senderName.style.cssText = `
            font-size: 12px;
            color: #6b7280;
            margin-bottom: 4px;
            font-weight: 500;
        `;
        senderName.textContent = sender;
        contentDiv.appendChild(senderName);
    }
    
    // Message content với TOOLTIP
    const messageContent = document.createElement('div');
    messageContent.style.cssText = `
        background: ${isOwn ? '#10b981' : '#f3f4f6'};
        color: ${isOwn ? 'white' : '#1f2937'};
        padding: 10px 15px;
        border-radius: ${isOwn ? '15px 15px 5px 15px' : '15px 15px 15px 5px'};
        word-wrap: break-word;
        box-shadow: 0 1px 2px rgba(0,0,0,0.1);
        cursor: help;
        position: relative;
        transition: all 0.2s ease;
    `;
    
    messageContent.title = formatTimestamp(timestamp);
    messageContent.textContent = message;
    
    // Hiệu ứng hover
    messageContent.onmouseenter = function() {
        this.style.transform = 'scale(1.02)';
        this.style.boxShadow = '0 4px 8px rgba(0,0,0,0.15)';
    };
    messageContent.onmouseleave = function() {
        this.style.transform = 'scale(1)';
        this.style.boxShadow = '0 1px 2px rgba(0,0,0,0.1)';
    };
    
    contentDiv.appendChild(messageContent);
    
    // Ghép avatar và content
    if (isOwn) {
        messageDiv.appendChild(contentDiv);
        messageDiv.appendChild(avatar);
    } else {
        messageDiv.appendChild(avatar);
        messageDiv.appendChild(contentDiv);
    }
    
    return messageDiv;
}

function displayFileMessage(sender, filename, filepath, timestamp, isOwn = false) {
    const messageDiv = document.createElement('div');
    messageDiv.classList.add('message-container');
    messageDiv.style.cssText = `
        display: flex;
        justify-content: ${isOwn ? 'flex-end' : 'flex-start'};
        margin-bottom: 15px;
        align-items: flex-end;
        animation: fadeIn 0.3s ease-in;
    `;
    
    const avatar = document.createElement('div');
    avatar.classList.add('avatar');
    avatar.style.cssText = `
        width: 35px;
        height: 35px;
        border-radius: 50%;
        background: ${isOwn ? '#10b981' : '#3b82f6'};
        display: flex;
        align-items: center;
        justify-content: center;
        color: white;
        font-weight: bold;
        margin: ${isOwn ? '0 0 0 10px' : '0 10px 0 0'};
        flex-shrink: 0;
    `;
    avatar.textContent = sender.charAt(0).toUpperCase();
    
    const contentDiv = document.createElement('div');
    contentDiv.style.cssText = `
        max-width: 60%;
        display: flex;
        flex-direction: column;
        align-items: ${isOwn ? 'flex-end' : 'flex-start'};
    `;
    
    if (!isOwn) {
        const senderName = document.createElement('div');
        senderName.style.cssText = `
            font-size: 12px;
            color: #6b7280;
            margin-bottom: 4px;
            font-weight: 500;
        `;
        senderName.textContent = sender;
        contentDiv.appendChild(senderName);
    }
    
    // File content
    const fileContent = document.createElement('a');
    fileContent.href = filepath;
    fileContent.target = '_blank';
    fileContent.download = filename;
    fileContent.style.cssText = `
        background: ${isOwn ? '#10b981' : '#f3f4f6'};
        color: ${isOwn ? 'white' : '#1f2937'};
        padding: 10px 15px;
        border-radius: ${isOwn ? '15px 15px 5px 15px' : '15px 15px 15px 5px'};
        text-decoration: none;
        display: flex;
        align-items: center;
        gap: 10px;
        box-shadow: 0 1px 2px rgba(0,0,0,0.1);
        cursor: pointer;
        transition: all 0.2s ease;
    `;
    
    fileContent.title = formatTimestamp(timestamp);
    
    fileContent.innerHTML = `
        <span style="font-size: 20px;">📎</span>
        <span style="font-weight: 500;">${filename}</span>
    `;
    
    fileContent.onmouseenter = function() {
        this.style.transform = 'scale(1.02)';
        this.style.boxShadow = '0 4px 8px rgba(0,0,0,0.15)';
    };
    fileContent.onmouseleave = function() {
        this.style.transform = 'scale(1)';
        this.style.boxShadow = '0 1px 2px rgba(0,0,0,0.1)';
    };
    
    contentDiv.appendChild(fileContent);
    
    if (isOwn) {
        messageDiv.appendChild(contentDiv);
        messageDiv.appendChild(avatar);
    } else {
        messageDiv.appendChild(avatar);
        messageDiv.appendChild(contentDiv);
    }
    
    return messageDiv;
}

function displayChatHistory(historyData, currentUser) {
    const chatBox = document.getElementById('chatBox');
    if (!chatBox) {
        console.error('Chat box element not found!');
        return;
    }
    
    chatBox.innerHTML = ''; // Clear
    
    // Kiểm tra có dữ liệu không
    const hasChat = historyData.chatHistory && historyData.chatHistory.length > 0;
    const hasFile = historyData.fileHistory && historyData.fileHistory.length > 0;
    
    if (!hasChat && !hasFile) {
        chatBox.innerHTML = `
            <div style="
                text-align: center; 
                color: #9ca3af; 
                padding: 40px 20px;
                font-size: 14px;
            ">
                <div style="font-size: 48px; margin-bottom: 10px;">💬</div>
                <div>Chưa có tin nhắn nào</div>
                <div style="font-size: 12px; margin-top: 5px;">Hãy bắt đầu cuộc trò chuyện!</div>
            </div>
        `;
        return;
    }
    
    // Tạo mảng tổng hợp tất cả tin nhắn và file
    const allMessages = [];
    
    // Thêm chat messages
    if (hasChat) {
        historyData.chatHistory.forEach(rawMsg => {
            const parsed = parseHistoryMessage(rawMsg);
            if (parsed) {
                allMessages.push({
                    type: 'chat',
                    ...parsed,
                    timestamp: new Date(parsed.timestamp.replace(' ', 'T'))
                });
            }
        });
    }
    
    // Thêm file messages
    if (hasFile) {
        historyData.fileHistory.forEach(fileMsg => {
            // Parse file format: [File] sender -> receiver: filename [timestamp]
            const filePattern = /\[File\]\s+(.+?)\s+->\s+(.+?):\s+(.+?)\s+\[(.+?)\]/;
            const match = fileMsg.match(filePattern);
            if (match) {
                allMessages.push({
                    type: 'file',
                    sender: match[1],
                    receiver: match[2],
                    filename: match[3],
                    timestamp: new Date(match[4].replace(' ', 'T'))
                });
            }
        });
    }
    
    // Sắp xếp theo thời gian
    allMessages.sort((a, b) => a.timestamp - b.timestamp);
    
    // Hiển thị từng message
    allMessages.forEach(msg => {
        const isOwn = msg.sender === currentUser;
        let msgElement;
        
        if (msg.type === 'chat') {
            msgElement = displayMessage(
                msg.sender, 
                msg.message, 
                msg.timestamp.toISOString().replace('T', ' ').substring(0, 19),
                isOwn
            );
        } else if (msg.type === 'file') {
            msgElement = displayFileMessage(
                msg.sender,
                msg.filename,
                `/download/${msg.filename}`, // Adjust path as needed
                msg.timestamp.toISOString().replace('T', ' ').substring(0, 19),
                isOwn
            );
        }
        
        if (msgElement) {
            chatBox.appendChild(msgElement);
        }
    });
    
    // Scroll to bottom
    chatBox.scrollTop = chatBox.scrollHeight;
}

const style = document.createElement('style');
style.textContent = `
    @keyframes fadeIn {
        from {
            opacity: 0;
            transform: translateY(10px);
        }
        to {
            opacity: 1;
            transform: translateY(0);
        }
    }
    
    
    [title] {
        position: relative;
    }
`;
document.head.appendChild(style);

// Khi nhận history response từ WebSocket:
/*
ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    
    if (data.action === 'history_response') {
        displayChatHistory(data, currentUsername);
    }
};
*/

const profileTrigger = document.getElementById('profile-trigger');
const profileMenu = document.getElementById('profile-menu');
const logoutButton = document.getElementById('logout-button');

if (profileTrigger && profileMenu) {
    profileTrigger.addEventListener('click', (e) => {
        e.stopPropagation(); 
        profileMenu.classList.toggle('show'); 
    });
   
    document.addEventListener('click', (event) => {
        const isClickInside = profileTrigger.contains(event.target) || profileMenu.contains(event.target);
        
        if (!isClickInside) {
            profileMenu.classList.remove('show');
        }
    });
}

if (logoutButton) {
    logoutButton.addEventListener('click', (e) => {
        e.preventDefault(); 
        localStorage.removeItem('username'); 
        sessionStorage.clear(); 
        if (window.chatWebSocket) {
            window.chatWebSocket.send(JSON.stringify({ action: "logout" }));
            window.chatWebSocket.close();
            delete window.chatWebSocket; 
        }
        window.location.href = 'login.html'; 
    });
}
