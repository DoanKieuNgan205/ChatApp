let socket;

document.getElementById("loginForm").addEventListener("submit", (e) => {
  e.preventDefault();

  const username = document.getElementById("username").value.trim();
  const password = document.getElementById("password").value.trim();
  const status = document.getElementById("status");

  if (!socket || socket.readyState !== WebSocket.OPEN) {
    socket = new WebSocket("ws://localhost:3000"); // kết nối gateway

    socket.onopen = () => {
      console.log("Đã kết nối tới Gateway");
      socket.send(JSON.stringify({
        action: "login",
        username,
        password
      }));
    };

    socket.onmessage = (event) => {
      const data = JSON.parse(event.data);
      if (data.status === "success") {
        status.style.color = "green";
        status.textContent = "Đăng nhập thành công!";
        // Có thể chuyển sang trang chat.html ở đây
      } else {
        status.style.color = "red";
        status.textContent = "Sai tài khoản hoặc mật khẩu!";
      }
    };

    socket.onerror = (err) => {
      status.textContent = "Lỗi kết nối Gateway!";
      console.error(err);
    };
  }
});


// Khi nhấn nút đăng xuất
document.getElementById("logoutBtn").addEventListener("click", () => {
  if (socket && currentUser) {
    const logoutMsg = {
      action: "logout",
      username: currentUser
    };
    socket.send(JSON.stringify(logoutMsg));
  }

  // Xóa session và quay lại màn hình đăng nhập
  currentUser = null;
  document.getElementById("chatContainer").style.display = "none";
  document.getElementById("loginContainer").style.display = "block";
});
