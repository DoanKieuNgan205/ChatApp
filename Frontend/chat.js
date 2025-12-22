const username = sessionStorage.getItem("chat_username");
if (!username) {
  alert("Vui lòng đăng nhập trước!");
  window.location.href = "login.html";
} 

const ws = new WebSocket("wss://10.246.147.186:3000");

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
        const fileUrl = data.file.startsWith('http') ? data.file : `https://10.246.147.186:3001${data.file}`;
        
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
  
  const user1 = data.user1;
  const user2 = data.user2;
  const partner = (user1 === username) ? user2 : user1;
  
  console.log("Partner:", partner, "Current chat:", currentChatUser);
  
  if (partner === currentChatUser) {
    messageHistory[partner] = [];
    (data.chatHistory || []).forEach((msg, index) => {

      const match = msg.match(/\[Chat\] ([^\s]+) -> ([^\s]+): (.+) \[([^\]]+)\]$/);
      
      if (match) {
        const sender = match[1].trim();
        const receiver = match[2].trim();
        const message = match[3].trim();
        const timestamp = match[4].trim();
        
        messageHistory[partner].push({
          type: "text",
          from: sender,
          text: message,
          timestamp: timestamp
        });
      }
    });

    (data.fileHistory || []).forEach((msg) => {
      const match = msg.match(/\[File\] ([^\s]+) -> ([^\s]+): (.+) \[([^\]]+)\]$/);
      
      if (match) {
        const sender = match[1].trim();
        const filename = match[3].trim();
        const timestamp = match[4].trim();
        
        const fileUrl = `https://10.246.147.186:3001/uploads/${filename}`;
        
        messageHistory[partner].push({
          type: "file",
          from: sender,
          filename: filename,
          url: fileUrl,
          timestamp: timestamp
        });
      }
    });

    (data.callHistory || []).forEach((msg, index) => {
      console.log(`[Call ${index}] Parsing:`, msg);

      const matchWithDuration = msg.match(/\[Call\] ([^\s]+) -> ([^\s]+): (\d+)s \((.+?)\) \[([^\]]+)\]$/);
    
      const matchNoDuration = msg.match(/\[Call\] ([^\s]+) -> ([^\s]+): (.+?) \[([^\]]+)\]$/);

      let caller, receiver, status, timestamp, duration = 0;

      if (matchWithDuration) {
        caller = matchWithDuration[1].trim();
        receiver = matchWithDuration[2].trim();
        duration = parseInt(matchWithDuration[3]);  
        status = matchWithDuration[4].trim();        
        timestamp = matchWithDuration[5].trim();
        
        console.log(`  ✓ Parsed: ${caller} -> ${receiver}, ${duration}s, status=${status}`);
      } else if (matchNoDuration) {
        caller = matchNoDuration[1].trim();
        receiver = matchNoDuration[2].trim();
        status = matchNoDuration[3].trim();
        timestamp = matchNoDuration[4].trim();
        duration = 0;
        
        console.log(`  ✓ Parsed (no duration): ${caller} -> ${receiver}, status=${status}`);
      } else {
        console.warn(`  ✗ Cannot parse call:`, msg);
        return;
      }

      const direction = (caller === username) ? "outgoing" : "incoming";
      const from = caller;

      messageHistory[partner].push({
        type: "call",
        from: from,
        status: status,
        timestamp: timestamp,
        direction: direction,
        duration: duration  
      });
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
        break;

    case "CALL_ENDED":
        if (!messageHistory[data.from]) messageHistory[data.from] = [];
        const currentTimestamp2 = new Date().toISOString().replace('T', ' ').substring(0, 19);
        
        messageHistory[data.from].push({ 
            type: "call", 
            from: data.from, 
            status: "ended",
            timestamp: currentTimestamp2
        });
        
        if (currentChatUser === data.from) {
            const callText = "📴 Cuộc gọi đã kết thúc";
            appendMessage("System", callText, currentTimestamp2);
            renderHistory(currentChatUser);
        }
        break;

    case "CALL_HISTORY_SAVED":
      console.log("📞 CALL_HISTORY_SAVED received:", data);
      
      if (data.status === "success") {
          console.log("✅ Call history saved successfully");
          
          /*const otherUser = (data.from === username) ? data.to : data.from;
          
          console.log(`[DEBUG] otherUser: ${otherUser}, currentChatUser: ${currentChatUser}`);
          
          if (otherUser && otherUser === currentChatUser) {
              console.log(`🔄 Auto-reloading history with ${otherUser}...`);
              
              setTimeout(() => {
                  ws.send(JSON.stringify({ 
                      action: "get_history", 
                      user1: username,
                      user2: currentChatUser
                  }));
                  console.log(`📤 Sent get_history request for ${username} <-> ${currentChatUser}`);
              }, 300);
          } else {
              console.log(`⏭️ Not reloading - not currently chatting with ${otherUser}`);
          }
      } else {
          console.error("❌ Failed to save call history:", data.message || "Unknown error");
      }*/

      const partnerOfThisCall = (data.from === username) ? data.to : data.from;

        if (currentChatUser === partnerOfThisCall) {
            console.log("🔄 Đang tải lại lịch sử mới nhất từ server...");
            
            setTimeout(() => {
                ws.send(JSON.stringify({ 
                    action: "get_history", 
                    user1: username,
                    user2: currentChatUser 
                }));
                console.log(`📤 Đã gửi request lấy lịch sử mới cho ${username} <-> ${currentChatUser}`);
            }, 500); 
        } else {
            console.log(`⏭️ Không reload - không đang chat với ${partnerOfThisCall}`);
        }
    } else {
        console.error("❌ Failed to save call history:", data.message || "Unknown error");
    }
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

sendBtn.onclick = sendMessage;

msgInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !e.shiftKey) {
    e.preventDefault();
    sendMessage();
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

    const res = await fetch("https://10.246.147.186:3001/upload", {
      method: "POST",
      body: formData,
    });

    const result = await res.json();
    console.log("📦 File upload result:", result);

    if (result.success) {
      const fileUrl = result.previewUrl.startsWith('http') 
        ? result.previewUrl 
        : `https://10.246.147.186:3001${result.previewUrl}`;

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

function renderHistory(user) {
  messages.innerHTML = "";
  const history = messageHistory[user];
  
  if (!history || history.length === 0) {
    messages.innerHTML = `<div style="text-align:center; color:#999; padding:20px;">Chưa có tin nhắn nào 📭</div>`;
    return;
  }

  
  const sortedHistory = [...history].sort((a, b) => {
    if (!a.timestamp || !b.timestamp) return 0;
    const dateA = new Date(a.timestamp.replace(' ', 'T'));
    const dateB = new Date(b.timestamp.replace(' ', 'T'));
     return dateA - dateB;
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
      const direction = item.direction || ((item.from === username) ? "outgoing" : "incoming");
      const duration = item.duration || 0;
      appendCallMessage(item.from, item.status, item.timestamp, direction, duration);
    }
  });

  setTimeout(() => {
        messages.scrollTop = messages.scrollHeight;
    }, 100);
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
  if (!incomingCallFrom) {
    console.warn("[CHAT] ⚠️ No incoming call to accept");
    return;
  }

  console.log(`[CHAT] ✅ Accepting call from ${incomingCallFrom}`);
  
  const url = `voice_call.html?me=${encodeURIComponent(username)}&to=${encodeURIComponent(incomingCallFrom)}&action=accept`;
  window.open(url, "voiceCall", "width=400,height=300,resizable=yes");

  incomingCallModal.className = 'call-modal-hidden';
  incomingCallFrom = null;
  
  console.log("[CHAT] 🧹 Cleared incoming call state");
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
    
    const diffTime = today.getTime() - messageDate.getTime();
    const diffDays = Math.floor(diffTime / (1000 * 60 * 60 * 24));

    const timeStr = date.toLocaleTimeString('vi-VN', { 
        hour: '2-digit', 
        minute: '2-digit',
        second: '2-digit'
    });
    

    if (diffDays === 0) {
        return `Hôm nay lúc ${timeStr}`;
    }

    if (diffDays === 1) {
        return `Hôm qua lúc ${timeStr}`;
    }

    if (diffDays < 7) {
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

function appendCallMessage(from, status, timestamp = null, direction = "outgoing", duration = 0) {
  const wrapper = document.createElement("div");
  wrapper.classList.add("message");

  const bubble = document.createElement("div");
  bubble.classList.add("bubble", "call-bubble");

  const avatar = document.createElement("div");
  avatar.classList.add("avatar");

  /*const isOutgoing = (direction === "outgoing");
  
  if (isOutgoing) {
    wrapper.classList.add("right");
    avatar.textContent = "🧑";
  } else {
    wrapper.classList.add("left");
    avatar.textContent = from[0].toUpperCase();
  }*/

  const isMe = (from === username || from === "Tôi");
  
  if (isMe) {
    wrapper.classList.add("right");
    avatar.textContent = "🧑";
    from = "Tôi"; 
  } else {
    wrapper.classList.add("left");
    avatar.textContent = from[0].toUpperCase();
  }

  let icon = "📞";
  let callText = "";
  let callClass = "";
  let arrow = "";
  
  if (direction === "outgoing") {
    arrow = "→";
    switch(status) {
      case "started":
        icon = "📞";
        callText = "Cuộc gọi đi";
        callClass = "call-outgoing";
        break;
      case "accepted":
        icon = "✅";
        callText = "Cuộc gọi đi";
        callClass = "call-outgoing-accepted";
        break;
      case "rejected":
        icon = "❌";
        callText = "Cuộc gọi bị từ chối";
        callClass = "call-rejected-outgoing";
        break;
      case "ended":
      case "completed":
        icon = "📴";
        callText = "Cuộc gọi đi";
        callClass = "call-ended";
        break;
    }
  } else {
    arrow = "←";
    switch(status) {
      case "started":
      case "incoming":
        icon = "📲";
        callText = "Cuộc gọi đến";
        callClass = "call-incoming";
        break;
      case "accepted":
        icon = "✅";
        callText = "Cuộc gọi đến";
        callClass = "call-incoming-accepted";
        break;
      case "rejected":
        icon = "❌";
        callText = "Cuộc gọi nhỡ";
        callClass = "call-missed";
        break;
      case "ended":
      case "completed":
        icon = "📴";
        callText = "Cuộc gọi đến";
        callClass = "call-ended";
        break;
    }
  }


  let timeText = "";
  if (timestamp) {
    try {
      const date = new Date(timestamp.replace(' ', 'T'));
      
      if (isNaN(date.getTime())) {
        timeText = "Không xác định";
      } else {
        const now = new Date();
        const diffMs = now - date;
        const diffSeconds = Math.floor(diffMs / 1000);
        const diffMins = Math.floor(diffSeconds / 60);
        const diffHours = Math.floor(diffMins / 60);
        const diffDays = Math.floor(diffHours / 24);
        
        if (diffSeconds < 60) {
          timeText = "Vừa xong";
        } else if (diffMins < 60) {
          timeText = diffMins + " phút";
        } else if (diffHours < 24) {
          timeText = diffHours + " giờ";
        } else if (diffDays === 1) {
          timeText = "Hôm qua";
        } else if (diffDays < 7) {
          timeText = diffDays + " ngày";
        } else {
          timeText = date.toLocaleDateString('vi-VN', { day: '2-digit', month: '2-digit' });
        }
      }
    } catch (e) {
      console.error("Error parsing timestamp:", timestamp, e);
      timeText = "Lỗi thời gian";
    }
  }


  let durationText = "";
  if (duration && duration > 0) {
    const mins = Math.floor(duration / 60);
    const secs = duration % 60;
    if (mins > 0) {
      durationText = ` • ${mins}:${String(secs).padStart(2, '0')}`;
    } else {
      durationText = ` • ${secs}s`;
    }
  }

  bubble.innerHTML = `
    <div class="call-info ${callClass}">
      <div class="call-icon">${icon}</div>
      <div class="call-details">
        <div class="call-title">${arrow} ${callText}${durationText}</div>
        <div class="call-time">${timeText}</div>
      </div>
    </div>
  `;
  
  if (timestamp) {
    bubble.title = formatTimestamp(timestamp);
    bubble.style.cursor = 'help';
  }
  
  wrapper.append(avatar, bubble);
  messages.appendChild(wrapper);
  wrapper.scrollIntoView({ behavior: "smooth" });
}


document.addEventListener('DOMContentLoaded', function() {
    console.log("🔧 Initializing profile management...");

    const profileTrigger = document.getElementById('profile-trigger');
    const profileMenu = document.getElementById('profile-menu');
    const logoutButton = document.getElementById('logout-button');
    const viewProfileBtn = document.getElementById("view-profile");
    const changePasswordBtn = document.getElementById("change-password-link");

    if (profileTrigger && profileMenu) {
        console.log("✅ Profile menu elements found");
        
        profileTrigger.addEventListener('click', (e) => {
            e.stopPropagation(); 
            profileMenu.classList.toggle('show'); 
            console.log("🔽 Profile menu toggled");
        });

        document.addEventListener('click', (event) => {
            const isClickInside = profileTrigger.contains(event.target) || profileMenu.contains(event.target);
            
            if (!isClickInside && profileMenu.classList.contains('show')) {
                profileMenu.classList.remove('show');
                console.log("🔼 Profile menu closed");
            }
        });
    } else {
        console.warn("⚠️ Profile menu elements not found");
        if (!profileTrigger) console.warn("  - Missing: profile-trigger");
        if (!profileMenu) console.warn("  - Missing: profile-menu");
    }


    if (viewProfileBtn) {
        console.log("✅ Found view-profile button");
        viewProfileBtn.addEventListener("click", (e) => {
            e.preventDefault();
            console.log("🔄 Navigating to profile.html");
            window.location.href = "profile.html"; 
        });
    } else {
        console.warn("⚠️ view-profile button not found");
    }

    if (changePasswordBtn) {
        console.log("✅ Found change-password-link button");
        changePasswordBtn.addEventListener("click", (e) => {
            e.preventDefault();
            console.log("🔄 Navigating to change_password.html");
            window.location.href = "change_password.html";
        });
    } else {
        console.warn("⚠️ change-password-link button not found");
    }

    if (logoutButton) {
        console.log("✅ Found logout button");
        logoutButton.addEventListener('click', (e) => {
            e.preventDefault();
            
            console.log("🚪 Logging out...");

            localStorage.removeItem('username'); 
            sessionStorage.clear();
            console.log("🗑️ Storage cleared");

            if (typeof ws !== 'undefined' && ws.readyState === WebSocket.OPEN) {
                try {
                    ws.send(JSON.stringify({ action: "logout" }));
                    ws.close();
                    console.log("🔌 WebSocket closed");
                } catch (err) {
                    console.error("⚠️ Error closing WebSocket:", err);
                }
            }

            window.location.href = 'login.html'; 
        });
    } else {
        console.warn("⚠️ logout button not found");
    }
    
    console.log("✅ Profile management initialized successfully");
});


const callStyle = document.createElement('style');
callStyle.textContent = `

.call-bubble {
  background: #f0f2f5 !important;
  padding: 8px 12px !important;
  min-width: 200px;
  max-width: 280px;
}

.call-info {
  display: flex;
  align-items: center;
  gap: 12px;
}

.call-icon {
  font-size: 24px;
  width: 40px;
  height: 40px;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 50%;
  background: white;
  flex-shrink: 0;
}

.call-details {
  flex: 1;
  min-width: 0;
}

.call-title {
  font-weight: 600;
  font-size: 14px;
  color: #050505;
  margin-bottom: 2px;
}

.call-time {
  font-size: 12px;
  color: #65676b;
}


.call-outgoing .call-icon,
.call-outgoing-accepted .call-icon {
  background: #e3f2fd;
}

.call-incoming .call-icon,
.call-incoming-accepted .call-icon {
  background: #e8f5e9;
}

.call-rejected-outgoing .call-icon {
  background: #ffebee;
}

.call-missed .call-icon {
  background: #fff3e0;
}

.call-ended .call-icon {
  background: #f5f5f5;
}


.call-bubble {
  animation: slideIn 0.3s ease;
}

@keyframes slideIn {
  from {
    opacity: 0;
    transform: translateY(10px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}
`;
document.head.appendChild(callStyle);

