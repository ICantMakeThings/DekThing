(function () {
  const BASE_URL = "http://localhost:5000";
  const TAG = "[ESP-Bridge]";

  function log(...args) {
    console.log(TAG, ...args);
  }

  async function sendCommandToESP(cmd) {
    try {
      const xhr = new XMLHttpRequest();
      xhr.open("GET", `${BASE_URL}/${cmd}`, true);
      xhr.onload = () => {
        if (xhr.status >= 200 && xhr.status < 300) log(`Sent command: ${cmd}`);
        else log(`ESP returned error for command ${cmd}:`, xhr.status);
      };
      xhr.onerror = () => log(`Failed to send command: ${cmd}`);
      xhr.send();
    } catch (e) {
      log(`Failed to send command: ${cmd}`, e);
    }
  }

  async function resizeToBase64(imageUrl) {
    try {
      if (!imageUrl) return null;

      if (imageUrl.startsWith("spotify:image:")) {
        const hash = imageUrl.split(":").pop();
        imageUrl = `https://i.scdn.co/image/${hash}`;
      }

      const res = await fetch(imageUrl);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);

      const blob = await res.blob();
      const imgBitmap = await createImageBitmap(blob);

      const targetSize = 180;
      const canvas = new OffscreenCanvas(targetSize, targetSize);
      const ctx = canvas.getContext("2d");

      ctx.imageSmoothingEnabled = false;
      ctx.imageSmoothingQuality = "high";

      const minSide = Math.min(imgBitmap.width, imgBitmap.height);
      const sx = Math.floor((imgBitmap.width - minSide) / 2);
      const sy = Math.floor((imgBitmap.height - minSide) / 2);

      ctx.drawImage(imgBitmap, sx, sy, minSide, minSide, 0, 0, targetSize, targetSize);

      const outBlob = await canvas.convertToBlob({ type: "image/jpeg", quality: 0.9 });
      const arrayBuf = await outBlob.arrayBuffer();
      let binary = '';
      const bytes = new Uint8Array(arrayBuf);
      for (let i = 0; i < bytes.length; i++) binary += String.fromCharCode(bytes[i]);
      return btoa(binary);

    } catch (e) {
      console.log("[ESP-Bridge] Image fetch/resize failed:", e);
      return null;
    }
  }

  async function buildAndSendCurrent() {
    const data = Spicetify.Player.data;
    if (!data || !data.item) {
      log("No active track");
      return;
    }

    const item = data.item;
    const title = item.name || "";
    const artists = (item.artists || []).map(a => a.name).join(", ");
    const album = (item.album && item.album.name) || "";

    let imageUrl = null;
    if (item.album?.images?.length) {
      const largeImage = item.album.images
        .filter(img => img.width >= 300 && img.height >= 300)
        .sort((a, b) => a.width - b.width)[0] || item.album.images[0];

      if (largeImage?.url) imageUrl = largeImage.url;
    }

    let cover_base64 = await resizeToBase64(imageUrl);

    const payload = { title, artists, album, cover_base64 };
    console.log("[ESP-Bridge] Sending payload:", {
      title,
      artists,
      album,
      cover_base64_length: cover_base64?.length || 0
    });

    await sendToDevice(payload);
  }

  async function sendToDevice(payload) {
    try {
      const res = await fetch(`${BASE_URL}/update`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      if (!res.ok) log("ESP returned error", res.status);
      else log("Payload sent successfully");
    } catch (e) {
      log("Send failed:", e);
    }
  }

  function init() {
    log("ESP Bridge initializing...");

    Spicetify.Player.addEventListener("songchange", buildAndSendCurrent);
    Spicetify.Player.addEventListener("onplaypause", () => {
      const playing = Spicetify.Player.isPlaying();
      sendCommandToESP(playing ? "play" : "pause");
    });
    Spicetify.Player.addEventListener("onnext", () => sendCommandToESP("next"));
    Spicetify.Player.addEventListener("onprevious", () => sendCommandToESP("prev"));

    setTimeout(buildAndSendCurrent, 1000);
  }

  if (typeof Spicetify !== "undefined" && Spicetify?.Player) init();
  else document.addEventListener("spicetify:ready", init);
})();

