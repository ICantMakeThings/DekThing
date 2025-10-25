import express from "express";
import cors from "cors";
import fetch from "node-fetch";

const app = express();
const PORT = 5000;
const ESP_IP = "192.168.1.235"; // Put the IP of your ESP32
const ESP_URL = `http://${ESP_IP}/update`;

app.use(cors());
app.use(express.json({ limit: "3mb" }));

app.post("/update", async (req, res) => {
  try {
    const payload = req.body;
    const espRes = await fetch(ESP_URL, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      timeout: 5000,
    });

    if (!espRes.ok) {
      console.error(`ESP responded with status ${espRes.status}`);
      return res.status(espRes.status).send("ESP error");
    }

    const text = await espRes.text();
    console.log(`[Proxy] Forwarded update -> ESP OK (${payload.title || "no title"})`);
    res.status(200).send(text);
  } catch (err) {
    console.error(`[Proxy] Failed to reach ESP: ${err.message}`);
    res.status(502).send("ESP unreachable");
  }
});

app.listen(PORT, () => {
  console.log(`ESP proxy running at http://localhost:${PORT}`);
});

