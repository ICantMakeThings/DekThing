#include <Arduino.h>
#include <WiFi.h>
#include <ESP32WebServer.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <ArduinoJson.h>
#include <vector>

struct ScrollingText
{
  String text;
  int16_t x;     
  int16_t y;     
  int16_t width; 
  int16_t speed;  
  bool scrolling; 
  bool visible;  
};

ScrollingText titleScroll, artistScroll, albumScroll;

static const char *WIFI_SSID = "wifi";
static const char *WIFI_PASS = "bigbigpasswordforwifi";

ESP32WebServer server(80);
TFT_eSPI tft = TFT_eSPI();

std::vector<uint8_t> jpgBuffer;

const int BUTTON_NEXT = 12;
const int BUTTON_PREV = 27;
const int BUTTON_PLAYPAUSE = 14;

String pcip = "";

static const char *B64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
bool base64_decode_to_vec(const String &b64input, std::vector<uint8_t> &out)
{
  size_t len = b64input.length();
  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++)
    T[(unsigned char)B64chars[i]] = i;
  std::vector<uint8_t> result;
  result.reserve(len * 3 / 4);
  int val = 0, valb = -8;
  for (size_t i = 0; i < len; i++)
  {
    unsigned char c = b64input[i];
    if (T[c] == -1)
    {
      if (c == '=')
        break;
      else
        continue;
    }
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0)
    {
      result.push_back((uint8_t)((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  out.swap(result);
  return true;
}

bool tftRenderCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void setupScrollingText(ScrollingText &st, const String &txt, int16_t yPos, int16_t areaWidth)
{
  st.text = txt;
  st.y = yPos;
  st.width = tft.textWidth(txt);
  st.scrolling = st.width > areaWidth;
  st.visible = true;
  st.x = st.scrolling ? tft.width() : (tft.width() - st.width) / 2;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(st.x, st.y);
  tft.print(st.text);
}

void handle_update()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String body = server.arg("plain");
  if (body.isEmpty())
  {
    server.send(400, "text/plain", "Empty body");
    return;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, body);
  if (err)
  {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  const char *title = doc["title"] | "";
  const char *artists = doc["artists"] | doc["artist"] | "";
  const char *album = doc["album"] | "";
  const char *cover_b64 = doc["cover_base64"] | "";

  pcip = server.client().remoteIP().toString();
  Serial.println("Received from IP: " + pcip);

  tft.fillScreen(TFT_BLACK);

  int screenW = tft.width();
  int screenH = tft.height();

  int imgSize = 180;
  int imgX = (screenW - imgSize) / 2;
  int imgY = 0;
  if (strlen(cover_b64) > 10)
  {
    std::vector<uint8_t> jpgVec;
    base64_decode_to_vec(String(cover_b64), jpgVec);
    if (!jpgVec.empty())
    {
      jpgBuffer = jpgVec;
      TJpgDec.setJpgScale(1);
      TJpgDec.setCallback(tftRenderCallback);
      TJpgDec.drawJpg(imgX, imgY, jpgBuffer.data(), jpgBuffer.size());
    }
  }

  int textAreaX = 10;
  int textAreaWidth = screenW - 20;
  int textAreaY = screenH * 0.6;
  int textAreaH = screenH - textAreaY;

  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);

  int lineHeight = 18 + 5;
  int textBlockH = 3 * lineHeight;
  int startY = textAreaY + (textAreaH - textBlockH) / 2;

  tft.setFreeFont(&FreeSans12pt7b);
  
  setupScrollingText(titleScroll, String(title), startY, textAreaWidth);
  setupScrollingText(artistScroll, String(artists), startY + lineHeight, textAreaWidth);
  setupScrollingText(albumScroll, String(album), startY + 2 * lineHeight, textAreaWidth);

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void setupRoutes()
{
  server.on("/update", HTTP_POST, handle_update);
  server.onNotFound([]()
                    { server.send(404, "text/plain", "Not Found"); });
}

void setupButtons()
{
  pinMode(BUTTON_NEXT, INPUT_PULLUP);
  pinMode(BUTTON_PREV, INPUT_PULLUP);
  pinMode(BUTTON_PLAYPAUSE, INPUT_PULLUP);
}

void sendToSpicetify(const String &cmd)
{
  if (pcip == "")
  {
    Serial.println("Spicetify IP not set");
    return;
  }
  WiFiClient client;
  if (client.connect(pcip.c_str(), 80))
  {
    client.print("GET /" + cmd + " HTTP/1.1\r\nHost: " + pcip + "\r\n\r\n");
    Serial.println("Sent: " + cmd);
  }
  else
    Serial.println("Failed to connect to Spicetify");
}

void checkButtons()
{
  if (digitalRead(BUTTON_NEXT) == LOW)
  {
    sendToSpicetify("next");
    delay(500);
  }
  if (digitalRead(BUTTON_PREV) == LOW)
  {
    sendToSpicetify("prev");
    delay(500);
  }
  if (digitalRead(BUTTON_PLAYPAUSE) == LOW)
  {
    sendToSpicetify("playpause");
    delay(500);
  }
}

void updateScrollingText(ScrollingText &st) {
  if (!st.visible) return;

  if (st.scrolling) {
    tft.fillRect(st.x, st.y - 12, st.width, 18, TFT_BLACK);

    st.x -= st.speed;
    if (st.x + st.width < 0) st.x = tft.width();

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(st.x, st.y);
    tft.print(st.text);
  }
}


void setup()
{
  Serial.begin(9600);
  delay(100);
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(tft.width() / 2, 10);
  tft.println(WiFi.localIP().toString());
  delay(2000);

  TJpgDec.setCallback(tftRenderCallback);
  setupRoutes();
  setupButtons();
  server.begin();
  Serial.println("HTTP server ready");
}

void loop()
{
  server.handleClient();
  checkButtons();
  updateScrollingText(titleScroll);
  updateScrollingText(artistScroll);
  updateScrollingText(albumScroll);
  delay(20);
}
