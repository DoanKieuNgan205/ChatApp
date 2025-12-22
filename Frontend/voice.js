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
let historySaved = false;

const urlParams = new URLSearchParams(window.location.search);
const me = urlParams.get('me');
const to = urlParams.get('to');
const action = urlParams.get('action');

username = me;
currentCall = to;

let isOriginalCaller = (action === "call"); 
let originalCaller = isOriginalCaller ? me : to;
let originalReceiver = isOriginalCaller ? to : me;

function connectToVoiceServer() {
    console.log("[VOICE] 🔌 Connecting to voice server...");
    console.log(`   me: ${me}`);
    console.log(`   to: ${to}`);
    console.log(`   action: ${action}`);
    
    ws = new WebSocket("wss://10.246.147.186:3000");

    ws.onopen = () => {
        console.log("✅ Kết nối Voice Gateway thành công");

        if (!me || me === "null" || me.trim() === "") {
            console.warn("🚫 Không đăng ký voice vì username null!");
            ws.close();
            return;
        }

        console.log(`[VOICE] 📝 Registering voice for: ${me}`);
        
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
            console.log(`[VOICE] ✅ Accepted call, currentCall = ${currentCall}`);
            startAudio();
            startCallTimer(); 
        }
    };

    ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        console.log("[VOICE] Received:", msg.action);
        
        switch (msg.action) {
            case "CALL_ACCEPTED":
                console.log(`${msg.from} đã chấp nhận cuộc gọi!`);
                if (callTimeout) {
                    clearTimeout(callTimeout);
                    callTimeout = null;
                }
                document.getElementById('callStatus').innerHTML = `🎧 Đang trong cuộc gọi với ${to}`;
                currentCall = (msg.from === username) ? to : msg.from;
                console.log(`[VOICE] ✅ currentCall set to: ${currentCall}`);
                startAudio();
                startCallTimer();
                break;
                
            case "CALL_REJECTED":
                console.log(`[VOICE] ❌ ${msg.from} từ chối cuộc gọi.`);
                saveCallHistory("rejected");

                document.getElementById('callStatus').innerHTML = `<span style="color:red;">🔴 Cuộc gọi bị từ chối</span>`;
                
                setTimeout(() => {
                    alert(`${msg.from} từ chối cuộc gọi.`);
                    window.close();
                }, 1000);
                break;
                
            case "CALL_ENDED":
                console.log(`[VOICE] 📴 ${msg.from} đã kết thúc cuộc gọi`);
    
                if (callStartTime && !historySaved) {
                    saveCallHistory("completed");
                }
                
                alert(`${msg.from} đã ngắt kết nối.`);
                
                stopCallTimer();
                stopAudio();
                
                setTimeout(() => window.close(), 500);
                break;
                
            case "AUDIO_DATA":
                if (msg.from !== me) {
                    if (Array.isArray(msg.data)) {
                        playIncomingAudio(msg.data);
                    }
                } 
                break;
                
            case "CALL_HISTORY_SAVED":
                if (msg.status === "success") {
                    console.log("[VOICE] ✅ Lịch sử đã được lưu");
                } else {
                    console.error("[VOICE] ❌ Lưu lịch sử thất bại");
                }
                break;
                
            default:
                console.log("[VOICE] Unhandled action:", msg.action);
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

let callTimeout = null;

function callUser(target) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        console.error("WebSocket chưa sẵn sàng!");
        return;
    }
    
    ws.send(JSON.stringify({ action: "CALL", from: username, to: target }));
    
    callTimeout = setTimeout(() => {
        if (!callStartTime) {
            console.log("[VOICE] ⏰ Call timeout - no answer");
            alert("Không có phản hồi từ người nhận");
            window.close();
        }
    }, 30000);
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
        const elapsed = Math.floor((Date.now() - callStartTime) / 1000);
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

function saveCallHistory(status) {
    if (historySaved) {
        console.log("[VOICE] History already saved");
        return;
    }
    
    if (!currentCall || !callStartTime) {
        console.warn("[VOICE] Cannot save: no currentCall or callStartTime");
        return;
    }

    if (currentCall === username) {
        console.error(`[VOICE] ❌ ERROR: currentCall === username (${username})`);
        return;
    }

    const duration = callStartTime ? Math.floor((Date.now() - callStartTime) / 1000) : 0;

    if (duration <= 0 && status !== "rejected") {
        console.warn("[VOICE] Duration is 0 and not a rejection, not saving");
        return;
    }

    console.log(`[VOICE] 💾 Saving call history: from=${username}, to=${currentCall}, duration=${duration}s, status=${status}`);

    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({
            action: "SAVE_CALL_HISTORY",
            from: username,
            to: currentCall,
            duration: duration,
            status: status
        }));
        
        historySaved = true; 
        console.log("[VOICE] ✅ History save request sent");
    } else {
        console.error("[VOICE] ❌ Cannot save - WebSocket not open");
    }
}

window.addEventListener('DOMContentLoaded', function() {
    const hangupBtn = document.querySelector('button');
    
    if (hangupBtn) {
        hangupBtn.addEventListener('click', function(e) {
            e.preventDefault(); 
            
            console.log("[VOICE] 🔴 User clicked hangup");
            
            hangupBtn.disabled = true;
            hangupBtn.innerText = "Đang kết thúc...";
            
            if (ws && ws.readyState === WebSocket.OPEN && currentCall) {
                ws.send(JSON.stringify({
                    action: "END_CALL",
                    from: username,
                    to: currentCall
                }));
                console.log("[VOICE] ✅ Sent END_CALL notification");
            }
            
            if (callStartTime) {
                saveCallHistory("completed");
            } else {
                console.warn("[VOICE] ⚠️ Call not started yet, not saving history");
            }
            
            stopAudio();
            stopCallTimer();
            
            setTimeout(() => {
                console.log("[VOICE] 🔌 Closing window...");

                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({
                        action: "UNREGISTER_VOICE",
                        username: username
                    }));
                    
                    setTimeout(() => {
                        ws.close();
                        window.close();
                    }, 300);
                } else {
                    window.close();
                }
            }, 2000); 
        });
    }
});

window.addEventListener('beforeunload', function(e) {
    console.log("[VOICE] ⚠️ Window is closing");
    
    if (callStartTime && !historySaved && currentCall) {
        console.log("[VOICE] 💾 Saving history before close...");
        
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({
                action: "END_CALL",
                from: username,
                to: currentCall
            }));
            
            const blob = new Blob([JSON.stringify({
                action: "SAVE_CALL_HISTORY",
                from: username,
                to: currentCall,
                duration: Math.floor((Date.now() - callStartTime) / 1000),
                status: "completed"
            })], { type: 'application/json' });
            

            saveCallHistory("completed");
        }
    }
    
    stopAudio();
    stopCallTimer();
    
});

connectToVoiceServer();