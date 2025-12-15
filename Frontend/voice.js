let ws;
let username = "";
let currentCall = null;
let audioContext;
let mediaStream;
let sourceNode;
let processor;
let callStartTime = null;
let callTimerInterval = null;


let nextAudioTime = 0;

const urlParams = new URLSearchParams(window.location.search);
const me = urlParams.get('me');
const to = urlParams.get('to');
const action = urlParams.get('action');

username = me;
currentCall = to;


function connectToVoiceServer() {
    ws = new WebSocket("wss://10.10.49.115:3000");

    ws.onopen = () => {
        console.log("✅ Kết nối Voice Gateway thành công");

        if (!me || me === "null" || me.trim() === "") {
            console.warn("🚫 Không đăng ký voice vì username null!");
            ws.close();
            return;
        }
        
        ws.send(JSON.stringify({ action: "REGISTER_VOICE", username: me }));

        if (action === "call") {
            console.log(`Đang gọi ${to}...`);
           
            setTimeout(() => callUser(to), 500);
        } else if (action === "accept") {
            console.log(`Chấp nhận cuộc gọi từ ${to}`);
            
            document.getElementById('callStatus').innerHTML = `🎧 Đang trong cuộc gọi với ${to}`;
            ws.send(JSON.stringify({ 
                action: "ACCEPT_CALL", 
                from: to, 
                to: me    
            }));
            currentCall = to; 
            startAudio();
            startCallTimer(); 
        }
    };

    ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        switch (msg.action) {
            case "CALL_ACCEPTED":
                alert(`${msg.from} đã chấp nhận cuộc gọi!`);
                document.getElementById('callStatus').innerHTML = `🎧 Đang trong cuộc gọi với ${to}`;
                currentCall = msg.from; 
                startAudio();
                startCallTimer();
                break;
            case "CALL_REJECTED":
                alert(`${msg.from} từ chối cuộc gọi.`);
                window.close();
                break;
            case "CALL_ENDED":
                alert(`${msg.from} đã ngắt kết nối.`);
                stopCallTimer(); 
                window.close();
                break;
            case "AUDIO_DATA":
                if (msg.from !== me) {
                    if (Array.isArray(msg.data)) {
                        playIncomingAudio(msg.data);
                    } else {
                        console.warn("⚠️ Bỏ qua gói AUDIO_DATA không hợp lệ:", msg);
                    }
                } 
                break;
            default:
                console.log("📩 Popup msg:", msg);
        }
    };

    ws.onclose = () => {
        console.log("🔌 Mất kết nối Gateway");
        stopCallTimer();
        stopAudio(); 
        if (mediaStream) {
            mediaStream.getTracks().forEach(track => track.stop());
        }
        if (audioContext) {
            audioContext.close();
        }
    };
}



function callUser(target) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        console.error("WebSocket chưa sẵn sàng để gọi!");
        return;
    }
    ws.send(JSON.stringify({ action: "CALL", from: username, to: target }));
}


function stopAudio() {
    if (mediaStream) {
        mediaStream.getTracks().forEach(track => track.stop());
        mediaStream = null;
    }
    if (processor) {
        processor.disconnect();
        processor = null;
    }
    if (sourceNode) {
        sourceNode.disconnect();
        sourceNode = null;
    }
    
}

async function startAudio() {
    try {
        if (!audioContext) audioContext = new AudioContext({ sampleRate: 44100 });
        
        nextAudioTime = audioContext.currentTime;
        mediaStream = await navigator.mediaDevices.getUserMedia({ audio: true });
        sourceNode = audioContext.createMediaStreamSource(mediaStream);
        processor = audioContext.createScriptProcessor(2048, 1, 1);
    
        const gainNode = audioContext.createGain();
        
        gainNode.gain.setValueAtTime(0, audioContext.currentTime);

        
        sourceNode.connect(processor);
        
        processor.connect(gainNode);
        
        gainNode.connect(audioContext.destination);

        processor.onaudioprocess = (e) => {
            const input = e.inputBuffer.getChannelData(0);
            const buf = new Int16Array(input.length);
            for (let i = 0; i < input.length; i++) {
                let s = Math.max(-1, Math.min(1, input[i]));
                buf[i] = s < 0 ? s * 0x8000 : s * 0x7FFF; 
            }
            const audioData = Array.from(new Uint8Array(buf.buffer));

            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    action: "AUDIO_DATA",
                    from: username,
                    to: currentCall,
                    data: audioData
                }));
            }

        };

    } catch (err) {
        console.error("Lỗi khi bật audio:", err);
        alert("Lỗi khi bật audio: " + err.message);
    }
}


function playIncomingAudio(dataArray) {
    try {
        if (!audioContext) audioContext = new AudioContext({ sampleRate: 44100 });
        if (!Array.isArray(dataArray) || dataArray.length === 0) return;

        
        const u8 = new Uint8Array(dataArray);
        const int16 = new Int16Array(u8.buffer);

        
        const sampleRate = audioContext.sampleRate;
        
        const audioBuffer = audioContext.createBuffer(1, int16.length, sampleRate);
        const channel = audioBuffer.getChannelData(0);

        
        for (let i = 0; i < int16.length; i++) {
            channel[i] = int16[i] / 32768;
        }

        const src = audioContext.createBufferSource();
        src.buffer = audioBuffer;
        src.connect(audioContext.destination);

        let currentTime = audioContext.currentTime;
        if (nextAudioTime < currentTime) {
            nextAudioTime = currentTime;
        }
        src.start(nextAudioTime);
        nextAudioTime += audioBuffer.duration;

    } catch (err) {
        console.error("❌ Lỗi phát audio:", err);
    }
}

function formatTime(seconds) {
    const h = String(Math.floor(seconds / 3600)).padStart(2, '0');
    const m = String(Math.floor((seconds % 3600) / 60)).padStart(2, '0');
    const s = String(seconds % 60).padStart(2, '0');
    return `${h}:${m}:${s}`;
}

function startCallTimer() {
    if (callTimerInterval) return;

    callStartTime = Date.now();
    callTimerInterval = setInterval(() => {
        const elapsed =
            Math.floor((Date.now() - callStartTime) / 1000);

        const timer = document.getElementById("callTimer");
        if (timer) timer.innerText = formatTime(elapsed);
    }, 1000);
}

function stopCallTimer() {
    clearInterval(callTimerInterval);
    callTimerInterval = null;

    const timer = document.getElementById("callTimer");
    if (timer) timer.innerText = "00:00:00";
}


connectToVoiceServer();