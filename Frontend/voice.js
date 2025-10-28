// voice.js - chạy trên trình duyệt

let ws;                  // WebSocket tới Node.js
let username = "";       // Tên user hiện tại
let currentCall = null;  // Người đang gọi
let audioContext;
let mediaStream;
let sourceNode;
let processor;
let audioQueue = [];     // Hàng đợi dữ liệu audio đến

// 🚀 LẤY THAM SỐ TỪ URL
const urlParams = new URLSearchParams(window.location.search);
const me = urlParams.get('me');
const to = urlParams.get('to');
const action = urlParams.get('action'); // 'call' hoặc 'accept'

username = me;
currentCall = to;

// === Kết nối tới Voice Gateway ===
function connectToVoiceServer(user) {
    //username = user;
    ws = new WebSocket("ws://localhost:3000");

    ws.onopen = () => {
        console.log("✅ Kết nối Voice Gateway thành công");
        ws.send(JSON.stringify({ action: "REGISTER", username: me }));

        // 🚀 TỰ ĐỘNG GỌI HOẶC CHẤP NHẬN
        if (action === "call") {
            console.log(`Đang gọi ${to}...`);
            callUser(to);
        } else if (action === "accept") {
            console.log(`Chấp nhận cuộc gọi từ ${to}`);
            // ❗ SỬA: Dùng "action"
            ws.send(JSON.stringify({ 
                action: "ACCEPT_CALL", 
                from: to, // người gọi
                to: me    // tôi
            }));
            startAudio();
        }
    };

    ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        switch (msg.action) {
            /*case "INCOMING_CALL":
                console.log(`📞 Cuộc gọi đến từ ${msg.from}`);
                if (confirm(`${msg.from} đang gọi bạn. Chấp nhận không?`)) {
                    ws.send(JSON.stringify({ type: "ACCEPT_CALL", from: msg.from, to: username }));
                    startAudio();
                } else {
                    ws.send(JSON.stringify({ type: "REJECT_CALL", from: msg.from, to: username }));
                }
                break;*/

            case "CALL_ACCEPTED":
                alert(`${msg.from} đã chấp nhận cuộc gọi!`);
                startAudio();
                break;

            case "CALL_REJECTED":
                alert(`${msg.from} từ chối cuộc gọi.`);
                window.close();
                break;

            case "CALL_ENDED": // 🚀 THÊM MỚI
                alert(`${msg.from} đã ngắt kết nối.`);
                window.close();
                break;

            case "AUDIO_DATA":
                if (msg.from !== me) { // Chỉ phát audio của người khác
                     playIncomingAudio(msg.data);
                }
                break;

            default:
                console.log("📩", msg);
        }
    };

    ws.onclose = () => console.log("🔌 Mất kết nối Gateway");
}

// === Gửi yêu cầu gọi ===
function callUser(target) {
    if (!ws) return;
    ws.send(JSON.stringify({ action: "CALL", from: username, to: target }));
}

// === Bắt đầu gửi âm thanh ===
async function startAudio() {
    try {
        if (!audioContext) {
            audioContext = new AudioContext({ sampleRate: 44100 });
        }
        mediaStream = await navigator.mediaDevices.getUserMedia({ audio: true });
        sourceNode = audioContext.createMediaStreamSource(mediaStream);
        processor = audioContext.createScriptProcessor(4096, 1, 1);

        sourceNode.connect(processor);
        processor.connect(audioContext.destination);

        processor.onaudioprocess = (e) => {
            const input = e.inputBuffer.getChannelData(0);
            
            // Chuyển đổi sang 16-bit PCM
            const buffer = new Int16Array(input.length);
            for (let i = 0; i < input.length; i++) {
                buffer[i] = Math.max(-1, Math.min(1, input[i])) * 0x7FFF;
            }
            
            // Chuyển đổi mảng Int16Array (2 bytes mỗi phần tử) thành mảng Uint8Array (1 byte mỗi phần tử)
            const audioData = Array.from(new Uint8Array(buffer.buffer));

            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    action: "AUDIO_DATA",
                    from: username,
                    to: currentCall,
                    data: audioData // data là một mảng các số (byte)
                }));
            }
        };
    } catch (err) {
        console.error("Lỗi khi bật audio:", err);
        alert("Không thể truy cập micro. Vui lòng cấp quyền.");
    }
}

// === Phát âm thanh nhận được ===
function playIncomingAudio(dataArray) {
    try {
        if (!audioContext) {
            audioContext = new AudioContext({ sampleRate: 44100 });
        }

        // Chuyển mảng byte (Uint8Array) về mảng 16-bit (Int16Array)
        const int16Buffer = new Int16Array(new Uint8Array(dataArray).buffer);
        
        const audioBuffer = audioContext.createBuffer(1, int16Buffer.length, audioContext.sampleRate);
        const channel = audioBuffer.getChannelData(0);
        
        // Chuyển đổi 16-bit PCM về float (-1.0 đến 1.0)
        for (let i = 0; i < int16Buffer.length; i++) {
            channel[i] = int16Buffer[i] / 0x7FFF;
        }

        const source = audioContext.createBufferSource();
        source.buffer = audioBuffer;
        source.connect(audioContext.destination);
        source.start();
    } catch (err)
    {
        console.error("Lỗi phát audio:", err);
    }
}

// 🚀 TỰ ĐỘNG KẾT NỐI KHI TRANG ĐƯỢC MỞ
connectToVoiceServer();
