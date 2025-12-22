const targetUser = sessionStorage.getItem("chat_username");
if (!targetUser) window.location.href = "login.html";

const ws = new WebSocket("wss://10.246.147.186:3000");

const oldPassInput = document.getElementById("old-pass");
const newPassInput = document.getElementById("new-pass");
const confirmPassInput = document.getElementById("confirm-pass");
const submitBtn = document.getElementById("submit-change-pwd");

ws.onopen = () => {
    // Luôn join_chat hoặc định danh lại để Gateway map đúng socket
    ws.send(JSON.stringify({ action: "join_chat", username: targetUser }));
};

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    if (data.action === "update_profile_response") {
        if (data.status === "success") {
            alert("✅ Đổi mật khẩu thành công! Vui lòng đăng nhập lại.");
            sessionStorage.clear();
            window.location.href = "login.html";
        } else {
            alert("❌ Lỗi: " + data.message);
        }
    }
};
submitBtn.onclick = () => {
    const oldPass = document.getElementById("old-pass").value;
    const newPass = document.getElementById("new-pass").value;
    const confirmPass = document.getElementById("confirm-pass").value;

    if (!oldPass || !newPass || !confirmPass) {
        return alert("Vui lòng điền đầy đủ tất cả các trường!");
    }

    // 2. Kiểm tra mật khẩu mới và nhập lại có khớp nhau không (Yêu cầu của bạn)
    if (newPass !== confirmPass) {
        return alert("Mật khẩu mới và xác nhận mật khẩu không khớp nhau!");
    }

    // 3. Kiểm tra độ dài tối thiểu (tùy chọn)
    if (newPass.length < 5) {
        return alert("Mật khẩu mới phải có ít nhất 5 ký tự!");
    }

    // Nếu mọi thứ ở Frontend ổn, gửi xuống Server
    ws.send(JSON.stringify({
        action: "change_password",
        username: targetUser,
        oldPass: oldPass,
        newPass: newPass
    }));
};