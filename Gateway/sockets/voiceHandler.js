import { sendUdpVoice } from "../utils/cppClient.js";

export function handleVoiceData(data) {
  const { from, to, chunk } = data;
  const audioBuffer = Buffer.from(chunk, "base64");
  sendUdpVoice(from, to, audioBuffer);
}
