// 1. Khai báo các thành phần giao diện (Đây là phần bạn đang thiếu)
const usernameInput = document.getElementById("prof-username");
const emailInput = document.getElementById("prof-email");
const saveBtn = document.getElementById("save-profile"); // ID trong HTML là save-profile

// 2. Lấy thông tin người dùng hiện tại
const targetUser = sessionStorage.getItem("chat_username");

if (!targetUser) {
    alert("Vui lòng đăng nhập lại!");
    window.location.href = "login.html";
}

// 3. Kết nối WebSocket
const ws = new WebSocket("wss://10.246.147.186:3000");

ws.onopen = () => {
    console.log("✅ Kết nối Gateway thành công");
    ws.send(JSON.stringify({
        action: "get_user_info",
        username: targetUser
    }));
};

// 4. Xử lý dữ liệu nhận được
ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    console.log("📩 Nhận dữ liệu:", data);

    if (data.action === "my_profile_response") {
        if (usernameInput && emailInput) {
            usernameInput.value = data.username || "";
            emailInput.value = data.email || "";
        }
    } 
    else if (data.action === "update_profile_response") { 
        if (data.status === "success") {
            alert("✅ Cập nhật thành công!");
            // Cập nhật lại sessionStorage nếu cần
        } else {
            alert("❌ Lỗi: " + data.message);
        }
    }
};

// 5. Xử lý khi nhấn nút Lưu (Sửa lỗi biến saveBtn)
if (saveBtn) {
    saveBtn.onclick = () => {
        const newEmail = emailInput.value.trim();
        if (!newEmail) return alert("Vui lòng nhập email!");

        console.log("📤 Đang gửi yêu cầu đổi email...");
        ws.send(JSON.stringify({
            action: "update_email",
            username: targetUser,
            newEmail: newEmail
        }));
    };
}