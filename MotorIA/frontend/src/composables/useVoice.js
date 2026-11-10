import { ref } from "vue";

/* Recorder de voz con MediaRecorder. onAudio(blob) se invoca al detener. */
export function useVoice(onAudio) {
  const recording = ref(false);
  const seconds = ref(0);
  const error = ref("");

  let mediaRecorder = null;
  let audioChunks = [];
  let interval = null;

  function clearTimer() {
    if (interval) {
      clearInterval(interval);
      interval = null;
    }
  }

  async function start() {
    error.value = "";
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      mediaRecorder = new MediaRecorder(stream);
      audioChunks = [];

      mediaRecorder.ondataavailable = (e) => audioChunks.push(e.data);
      mediaRecorder.onstop = () => {
        stream.getTracks().forEach((t) => t.stop());
        const blob = new Blob(audioChunks, { type: "audio/webm" });
        if (onAudio) onAudio(blob);
      };

      mediaRecorder.start();
      recording.value = true;
      seconds.value = 0;
      interval = setInterval(() => {
        seconds.value++;
      }, 1000);
    } catch (e) {
      error.value = "No se pudo acceder al micrófono: " + e.message;
    }
  }

  function stop() {
    if (mediaRecorder && mediaRecorder.state === "recording") {
      mediaRecorder.stop();
    }
    recording.value = false;
    clearTimer();
  }

  function toggle() {
    if (recording.value) stop();
    else start();
  }

  return { recording, seconds, error, start, stop, toggle };
}
