// M5 Pocket Clock - 12-hour clock with AM/PM and date for the M5Stack AtomS3.
// Wi-Fi is entered once on a phone (setup page); the time then syncs by itself (NTP) and the
// timezone is found automatically. Wi-Fi is switched off between syncs.
//  click        : next watch face (digital / ring / night)
//  hold 0.6 s   : next colour
//  hold 3 s     : Wi-Fi setup again
// Serial console (115200): ver | face N | theme N | settime H M S | sync | portal | stop | wifireset | shot
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

#define FW_VERSION "1.0.0"
#define AP_NAME "ClockSetup"

static const uint32_t SYNC_EVERY_MS = 6UL * 3600UL * 1000UL;  // resync every 6 hours
static const uint32_t RETRY_MS = 10UL * 60UL * 1000UL;        // retry after a failure
static const int NIGHT_FROM = 22, NIGHT_TO = 6;               // dim between these hours
static const uint8_t BRIGHT_DAY = 130, BRIGHT_NIGHT = 18;

static M5Canvas cv(&M5.Display);
static Preferences prefs;
static int W = 128, H = 128;

// ---- settings --------------------------------------------------------------
static uint8_t face = 0, theme = 0;
static float tzHours = 0;      // manual UTC offset (hours), used when auto timezone is off
static bool autoTz = true;
static int tzSec = 0;          // current total offset incl. DST, seconds
enum { FACES = 3 };
static const uint8_t THEMES[][3] = {{80, 200, 255}, {255, 205, 70}, {255, 120, 170},
                                    {120, 230, 130}, {255, 150, 80}, {180, 140, 255}};
static const int NUM_THEMES = sizeof(THEMES) / sizeof(THEMES[0]);

// ---- network state (written by the net task, read by the UI) ----------------
enum NetState : uint8_t { NET_IDLE, NET_CONNECTING, NET_SYNCING, NET_PORTAL, NET_FAILED };
static volatile NetState netState = NET_IDLE;
static volatile bool syncWanted = true, portalWanted = false, portalStop = false, wifiResetWanted = false;
static volatile bool everSynced = false, ntpDone = false, saveFlag = false;
static volatile uint32_t lastSyncMs = 0, lastTryMs = 0;
static volatile bool portalHasCreds = false;

static uint16_t C_BG, C_DIM, C_TEXT, C_ACC, C_GOOD, C_WARN, C_BAD;
static uint32_t lastFrame = 0, brightUntil = 0, toastUntil = 0, lastLog = 0;
static uint8_t curBright = 0;
static bool shotRequested = false;
static char toast[24] = "";
static float pax = 0, pay = 0, paz = 1;

static inline uint16_t rgb(int r, int g, int b) { return cv.color565(r, g, b); }

static String tzString(int sec) {
  char b[24];
  int a = abs(sec);
  snprintf(b, sizeof b, "UTC%c%02d:%02d", sec >= 0 ? '-' : '+', a / 3600, (a % 3600) / 60);  // POSIX sign is inverted
  return String(b);
}

static void applyTz() {
  setenv("TZ", tzString(tzSec).c_str(), 1);
  tzset();
}

static bool timeValid() { return time(nullptr) > 1700000000L; }

static void saveCfg() {
  prefs.putUChar("face", face);
  prefs.putUChar("theme", theme);
  prefs.putFloat("tzh", tzHours);
  prefs.putBool("auto", autoTz);
  prefs.putInt("tzsec", tzSec);
}

static void say(const char* t) {
  strlcpy(toast, t, sizeof(toast));
  toastUntil = millis() + 1400;
}

// ---- networking -------------------------------------------------------------
static void onNtp(struct timeval*) { ntpDone = true; }

// Asks free "where am I" services for the UTC offset (including daylight saving).
static bool tzFromUrl(const char* url, const char* key, const char* dstKey) {
  HTTPClient http;
  http.setTimeout(5000);
  bool ok = false;
  if (http.begin(url)) {
    if (http.GET() == 200) {
      JsonDocument d;
      if (!deserializeJson(d, http.getStream()) && !d[key].isNull()) {
        tzSec = (int)(d[key] | 0) + (dstKey ? (int)(d[dstKey] | 0) : 0);
        prefs.putInt("tzsec", tzSec);
        ok = true;
      }
    }
    http.end();
  }
  return ok;
}

static void fetchTimezone() {
  if (!autoTz) { tzSec = (int)(tzHours * 3600.0f); return; }
  if (tzFromUrl("http://ip-api.com/json/?fields=status,offset", "offset", nullptr)) return;
  tzFromUrl("http://worldtimeapi.org/api/ip", "raw_offset", "dst_offset");  // fallback; keeps the last value if both fail
}

static bool ntpSync() {
  ntpDone = false;
  sntp_set_time_sync_notification_cb(onNtp);
  configTzTime(tzString(tzSec).c_str(), "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  uint32_t t0 = millis();
  while (!ntpDone && millis() - t0 < 12000) delay(50);
  return ntpDone;
}

static void doSync() {
  netState = NET_CONNECTING;
  WiFi.mode(WIFI_STA);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin();  // stored credentials
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    lastTryMs = millis();
    netState = NET_FAILED;
    if (WiFi.SSID().length() == 0) portalWanted = true;  // never configured
    WiFi.mode(WIFI_OFF);
    return;
  }
  netState = NET_SYNCING;
  fetchTimezone();
  applyTz();
  if (ntpSync()) {
    applyTz();
    everSynced = true;
    lastSyncMs = millis();
    netState = NET_IDLE;
  } else {
    lastTryMs = millis();
    netState = NET_FAILED;
  }
  WiFi.disconnect(true, false);  // keep credentials, switch the radio off
  WiFi.mode(WIFI_OFF);
}

static void runPortal() {
  static WiFiManager wm;
  static WiFiManagerParameter pTz("tz", "UTC offset in hours (e.g. 5.5). Used only if auto timezone is 0", "0", 8);
  static WiFiManagerParameter pAuto("auto", "Auto timezone from internet: 1 = yes, 0 = no", "1", 2);
  static bool added = false;
  if (!added) {
    wm.addParameter(&pTz);
    wm.addParameter(&pAuto);
    wm.setDebugOutput(false);
    wm.setConfigPortalBlocking(false);
    wm.setSaveConfigCallback([]() { saveFlag = true; });
    added = true;
  }
  if (wifiResetWanted) { wm.resetSettings(); wifiResetWanted = false; }
  saveFlag = false;
  portalStop = false;
  netState = NET_PORTAL;
  WiFi.mode(WIFI_AP_STA);
  wm.startConfigPortal(AP_NAME);
  while (!portalStop && !saveFlag) {
    wm.process();
    delay(10);
  }
  if (saveFlag) {  // user entered Wi-Fi + options
    tzHours = atof(pTz.getValue());
    autoTz = atoi(pAuto.getValue()) != 0;
    if (!autoTz) tzSec = (int)(tzHours * 3600.0f);
    saveCfg();
    wm.stopConfigPortal();
    syncWanted = true;  // already connected: sync right away
  } else {
    wm.stopConfigPortal();
    WiFi.mode(WIFI_OFF);
    netState = NET_IDLE;
  }
  portalWanted = false;
}

static void netTask(void*) {
  for (;;) {
    if (portalWanted) runPortal();
    else if (syncWanted) { syncWanted = false; doSync(); }
    else {
      uint32_t now = millis();
      if (netState == NET_FAILED && now - lastTryMs > RETRY_MS) syncWanted = true;
      else if (everSynced && now - lastSyncMs > SYNC_EVERY_MS) syncWanted = true;
    }
    delay(100);
  }
}

// ---- drawing helpers ---------------------------------------------------------
static const lgfx::IFont* BOLD[] = {&fonts::FreeSansBold24pt7b, &fonts::FreeSansBold18pt7b, &fonts::FreeSansBold12pt7b};

static void txt(const char* s, int x, int y, const lgfx::IFont* f, uint16_t col,
                textdatum_t d = textdatum_t::middle_center) {
  cv.setFont(f);
  if (cv.textWidth(s) > W - 4) cv.setFont(&fonts::Font0);  // too wide: use the small font
  if (d == textdatum_t::middle_center) {  // centre by hand (bitmap fonts centre wrongly)
    x -= cv.textWidth(s) / 2;
    d = textdatum_t::middle_left;
  }
  cv.setTextDatum(d);
  cv.setTextColor(col);
  cv.drawString(s, x, y);
}

static const lgfx::IFont* fit(const char* s, int maxW) {
  for (auto f : BOLD) {
    cv.setFont(f);
    if (cv.textWidth(s) <= maxW) return f;
  }
  return BOLD[2];
}

// hh:mm with a blinking colon, centred on (cx, cy)
static void bigTime(int cx, int cy, int maxW, int hh, int mm, bool colon, uint16_t col, uint16_t colonCol) {
  char sh[4], sm[4], full[8];
  snprintf(sh, sizeof sh, "%d", hh);
  snprintf(sm, sizeof sm, "%02d", mm);
  snprintf(full, sizeof full, "%s:%s", sh, sm);
  const lgfx::IFont* f = fit("00:00", maxW);  // same size for every time, so it never jumps
  cv.setFont(f);
  int wh = cv.textWidth(sh), wc = cv.textWidth(":"), wm = cv.textWidth(sm);
  int x = cx - (wh + wc + wm) / 2;
  txt(sh, x, cy, f, col, textdatum_t::middle_left);
  if (colon) txt(":", x + wh, cy - 2, f, colonCol, textdatum_t::middle_left);
  txt(sm, x + wh + wc, cy, f, col, textdatum_t::middle_left);
}

static void tzLabel(char* b, size_t n) {
  int a = abs(tzSec);
  if (a % 3600) snprintf(b, n, "UTC%c%d:%02d", tzSec >= 0 ? '+' : '-', a / 3600, (a % 3600) / 60);
  else snprintf(b, n, "UTC%c%d", tzSec >= 0 ? '+' : '-', a / 3600);
}

static void statusRow(uint32_t now) {
  uint16_t dot;
  const char* label;
  char b[16];
  bool busy = netState == NET_CONNECTING || netState == NET_SYNCING;
  if (busy) { dot = ((now / 400) % 2) ? C_WARN : C_DIM; label = "syncing"; }
  else if (!everSynced) { dot = C_BAD; label = "no sync"; }
  else if (now - lastSyncMs > 12UL * 3600UL * 1000UL) { dot = C_WARN; label = "old sync"; }
  else { dot = C_GOOD; tzLabel(b, sizeof b); label = b; }
  cv.fillCircle(W / 2 - 38, H - 9, 3, dot);
  txt(label, W / 2 - 30, H - 9, &fonts::Font2, C_DIM, textdatum_t::middle_left);
}

// ---- watch faces ---------------------------------------------------------------
static void faceDigital(const tm& t, uint32_t now) {
  int h12 = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
  char b[24];
  strftime(b, sizeof b, "%a %d %b", &t);
  for (char* p = b; *p; ++p) *p = toupper(*p);
  txt(b, W / 2, 15, &fonts::FreeSans9pt7b, C_DIM);
  bigTime(W / 2, 58, W - 6, h12, t.tm_min, t.tm_sec % 2 == 0, C_TEXT, C_ACC);
  txt(t.tm_hour < 12 ? "AM" : "PM", W / 2 - 22, 94, &fonts::FreeSansBold18pt7b, C_ACC);
  snprintf(b, sizeof b, ":%02d", t.tm_sec);
  txt(b, W / 2 + 30, 96, &fonts::FreeSans12pt7b, C_DIM);
}

static void faceRing(const tm& t, uint32_t now) {
  int h12 = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
  cv.fillArc(W / 2, H / 2, 54, 61, 0, 360, rgb(28, 34, 52));
  float frac = (t.tm_sec + (now % 1000) / 1000.0f) / 60.0f;
  if (frac > 0.003f) cv.fillArc(W / 2, H / 2, 54, 61, 270, 270 + 360 * frac, C_ACC);
  char b[24];
  bigTime(W / 2, 50, 96, h12, t.tm_min, t.tm_sec % 2 == 0, C_TEXT, C_ACC);
  txt(t.tm_hour < 12 ? "AM" : "PM", W / 2, 79, &fonts::FreeSansBold12pt7b, C_ACC);
  strftime(b, sizeof b, "%a %d", &t);
  for (char* p = b; *p; ++p) *p = toupper(*p);
  txt(b, W / 2, 100, &fonts::FreeSans9pt7b, C_DIM);
}

static void faceNight(const tm& t, uint32_t now) {
  int h12 = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
  const uint8_t* c = THEMES[theme];
  uint16_t dim = rgb(c[0] * 55 / 100, c[1] * 55 / 100, c[2] * 55 / 100);
  bigTime(W / 2, 58, W - 6, h12, t.tm_min, true, dim, dim);
  txt(t.tm_hour < 12 ? "AM" : "PM", W / 2, 92, &fonts::FreeSansBold12pt7b, dim);
}

static void screenSetup(uint32_t now) {
  txt("WI-FI SETUP", W / 2, 14, &fonts::FreeSansBold9pt7b, C_ACC);
  txt("1. Join Wi-Fi", 8, 38, &fonts::Font2, C_TEXT, textdatum_t::middle_left);
  txt(AP_NAME, W / 2, 59, &fonts::FreeSansBold9pt7b, C_WARN);
  txt("2. Pick home", 8, 82, &fonts::Font2, C_TEXT, textdatum_t::middle_left);
  txt("   Wi-Fi + Save", 8, 98, &fonts::Font2, C_TEXT, textdatum_t::middle_left);
  txt("the page opens itself", W / 2, 119, &fonts::Font0, C_DIM);
}

static void screenConnecting(uint32_t now, const char* msg) {
  for (int i = 0; i < 8; i++) {
    float a = now * 0.006f - i * 0.4f;
    cv.fillCircle(W / 2 + (int)(cosf(a) * 26), 44 + (int)(sinf(a) * 26), 6 - i * 3 / 5, C_ACC);
  }
  txt("POCKET CLOCK", W / 2, 90, &fonts::Font2, C_TEXT);
  txt(msg, W / 2, 110, &fonts::Font2, C_DIM);
}

static void draw(uint32_t now) {
  const uint8_t* c = THEMES[theme];
  C_ACC = rgb(c[0], c[1], c[2]);
  cv.fillSprite(C_BG);

  time_t tt = time(nullptr);
  tm t;
  localtime_r(&tt, &t);

  if (netState == NET_PORTAL) {
    screenSetup(now);
  } else if (!timeValid()) {
    const char* m = netState == NET_FAILED ? "no Wi-Fi" : (netState == NET_SYNCING ? "syncing..." : "connecting...");
    screenConnecting(now, m);
  } else {
    if (face == 1) faceRing(t, now);
    else if (face == 2) faceNight(t, now);
    else faceDigital(t, now);
    if (face == 0) statusRow(now);
  }

  if (M5.BtnA.isPressed()) {
    if (M5.BtnA.pressedFor(3000)) txt("release: Wi-Fi setup", W / 2, 118, &fonts::Font0, C_WARN);
    else if (M5.BtnA.pressedFor(600)) txt("release: new colour", W / 2, 118, &fonts::Font0, C_GOOD);
  }
  if (now < toastUntil) {
    cv.fillRoundRect(14, 50, 100, 26, 6, rgb(20, 24, 40));
    cv.drawRoundRect(14, 50, 100, 26, 6, C_GOOD);
    txt(toast, W / 2, 63, &fonts::Font2, C_TEXT);
  }
  cv.pushSprite(0, 0);
}

// ---- brightness / motion -----------------------------------------------------
static void updateBrightness(uint32_t now) {
  uint8_t target = BRIGHT_DAY;
  if (timeValid()) {
    time_t tt = time(nullptr);
    tm t;
    localtime_r(&tt, &t);
    bool night = NIGHT_FROM > NIGHT_TO ? (t.tm_hour >= NIGHT_FROM || t.tm_hour < NIGHT_TO)
                                       : (t.tm_hour >= NIGHT_FROM && t.tm_hour < NIGHT_TO);
    if (night && now > brightUntil) target = BRIGHT_NIGHT;
  }
  if (target != curBright) {
    curBright = target;
    M5.Display.setBrightness(curBright);
  }
}

static void readMotion(uint32_t now) {
  float ax, ay, az;
  if (!M5.Imu.update()) return;
  M5.Imu.getAccel(&ax, &ay, &az);
  float j = fabsf(ax - pax) + fabsf(ay - pay) + fabsf(az - paz);
  pax = ax; pay = ay; paz = az;
  if (j > 0.35f) brightUntil = now + 8000;  // pick it up / bump it: full brightness for 8 s
}

// ---- console ------------------------------------------------------------------
static void runCommand(char* cmd) {
  int a, b, c;
  if (!strcmp(cmd, "ver")) Serial.printf("Pocket Clock %s\n", FW_VERSION);
  else if (sscanf(cmd, "face %d", &a) == 1) { face = ((a % FACES) + FACES) % FACES; saveCfg(); }
  else if (sscanf(cmd, "theme %d", &a) == 1) { theme = ((a % NUM_THEMES) + NUM_THEMES) % NUM_THEMES; saveCfg(); }
  else if (sscanf(cmd, "settime %d %d %d", &a, &b, &c) == 3) {
    time_t now = time(nullptr);
    tm t;
    localtime_r(&now, &t);
    t.tm_year = 126; t.tm_mon = 10; t.tm_mday = 18;  // 2026-11-18 (a Wednesday), for screenshots
    t.tm_hour = a; t.tm_min = b; t.tm_sec = c;
    timeval tv = {mktime(&t), 0};
    settimeofday(&tv, nullptr);
  } else if (!strcmp(cmd, "sync")) syncWanted = true;
  else if (!strcmp(cmd, "portal")) portalWanted = true;
  else if (!strcmp(cmd, "stop")) portalStop = true;
  else if (!strcmp(cmd, "wifireset")) { wifiResetWanted = true; portalWanted = true; }
  else if (!strcmp(cmd, "shot")) shotRequested = true;
  else Serial.println("commands: ver | face 0-2 | theme 0-5 | settime H M S | sync | portal | stop | wifireset | shot");
}

static void pollSerial() {
  static char buf[40];
  static int n = 0;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      buf[n] = 0;
      if (n) runCommand(buf);
      n = 0;
    } else if (n < (int)sizeof(buf) - 1) {
      buf[n++] = ch;
    }
  }
}

static void sendShot() {
  Serial.printf("SHOT %d %d\n", W, H);
  const uint8_t* p = (const uint8_t*)cv.getBuffer();
  size_t left = (size_t)W * H * 2;
  uint32_t t0 = millis();
  while (left && millis() - t0 < 4000) {  // the USB serial buffer can accept less than asked: retry
    size_t n = Serial.write(p, left > 256 ? 256 : left);
    if (n == 0) { delay(1); continue; }
    p += n;
    left -= n;
  }
  Serial.println();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(0);
  W = M5.Display.width();
  H = M5.Display.height();
  cv.setColorDepth(16);
  cv.createSprite(W, H);
  C_BG = rgb(6, 8, 16);
  C_DIM = rgb(120, 132, 160);
  C_TEXT = rgb(240, 244, 255);
  C_GOOD = rgb(60, 220, 120);
  C_WARN = rgb(255, 190, 60);
  C_BAD = rgb(255, 80, 80);
  prefs.begin("clock", false);
  face = prefs.getUChar("face", 0) % FACES;
  theme = prefs.getUChar("theme", 0) % NUM_THEMES;
  tzHours = prefs.getFloat("tzh", 0);
  autoTz = prefs.getBool("auto", true);
  tzSec = prefs.getInt("tzsec", (int)(tzHours * 3600));
  applyTz();
  curBright = BRIGHT_DAY;
  M5.Display.setBrightness(curBright);
  Serial.printf("Pocket Clock %s, screen %dx%d\n", FW_VERSION, W, H);
  xTaskCreate(netTask, "net", 10240, nullptr, 1, nullptr);
}

void loop() {
  M5.update();
  uint32_t now = millis();

  if (M5.BtnA.wasReleaseFor(3000)) {
    wifiResetWanted = false;
    portalWanted = true;
    say("WI-FI SETUP");
  } else if (M5.BtnA.wasReleaseFor(600)) {
    theme = (theme + 1) % NUM_THEMES;
    saveCfg();
  } else if (M5.BtnA.wasClicked()) {
    if (netState == NET_PORTAL) portalStop = true;
    else { face = (face + 1) % FACES; saveCfg(); }
  }

  pollSerial();
  if (now - lastFrame < 33) return;
  lastFrame = now;
  readMotion(now);
  updateBrightness(now);
  draw(now);
  if (shotRequested) {
    shotRequested = false;
    sendShot();
  }
  if (now - lastLog > 5000) {
    lastLog = now;
    Serial.printf("net=%d synced=%d tz=%d heap=%u\n", (int)netState, (int)everSynced, tzSec, ESP.getFreeHeap());
  }
}
