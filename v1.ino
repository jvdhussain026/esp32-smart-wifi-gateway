/*
  Javed_FreeWiFi - ULTIMATE EDITION v1.0
  Enhanced Captive Portal with Advanced Ad System & Task Management
  - Comprehensive task-based access system
  - Advanced admin features with real-time monitoring
  - Robust error handling and recovery
  - System performance monitoring
  - Auto-recovery mechanisms
  - Data usage tracking
  - Multiple access packages


 IMPORTANT:
  - This sketch does NOT perform NAT/IP forwarding for general internet traffic.
    To actually allow clients full internet access after grant, use a router/openwrt/pfSense
    to add firewall rules for the granted IP OR use ESP-IDF NAT forwarding example (advanced).
*/

// this code is not fully working the internet is still not avilable to we are working on it it will fixed soon 
// it just demondtrste how the ui will look like 


#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include <Update.h>

// Note: Avoid IDF-specific netif/tcpip headers for Arduino portability

// ---------- CONFIG ----------
const char* AP_SSID = "Free WiFi Gateway ";  //  simplified AP
const char* AP_PASSWORD = "";    // open network (leave empty for open).
const byte DNS_PORT = 53;

const char* ADMIN_USER = "admin";   // its your admin panel username 
const char* ADMIN_PASS = "admin123"; // its your admin panel password 

// Uplink (hardcoded) - main network credentials to which ESP32 will try to connect as STA

const char* UPLINK_SSID_DEFAULT = "home_wifi";  // change this to your real home wifi ssid
const char* UPLINK_PASS_DEFAULT = "12345678";  // change this to your real home wifi password

// Task durations in milliseconds
const unsigned long TASK_YOUTUBE_DURATION = 30 * 60 * 1000UL; // 30 minutes
const unsigned long TASK_INSTAGRAM_DURATION = 30 * 60 * 1000UL; // 30 minutes
const unsigned long TASK_AD_30_SEC_DURATION = 45 * 60 * 1000UL; // 45 minutes
const unsigned long TASK_AD_60_SEC_DURATION = 2 * 60 * 60 * 1000UL; // 2 hours
const unsigned long TASK_SIMPLE_DURATION = 15 * 60 * 1000UL; // 15 minutes (new)

// Whitelist duration (2 hours) - fallback
const unsigned long GRANT_DURATION_MS = 2UL * 60UL * 60UL * 1000UL;

// File paths
const char* SESSIONS_FILE = "/sessions.json";
const char* SETTINGS_FILE = "/settings.json";
const char* STATS_FILE = "/stats.json";

// System settings
#define MAX_SESSIONS 50
#define SERIAL_BAUD_RATE 115200
#define DNS_CACHE_SIZE 100
#define WIFI_CONNECT_TIMEOUT 10000
#define AP_START_TIMEOUT 5000
#define MAINTENANCE_INTERVAL 60000

// ---------- GLOBALS ----------
WebServer server(80);
DNSServer dnsServer;

struct Session {
  String ip;
  String mac;
  String hostname;
  unsigned long grantTime;
  unsigned long expireAt;
  bool active;
  unsigned long dataUsed; // Track data usage
  String activePackage; // Track which package is active
  String deviceType; // Track device type
};

struct SystemStats {
  unsigned long startTime;
  unsigned long totalGrants;
  unsigned long totalKicks;
  unsigned long adminLogins;
  unsigned long portalViews;
  unsigned long errors;
  unsigned long adsShown;
  unsigned long youtubeTasks;
  unsigned long instagramTasks;
  unsigned long simpleTasks;
  unsigned long totalDataUsed;
};

// Global arrays
Session sessions[MAX_SESSIONS];
SystemStats stats;

// Runtime settings
String uplinkSSID = UPLINK_SSID_DEFAULT;
String uplinkPASS = UPLINK_PASS_DEFAULT;
String apSSID = AP_SSID;
String apPassword = AP_PASSWORD;

// System state
bool isAPRunning = false;
bool isSTAConnected = false;
bool isSPIFFSMounted = false;
unsigned long systemUptime = 0;

// ---------- NEW TASK MANAGEMENT ----------

struct Task {
  String id;
  String name;
  String description;
  String type; // "youtube", "instagram", "ad", "simple"
  int duration; // seconds to complete
  unsigned long rewardDuration; // milliseconds of access
  String redirectUrl;
  bool requiresInternet;
  String icon;
};

Task availableTasks[] = {
  {"simple", "Quick Access", "Get 15 minutes of free access", "simple", 5, TASK_SIMPLE_DURATION, "", false, "⚡"},
  {"ad_30s", "Watch Quick Ad", "30 second sponsor video", "ad", 30, TASK_AD_30_SEC_DURATION, "", false, "👀"},
  {"yt_engage", "YouTube Engagement", "Watch 3 videos & subscribe to channel", "youtube", 30, TASK_YOUTUBE_DURATION, "https://www.youtube.com", true, "📹"},
  {"ig_engage", "Instagram Boost", "Like 3 reels & follow channel", "instagram", 30, TASK_INSTAGRAM_DURATION, "https://www.instagram.com", true, "📸"},
  {"ad_60s", "Premium Ad Watch", "60 second premium ad for 2 hours access", "ad", 60, TASK_AD_60_SEC_DURATION, "", false, "🎬"}
};

const int TASK_COUNT = 5;

// ---------- UTILITY FUNCTIONS ----------

void logInfo(String message) {
  Serial.println("✅ " + message);
}

void logWarning(String message) {
  Serial.println("⚠️ " + message);
}

void logError(String message) {
  Serial.println("❌ " + message);
  stats.errors++;
  saveStatsToSPIFFS();
}

void logStep(String message) {
  Serial.println("📝 " + message);
}

void logSuccess(String message) {
  Serial.println("🎉 " + message);
}

void logDebug(String message) {
  Serial.println("🐛 " + message);
}

String getSystemUptime() {
  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  
  return String(days) + "d " + String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
}

String getMemoryInfo() {
  return "Heap: " + String(esp_get_free_heap_size()) + " bytes, Min Free: " + 
         String(esp_get_minimum_free_heap_size()) + " bytes";
}

String ipToString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

String getClientMAC() { return WiFi.macAddress(); }

String formatDataUsage(unsigned long bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < 1024 * 1024) return String(bytes / 1024.0, 1) + " KB";
  else return String(bytes / (1024.0 * 1024.0), 1) + " MB";
}

String formatDuration(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds %= 60;
  minutes %= 60;
  
  if (hours > 0) {
    return String(hours) + "h " + String(minutes) + "m";
  } else if (minutes > 0) {
    return String(minutes) + "m " + String(seconds) + "s";
  } else {
    return String(seconds) + "s";
  }
}

String getDeviceType(String userAgent) {
  userAgent.toLowerCase();
  if (userAgent.indexOf("android") != -1) return "Android";
  if (userAgent.indexOf("iphone") != -1 || userAgent.indexOf("ipad") != -1) return "iOS";
  if (userAgent.indexOf("windows") != -1) return "Windows";
  if (userAgent.indexOf("mac") != -1) return "Mac";
  if (userAgent.indexOf("linux") != -1) return "Linux";
  return "Unknown";
}

// ---------- SESSION MANAGEMENT ----------

void initializeSessions() {
  logStep("Initializing sessions array...");
  for(int i = 0; i < MAX_SESSIONS; i++) {
    sessions[i].ip = "";
    sessions[i].mac = "";
    sessions[i].hostname = "Unknown";
    sessions[i].grantTime = 0;
    sessions[i].expireAt = 0;
    sessions[i].active = false;
    sessions[i].dataUsed = 0;
    sessions[i].activePackage = "None";
    sessions[i].deviceType = "Unknown";
  }
  logSuccess("Sessions array initialized");
}

int findSessionByIP(const String &ip) {
  for(int i = 0; i < MAX_SESSIONS; i++) {
    if(sessions[i].ip == ip && sessions[i].active) return i;
  }
  return -1;
}

int findEmptySessionSlot() {
  for(int i = 0; i < MAX_SESSIONS; i++) {
    if(!sessions[i].active) return i;
  }
  return -1;
}

int getActiveSessionCount() {
  int count = 0;
  for(int i = 0; i < MAX_SESSIONS; i++) {
    if(sessions[i].active) count++;
  }
  return count;
}

void saveSessionsToSPIFFS() {
  logStep("Saving sessions to SPIFFS...");
  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.createNestedArray("sessions");
  
  for(int i = 0; i < MAX_SESSIONS; i++) {
    if(sessions[i].active) {
      JsonObject o = arr.createNestedObject();
      o["ip"] = sessions[i].ip;
      o["mac"] = sessions[i].mac;
      o["hostname"] = sessions[i].hostname;
      o["grantTime"] = sessions[i].grantTime;
      o["expireAt"] = sessions[i].expireAt;
      o["active"] = sessions[i].active;
      o["dataUsed"] = sessions[i].dataUsed;
      o["activePackage"] = sessions[i].activePackage;
      o["deviceType"] = sessions[i].deviceType;
    }
  }
  
  File f = SPIFFS.open(SESSIONS_FILE, FILE_WRITE);
  if(f) {
    serializeJson(doc, f);
    f.close();
    logSuccess("Sessions saved successfully");
  } else {
    logError("Failed to save sessions to SPIFFS");
  }
}

void loadSessionsFromSPIFFS() {
  logStep("Loading sessions from SPIFFS...");
  if(!SPIFFS.exists(SESSIONS_FILE)) {
    logWarning("No sessions file found, starting fresh");
    return;
  }
  
  File f = SPIFFS.open(SESSIONS_FILE, FILE_READ);
  if(!f) {
    logError("Failed to open sessions file");
    return;
  }
  
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  
  if(err) {
    logError("Failed to parse sessions JSON: " + String(err.c_str()));
    return;
  }
  
  JsonArray arr = doc["sessions"].as<JsonArray>();
  int loaded = 0;
  for(JsonVariant v : arr) {
    int idx = findEmptySessionSlot();
    if(idx >= 0) {
      sessions[idx].ip = String((const char*)v["ip"]);
      sessions[idx].mac = String((const char*)v["mac"]);
      sessions[idx].hostname = String((const char*)v["hostname"]);
      sessions[idx].grantTime = (unsigned long)(v["grantTime"].as<unsigned long>());
      sessions[idx].expireAt = (unsigned long)(v["expireAt"].as<unsigned long>());
      sessions[idx].active = v["active"].as<bool>();
      if(v.containsKey("dataUsed")) sessions[idx].dataUsed = v["dataUsed"].as<unsigned long>();
      if(v.containsKey("activePackage")) sessions[idx].activePackage = String((const char*)v["activePackage"]);
      if(v.containsKey("deviceType")) sessions[idx].deviceType = String((const char*)v["deviceType"]);
      loaded++;
    }
  }
  logSuccess("Loaded " + String(loaded) + " sessions from storage");
}

// ---------- SETTINGS MANAGEMENT ----------

void saveSettingsToSPIFFS() {
  logStep("Saving settings to SPIFFS...");
  DynamicJsonDocument doc(2048);
  doc["uplink_ssid"] = uplinkSSID;
  doc["uplink_pass"] = uplinkPASS;
  doc["ap_ssid"] = apSSID;
  doc["ap_password"] = apPassword;
  
  File f = SPIFFS.open(SETTINGS_FILE, FILE_WRITE);
  if(f) {
    serializeJson(doc, f);
    f.close();
    logSuccess("Settings saved successfully");
  } else {
    logError("Failed to save settings to SPIFFS");
  }
}

void loadSettingsFromSPIFFS() {
  logStep("Loading settings from SPIFFS...");
  if(!SPIFFS.exists(SETTINGS_FILE)) {
    logWarning("No settings file found, using defaults");
    return;
  }
  
  File f = SPIFFS.open(SETTINGS_FILE, FILE_READ);
  if(!f) {
    logError("Failed to open settings file");
    return;
  }
  
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  
  if(err) {
    logError("Failed to parse settings JSON: " + String(err.c_str()));
    return;
  }
  
  if(doc.containsKey("uplink_ssid")) uplinkSSID = String((const char*)doc["uplink_ssid"]);
  if(doc.containsKey("uplink_pass")) uplinkPASS = String((const char*)doc["uplink_pass"]);
  if(doc.containsKey("ap_ssid")) apSSID = String((const char*)doc["ap_ssid"]);
  if(doc.containsKey("ap_password")) apPassword = String((const char*)doc["ap_password"]);
  
  logSuccess("Settings loaded: Uplink=" + uplinkSSID + ", AP=" + apSSID);
}

// ---------- STATISTICS MANAGEMENT ----------

void initializeStats() {
  logStep("Initializing system statistics...");
  stats.startTime = millis();
  stats.totalGrants = 0;
  stats.totalKicks = 0;
  stats.adminLogins = 0;
  stats.portalViews = 0;
  stats.errors = 0;
  stats.adsShown = 0;
  stats.youtubeTasks = 0;
  stats.instagramTasks = 0;
  stats.simpleTasks = 0;
  stats.totalDataUsed = 0;
  logSuccess("Statistics system initialized");
}

void saveStatsToSPIFFS() {
  DynamicJsonDocument doc(1024);
  doc["startTime"] = stats.startTime;
  doc["totalGrants"] = stats.totalGrants;
  doc["totalKicks"] = stats.totalKicks;
  doc["adminLogins"] = stats.adminLogins;
  doc["portalViews"] = stats.portalViews;
  doc["errors"] = stats.errors;
  doc["adsShown"] = stats.adsShown;
  doc["youtubeTasks"] = stats.youtubeTasks;
  doc["instagramTasks"] = stats.instagramTasks;
  doc["simpleTasks"] = stats.simpleTasks;
  doc["totalDataUsed"] = stats.totalDataUsed;
  
  File f = SPIFFS.open(STATS_FILE, FILE_WRITE);
  if(f) {
    serializeJson(doc, f);
    f.close();
  }
}

void loadStatsFromSPIFFS() {
  logStep("Loading statistics from SPIFFS...");
  if(!SPIFFS.exists(STATS_FILE)) {
    logWarning("No stats file found, starting fresh");
    return;
  }
  
  File f = SPIFFS.open(STATS_FILE, FILE_READ);
  if(!f) {
    logError("Failed to open stats file");
    return;
  }
  
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  
  if(err) {
    logError("Failed to parse stats JSON: " + String(err.c_str()));
    return;
  }
  
  stats.startTime = doc["startTime"].as<unsigned long>();
  stats.totalGrants = doc["totalGrants"].as<unsigned long>();
  stats.totalKicks = doc["totalKicks"].as<unsigned long>();
  stats.adminLogins = doc["adminLogins"].as<unsigned long>();
  stats.portalViews = doc["portalViews"].as<unsigned long>();
  stats.errors = doc["errors"].as<unsigned long>();
  stats.adsShown = doc["adsShown"].as<unsigned long>();
  stats.youtubeTasks = doc["youtubeTasks"].as<unsigned long>();
  stats.instagramTasks = doc["instagramTasks"].as<unsigned long>();
  stats.simpleTasks = doc["simpleTasks"].as<unsigned long>();
  stats.totalDataUsed = doc["totalDataUsed"].as<unsigned long>();
  
  logSuccess("Statistics loaded: " + String(stats.totalGrants) + " grants, " + String(stats.errors) + " errors");
}

// ---------- IMPROVED TASK GRANT SYSTEM ----------

void grantAccessWithDuration(const String &ip, unsigned long durationMs, const String &packageName) {
  logStep("Granting " + String(durationMs/60000) + " minutes access to IP: " + ip);
  
  int idx = findSessionByIP(ip);
  if(idx < 0) idx = findEmptySessionSlot();
  
  if(idx < 0) {
    logError("No available session slots for IP: " + ip);
    return;
  }
  
  sessions[idx].ip = ip;
  // Store the ESP's MAC as placeholder (Arduino core does not expose per-station MAC by IP)
  sessions[idx].mac = getClientMAC();
  sessions[idx].hostname = "Client-" + String(random(1000, 9999));
  sessions[idx].grantTime = millis();
  sessions[idx].expireAt = millis() + durationMs;
  sessions[idx].active = true;
  sessions[idx].activePackage = packageName;
  
  // Detect device type from User-Agent
  String userAgent = server.header("User-Agent");
  sessions[idx].deviceType = getDeviceType(userAgent);
  
  stats.totalGrants++;
  saveSessionsToSPIFFS();
  saveStatsToSPIFFS();
  
  logSuccess("Access granted to IP: " + ip + " for " + String(durationMs/60000) + " minutes (" + sessions[idx].deviceType + ")");
}

void grantIP(const String &ip) {
  grantAccessWithDuration(ip, GRANT_DURATION_MS, "Legacy Grant");
}

// ---------- SESSION OPERATIONS ----------

void purgeExpiredSessions() {
  unsigned long now = millis();
  bool changed = false;
  
  for(int i = 0; i < MAX_SESSIONS; i++) {
    if(sessions[i].active) {
      if((long)(sessions[i].expireAt - now) <= 0) {
        // Add data usage to total before expiring
        stats.totalDataUsed += sessions[i].dataUsed;
        sessions[i].active = false;
        changed = true;
        logInfo("Session expired for IP: " + sessions[i].ip + " (Data used: " + formatDataUsage(sessions[i].dataUsed) + ")");
      }
    }
  }
  
  if(changed) {
    saveSessionsToSPIFFS();
    saveStatsToSPIFFS();
    logInfo("Purged expired sessions");
  }
}

bool isWhitelisted(const String &ip) {
  int idx = findSessionByIP(ip);
  if(idx < 0) return false;
  
  unsigned long now = millis();
  bool valid = (long)(sessions[idx].expireAt - now) > 0;
  
  if(!valid) {
    stats.totalDataUsed += sessions[idx].dataUsed;
    sessions[idx].active = false;
    saveSessionsToSPIFFS();
    saveStatsToSPIFFS();
  }
  
  return valid;
}

void kickIP(const String &ip) {
  int idx = findSessionByIP(ip);
  if(idx >= 0) {
    stats.totalDataUsed += sessions[idx].dataUsed;
    sessions[idx].active = false;
    saveSessionsToSPIFFS();
    stats.totalKicks++;
    saveStatsToSPIFFS();
    logInfo("Kicked IP: " + ip);
  }
}

void kickAllSessions() {
  logStep("Kicking all active sessions...");
  for(int i = 0; i < MAX_SESSIONS; i++) {
    if(sessions[i].active) {
      stats.totalDataUsed += sessions[i].dataUsed;
      sessions[i].active = false;
      stats.totalKicks++;
    }
  }
  saveSessionsToSPIFFS();
  saveStatsToSPIFFS();
  logSuccess("All sessions kicked");
}

String getClientIPString() {
  IPAddress clientIP = server.client().remoteIP();
  return ipToString(clientIP);
}

// ---------- WIFI MANAGEMENT ----------

bool startAccessPoint() {
  logStep("Starting Access Point...");
  logInfo("AP SSID: " + apSSID);
  logInfo("AP Password: " + String(apPassword.length() > 0 ? "***" : "Open Network"));
  
  unsigned long startTime = millis();
  bool apStarted;
  
  if(apPassword.length() == 0) {
    apStarted = WiFi.softAP(apSSID.c_str());
  } else {
    apStarted = WiFi.softAP(apSSID.c_str(), apPassword.c_str());
  }
  
  if(!apStarted) {
    logError("Failed to start Access Point!");
    isAPRunning = false;
    return false;
  }
  
  // Wait for AP to fully start
  while(millis() - startTime < AP_START_TIMEOUT) {
    if(WiFi.softAPIP().toString() != "0.0.0.0") {
      break;
    }
    delay(100);
  }
  
  IPAddress apIP = WiFi.softAPIP();
  if(apIP.toString() == "0.0.0.0") {
    logError("AP failed to get IP address!");
    isAPRunning = false;
    return false;
  }
  
  isAPRunning = true;
  logSuccess("Access Point started successfully!");
  logInfo("AP IP: " + apIP.toString());
  logInfo("AP MAC: " + WiFi.softAPmacAddress());
  logInfo("AP Stations: " + String(WiFi.softAPgetStationNum()));
  
  return true;
}

bool connectToUplink() {
  logStep("Connecting to uplink WiFi...");
  logInfo("Uplink SSID: " + uplinkSSID);
  logInfo("Uplink Password: " + String(uplinkPASS.length() > 0 ? "***" : "No Password"));
  
  if(uplinkSSID.length() == 0) {
    logWarning("No uplink SSID configured, skipping STA connection");
    isSTAConnected = false;
    return false;
  }
  
  WiFi.begin(uplinkSSID.c_str(), uplinkPASS.c_str());
  
  unsigned long startTime = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    isSTAConnected = true;
    logSuccess("Connected to uplink successfully!");
    logInfo("STA IP: " + WiFi.localIP().toString());
    logInfo("STA MAC: " + WiFi.macAddress());
    logInfo("Signal Strength: " + String(WiFi.RSSI()) + " dBm");
    return true;
  } else {
    isSTAConnected = false;
    logWarning("Failed to connect to uplink WiFi");
    logInfo("Status: " + String(WiFi.status()));
    return false;
  }
}

void printNetworkInfo() {
  logStep("=== NETWORK INFORMATION ===");
  logInfo("AP SSID: " + apSSID);
  logInfo("AP IP: " + WiFi.softAPIP().toString());
  logInfo("AP Stations: " + String(WiFi.softAPgetStationNum()));
  logInfo("AP Status: " + String(isAPRunning ? "Running" : "Stopped"));
  
  if(isSTAConnected) {
    logInfo("STA SSID: " + uplinkSSID);
    logInfo("STA IP: " + WiFi.localIP().toString());
    logInfo("STA Gateway: " + WiFi.gatewayIP().toString());
    logInfo("STA RSSI: " + String(WiFi.RSSI()) + " dBm");
  } else {
    logInfo("STA Status: Disconnected");
  }
  logStep("============================");
}

// ---------- ENHANCED WEB PAGES ----------

const char* WELCOME_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Welcome to Free WiFi</title>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      text-align: center;
      padding: 40px 20px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      min-height: 100vh;
      margin: 0;
    }
    .container {
      max-width: 500px;
      margin: 0 auto;
      background: rgba(255,255,255,0.1);
      padding: 40px;
      border-radius: 20px;
      backdrop-filter: blur(10px);
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    .btn {
      display: inline-block;
      padding: 15px 30px;
      background: #00ff88;
      color: #333;
      border-radius: 50px;
      text-decoration: none;
      font-weight: bold;
      margin: 10px;
      transition: all 0.3s ease;
      box-shadow: 0 4px 15px rgba(0,255,136,0.3);
    }
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 6px 20px rgba(0,255,136,0.4);
    }
    .logo {
      font-size: 2.5em;
      margin-bottom: 20px;
    }
    .feature {
      background: rgba(255,255,255,0.1);
      padding: 15px;
      margin: 10px 0;
      border-radius: 10px;
      text-align: left;
    }
    .footer {
      margin-top: 30px;
      padding: 20px;
      background: rgba(0,0,0,0.2);
      border-radius: 10px;
      font-size: 0.9em;
    }
    .whatsapp {
      color: #25D366;
      font-weight: bold;
    }
    .stats {
      display: flex;
      justify-content: space-around;
      margin: 20px 0;
    }
    .stat-item {
      text-align: center;
    }
    .stat-number {
      font-size: 1.5em;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="logo">🚀</div>
    <h1>Welcome to Javed's Secure Network</h1>
    <p>Experience up to 1 Gbps high-speed internet access!</p>
    
    <div class="stats">
      <div class="stat-item">
        <div class="stat-number" id="activeUsers">--</div>
        <div>Active Users</div>
      </div>
      <div class="stat-item">
        <div class="stat-number" id="uptime">--</div>
        <div>Uptime</div>
      </div>
    </div>
    
    <div class="feature">
      <strong>✨ Premium Features:</strong>
      <ul>
        <li>High-speed 5G connectivity</li>
        <li>Enhanced security protocols</li>
        <li>Uninterrupted streaming</li>
        <li>24/7 availability</li>
      </ul>
    </div>
    
    <p>Complete a quick task to get <strong>free internet access!</strong></p>
    
    <a class="btn" href="/tasks">Get Free Access</a>
    <a class="btn" href="/userstatus" style="background: #ff6b6b;">My Status</a>
    <a class="btn" href="/status" style="background: #17a2b8;">Network Status</a>
    
    <div class="footer">
      <p><strong>Nothing is free bro! 😊</strong></p>
      <p>We provide this service through ads and sponsorships. This revenue covers our WiFi costs, servers, electricity, and maintenance.</p>
      <p>Made with ❤️ by Javed</p>
      <p>Want your ad or channel featured here? <span class="whatsapp">Contact us on WhatsApp</span> 📱</p>
      <p>Like our work? <a href="#" style="color: #ffd700;">Buy me a coffee ☕</a></p>
    </div>
  </div>

  <script>
    // Fetch real-time stats
    fetch('/api/stats')
      .then(r => r.json())
      .then(data => {
        document.getElementById('activeUsers').textContent = data.activeSessions;
        document.getElementById('uptime').textContent = data.uptime;
      });
  </script>
</body>
</html>
)rawliteral";

String makeTasksPage() {
  stats.portalViews++;
  saveStatsToSPIFFS();
  
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Earn Free WiFi</title>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      padding: 20px;
      background: linear-gradient(135deg, #ff6b6b 0%, #feca57 100%);
      color: #333;
      margin: 0;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: white;
      padding: 30px;
      border-radius: 20px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
    }
    .task-card {
      background: #f8f9fa;
      padding: 20px;
      margin: 15px 0;
      border-radius: 15px;
      text-align: left;
      border-left: 5px solid #007bff;
      transition: all 0.3s ease;
      cursor: pointer;
    }
    .task-card:hover {
      transform: translateY(-3px);
      box-shadow: 0 5px 15px rgba(0,0,0,0.1);
    }
    .task-reward {
      background: #28a745;
      color: white;
      padding: 5px 10px;
      border-radius: 20px;
      font-size: 0.8em;
      float: right;
    }
    .task-icon {
      font-size: 1.5em;
      margin-right: 10px;
    }
    .btn {
      display: inline-block;
      padding: 12px 25px;
      background: #007bff;
      color: white;
      border-radius: 50px;
      text-decoration: none;
      font-weight: bold;
      margin: 10px 5px;
      border: none;
      cursor: pointer;
      transition: all 0.3s ease;
    }
    .btn:hover {
      transform: translateY(-2px);
    }
    .loading {
      display: none;
      text-align: center;
      margin: 20px 0;
    }
    .spinner {
      border: 5px solid #f3f3f3;
      border-top: 5px solid #007bff;
      border-radius: 50%;
      width: 50px;
      height: 50px;
      animation: spin 1s linear infinite;
      margin: 0 auto;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    .footer {
      margin-top: 30px;
      padding: 15px;
      background: #f8f9fa;
      border-radius: 10px;
      font-size: 0.9em;
      text-align: center;
    }
    .popular-badge {
      background: #ffc107;
      color: #000;
      padding: 2px 8px;
      border-radius: 10px;
      font-size: 0.7em;
      margin-left: 5px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🎯 Earn Free WiFi Access</h1>
    <p>Choose a task below to get free internet access!</p>
    
    <div class="task-card" onclick="startTask('simple')">
      <div class="task-reward">15 MIN</div>
      <h3><span class="task-icon">⚡</span> Quick Access</h3>
      <p>Get 15 minutes of instant access - no questions asked!</p>
    </div>
    
    <div class="task-card" onclick="startTask('ad_30s')">
      <div class="task-reward">45 MIN</div>
      <h3><span class="task-icon">👀</span> Watch Quick Ad</h3>
      <p>30 second sponsor video for 45 minutes access</p>
    </div>
    
    <div class="task-card" onclick="startTask('yt_engage')">
      <div class="task-reward">30 MIN</div>
      <h3><span class="task-icon">📹</span> YouTube Engagement</h3>
      <p>Watch 3 videos & subscribe to our channel</p>
    </div>
    
    <div class="task-card" onclick="startTask('ig_engage')">
      <div class="task-reward">30 MIN</div>
      <h3><span class="task-icon">📸</span> Instagram Boost</h3>
      <p>Like 3 reels & follow our channel</p>
    </div>
    
    <div class="task-card" onclick="startTask('ad_60s')">
      <div class="task-reward">2 HOURS <span class="popular-badge">POPULAR</span></div>
      <h3><span class="task-icon">🎬</span> Premium Ad Watch</h3>
      <p>60 second premium ad for 2 hours full access</p>
    </div>

    <div class="loading" id="loading">
      <div class="spinner"></div>
      <p id="loading-text">Preparing your task...</p>
    </div>

    <div style="text-align: center; margin-top: 20px;">
      <a class="btn" href="/" style="background: #6c757d;">Back to Home</a>
      <a class="btn" href="/userstatus" style="background: #17a2b8;">My Status</a>
    </div>

    <div class="footer">
      <p><strong>Support our service!</strong> Ads help us cover WiFi costs and maintenance.</p>
      <p>Made with ❤️ by Javed | Contact for advertising: WhatsApp 📱</p>
    </div>
  </div>

  <script>
    function startTask(taskId) {
      const loading = document.getElementById('loading');
      const loadingText = document.getElementById('loading-text');
      const taskCards = document.querySelectorAll('.task-card');
      
      // Disable all task cards
      taskCards.forEach(card => {
        card.style.opacity = '0.6';
        card.style.pointerEvents = 'none';
      });
      
      loading.style.display = 'block';
      loadingText.textContent = 'Starting task...';
      
      // Simulate task preparation
      setTimeout(() => {
        window.location.href = '/dotask?task=' + taskId;
      }, 1500);
    }
  </script>
</body>
</html>
)rawliteral";
  return page;
}

String makeDoTaskPage(String taskId) {
  Task task;
  for(int i = 0; i < TASK_COUNT; i++) {
    if(availableTasks[i].id == taskId) {
      task = availableTasks[i];
      break;
    }
  }
  
  String page = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>Completing Task</title><style>body{font-family:'Segoe UI',sans-serif; text-align:center; padding:20px; background:#f0f0f0;}";
  page += ".container{max-width:500px; margin:0 auto; background:white; padding:30px; border-radius:15px; box-shadow:0 5px 15px rgba(0,0,0,0.1);}";
  page += ".ad-container{background:#000; color:white; padding:30px; border-radius:10px; margin:20px 0;}";
  page += ".timer{font-size:2em; font-weight:bold; color:#007bff; margin:20px 0;}";
  page += ".progress-bar{width:100%; height:10px; background:#e9ecef; border-radius:5px; overflow:hidden; margin:20px 0;}";
  page += ".progress{height:100%; background:#007bff; width:0%; transition:width 0.1s linear;}";
  page += ".spinner{border:5px solid #f3f3f3; border-top:5px solid #007bff; border-radius:50%; width:60px; height:60px; animation:spin 1s linear infinite; margin:20px auto;}";
  page += "@keyframes spin{0%{transform:rotate(0deg);}100%{transform:rotate(360deg);}}</style></head><body>";
  page += "<div class='container'>";
  
  if(task.type == "ad" || task.type == "simple") {
    page += "<h1>" + task.icon + " " + task.name + "</h1>";
    page += "<div class='ad-container'>";
    page += "<h3>Premium Advertisement</h3>";
    page += "<p>Please watch this ad to continue</p>";
    page += "<div class='timer' id='timer'>" + String(task.duration) + "s</div>";
    page += "<div class='progress-bar'><div class='progress' id='progress'></div></div>";
    page += "<p>You'll get " + String(task.rewardDuration/60000) + " minutes of access</p>";
    page += "</div>";
    page += "<div class='spinner'></div>";
    page += "<p>Verifying completion...</p>";
    
    page += "<script>";
    page += "let timeLeft = " + String(task.duration) + ";";
    page += "const totalTime = " + String(task.duration) + ";";
    page += "const timer = setInterval(function() {";
    page += "timeLeft--;";
    page += "document.getElementById('timer').textContent = timeLeft + 's';";
    page += "document.getElementById('progress').style.width = ((totalTime - timeLeft) / totalTime * 100) + '%';";
    page += "if(timeLeft <= 0) {";
    page += "clearInterval(timer);";
    page += "fetch('/granttask?task=" + taskId + "', {method: 'POST'})";
    page += ".then(() => {";
    page += "document.body.innerHTML = `";
    page += "<div class='container'><h1 style='color:#28a745;'>✅ Task Complete!</h1>";
    page += "<p>You've earned " + String(task.rewardDuration/60000) + " minutes of WiFi access!</p>";
    page += "<div class='spinner'></div>";
    page += "<p>Starting your browsing session...</p>";
    page += "<p>This page will close in <span id='countdown'>10</span> seconds</p></div>`;";
    page += "let count = 10;";
    page += "const countdown = setInterval(() => {";
    page += "count--; document.getElementById('countdown').textContent = count;";
    page += "if(count <= 0) { clearInterval(countdown); window.close(); }";
    page += "}, 1000);";
    page += "});";
    page += "}";
    page += "}, 1000);";
    page += "</script>";
    
  } else {
    page += "<h1>🔗 Redirecting to " + task.name + "</h1>";
    page += "<div class='spinner'></div>";
    page += "<p>We're redirecting you to complete the task...</p>";
    page += "<p>After completing the task, return to this page to verify.</p>";
    
    page += "<script>";
    page += "setTimeout(() => {";
    page += "window.open('" + task.redirectUrl + "', '_blank');";
    page += "setTimeout(() => {";
    page += "document.body.innerHTML = `";
    page += "<div class='container'><h1>✅ Task Verification</h1>";
    page += "<p>Please return to this tab after completing the task on " + task.name + "</p>";
    page += "<button onclick='verifyTask()' style='padding:15px 30px; background:#007bff; color:white; border:none; border-radius:10px; font-size:16px; cursor:pointer;'>I Completed the Task</button>";
    page += "</div>`;";
    page += "}, 3000);";
    page += "}, 2000);";
    page += "function verifyTask() {";
    page += "document.body.innerHTML = `<div class='container'><div class='spinner'></div><p>Verifying your task completion...</p></div>`;";
    page += "setTimeout(() => {";
    page += "fetch('/granttask?task=" + taskId + "', {method: 'POST'})";
    page += ".then(() => {";
    page += "document.body.innerHTML = `";
    page += "<div class='container'><h1 style='color:#28a745;'>✅ Verification Complete!</h1>";
    page += "<p>You've earned " + String(task.rewardDuration/60000) + " minutes of WiFi access!</p>";
    page += "<a href='/' style='display:inline-block; padding:15px 30px; background:#28a745; color:white; text-decoration:none; border-radius:10px; margin:10px;'>Start Browsing</a>";
    page += "<p>This page will auto-close in <span id='countdown'>10</span> seconds</p></div>`;";
    page += "let count = 10;";
    page += "const countdown = setInterval(() => {";
    page += "count--; document.getElementById('countdown').textContent = count;";
    page += "if(count <= 0) { clearInterval(countdown); window.close(); }";
    page += "}, 1000);";
    page += "});";
    page += "}, 3000);";
    page += "}";
    page += "</script>";
  }
  
  page += "</div></body></html>";
  return page;
}

String makeUserStatusPage() {
  String clientIP = getClientIPString();
  int sessionIdx = findSessionByIP(clientIP);
  
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>My WiFi Status</title>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      padding: 20px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      margin: 0;
    }
    .container {
      max-width: 500px;
      margin: 0 auto;
    }
    .status-card {
      background: rgba(255,255,255,0.1);
      padding: 25px;
      margin: 15px 0;
      border-radius: 15px;
      backdrop-filter: blur(10px);
    }
    .status-item {
      display: flex;
      justify-content: space-between;
      margin: 10px 0;
      padding: 10px;
      background: rgba(255,255,255,0.1);
      border-radius: 8px;
    }
    .warning {
      background: rgba(255,193,7,0.2);
      border-left: 4px solid #ffc107;
    }
    .success {
      background: rgba(40,167,69,0.2);
      border-left: 4px solid #28a745;
    }
    .btn {
      display: block;
      width: 100%;
      padding: 15px;
      background: #00ff88;
      color: #333;
      border: none;
      border-radius: 10px;
      font-weight: bold;
      margin: 10px 0;
      cursor: pointer;
      text-align: center;
      text-decoration: none;
    }
    .signal-bar {
      display: flex;
      gap: 3px;
      align-items: end;
    }
    .bar {
      width: 8px;
      background: #00ff88;
      border-radius: 2px;
    }
    .bar1 { height: 10px; }
    .bar2 { height: 15px; }
    .bar3 { height: 20px; }
    .bar4 { height: 25px; }
    .device-icon {
      font-size: 1.2em;
      margin-right: 5px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>📊 My Connection Status</h1>
    )rawliteral";
    
  if(sessionIdx >= 0 && sessions[sessionIdx].active) {
    unsigned long now = millis();
    long timeLeft = (sessions[sessionIdx].expireAt - now) / 1000; // seconds
    long minutesLeft = timeLeft / 60;
    long hoursLeft = minutesLeft / 60;
    
    String timeLeftStr;
    if (hoursLeft > 0) {
      timeLeftStr = String(hoursLeft) + "h " + String(minutesLeft % 60) + "m";
    } else {
      timeLeftStr = String(minutesLeft) + "m " + String(timeLeft % 60) + "s";
    }
    
    page += "<div class='status-card'>";
    page += "<h2>✅ Connected</h2>";
    
    page += "<div class='status-item'><span>Active Package:</span><span>" + sessions[sessionIdx].activePackage + "</span></div>";
    page += "<div class='status-item'><span>Time Remaining:</span><span>" + timeLeftStr + "</span></div>";
    page += "<div class='status-item'><span>Data Used:</span><span>" + formatDataUsage(sessions[sessionIdx].dataUsed) + "</span></div>";
    page += "<div class='status-item'><span>Device Type:</span><span><span class='device-icon'>";
    
    // Add device-specific icon
    if (sessions[sessionIdx].deviceType == "Android") page += "📱";
    else if (sessions[sessionIdx].deviceType == "iOS") page += "📱";
    else if (sessions[sessionIdx].deviceType == "Windows") page += "💻";
    else if (sessions[sessionIdx].deviceType == "Mac") page += "💻";
    else if (sessions[sessionIdx].deviceType == "Linux") page += "💻";
    else page += "🔧";
    
    page += "</span>" + sessions[sessionIdx].deviceType + "</span></div>";
    page += "<div class='status-item'><span>Signal Strength:</span><span><div class='signal-bar'><div class='bar bar1'></div><div class='bar bar2'></div><div class='bar bar3'></div><div class='bar bar4'></div></div></span></div>";
    page += "<div class='status-item'><span>Connection Time:</span><span>" + String((now - sessions[sessionIdx].grantTime) / 60000) + " minutes</span></div>";
    
    if(minutesLeft < 10) {
      page += "<div class='status-item warning'><span>⚠️ Low Time!</span><span>Your access is running out</span></div>";
    }
    
    page += "</div>";
    
    page += "<a href='/tasks' class='btn'>🎬 Claim More Time with Ads</a>";
    
  } else {
    page += "<div class='status-card'>";
    page += "<h2>❌ No Active Session</h2>";
    page += "<p>You don't have active WiFi access. Complete a task to get connected!</p>";
    page += "<a href='/tasks' class='btn'>Get Free Access</a>";
    page += "</div>";
  }
  
  page += R"rawliteral(
    <div class="status-card">
      <h3>ℹ️ About This Service</h3>
      <p>This free WiFi is supported by advertisements. Your participation helps cover our operational costs including:</p>
      <ul>
        <li>📡 Internet & Server Costs</li>
        <li>⚡ Electricity</li>
        <li>🔧 Maintenance</li>
        <li>🛠️ Equipment Upgrades</li>
      </ul>
      <p><strong>Made with ❤️ by Javed</strong></p>
      <p>Want to advertise here? Contact us on WhatsApp!</p>
    </div>
    
    <a href="/" class="btn" style="background: #6c757d;">← Back to Home</a>
  </div>
</body>
</html>
)rawliteral";
  
  return page;
}

String makeStatusPage() {
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Network Status</title>
    <style>
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        padding: 20px;
        background: #f8f9fa;
        color: #333;
      }
      .container {
        max-width: 800px;
        margin: 0 auto;
      }
      .card {
        background: white;
        padding: 20px;
        margin: 10px 0;
        border-radius: 10px;
        box-shadow: 0 2px 10px rgba(0,0,0,0.1);
      }
      .status-online { color: #28a745; font-weight: bold; }
      .status-offline { color: #dc3545; font-weight: bold; }
      .info-grid {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 10px;
      }
      .info-item {
        padding: 10px;
        background: #f8f9fa;
        border-radius: 5px;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>📊 Network Status Dashboard</h1>
      
      <div class="card">
        <h2>System Information</h2>
        <div class="info-grid">
          <div class="info-item">Uptime: <span class="status-online">)rawliteral" + getSystemUptime() + R"rawliteral(</span></div>
          <div class="info-item">Active Sessions: <span class="status-online">)rawliteral" + String(getActiveSessionCount()) + R"rawliteral(</span></div>
          <div class="info-item">AP Status: <span class="status-online">)rawliteral" + String(isAPRunning ? "Running" : "Stopped") + R"rawliteral(</span></div>
          <div class="info-item">STA Status: <span class="status-online">)rawliteral" + String(isSTAConnected ? "Connected" : "Disconnected") + R"rawliteral(</span></div>
          <div class="info-item">AP IP: <span class="status-online">)rawliteral" + WiFi.softAPIP().toString() + R"rawliteral(</span></div>
          <div class="info-item">Connected Clients: <span class="status-online">)rawliteral" + String(WiFi.softAPgetStationNum()) + R"rawliteral(</span></div>
        )rawliteral" + 
        (isSTAConnected ? 
          "<div class=\"info-item\">STA IP: <span class=\"status-online\">" + WiFi.localIP().toString() + "</span></div>" +
          "<div class=\"info-item\">Signal Strength: <span class=\"status-online\">" + String(WiFi.RSSI()) + " dBm</span></div>" 
        : "") +
        R"rawliteral(
        </div>
      </div>
      
      <div class="card">
        <h2>Active Sessions</h2>
        )rawliteral";
        
  int activeCount = getActiveSessionCount();
  if(activeCount > 0) {
    page += "<table style='width: 100%; border-collapse: collapse;'>";
    page += "<tr style='background: #007bff; color: white;'><th>IP Address</th><th>Device</th><th>Package</th><th>Time Left</th><th>Data Used</th></tr>";
    
    unsigned long now = millis();
    for(int i = 0; i < MAX_SESSIONS; i++) {
      if(sessions[i].active) {
        long timeLeft = (sessions[i].expireAt - now) / 1000 / 60; // minutes
        page += "<tr style='border-bottom: 1px solid #ddd;'>";
        page += "<td style='padding: 10px;'>" + sessions[i].ip + "</td>";
        page += "<td style='padding: 10px;'>" + sessions[i].deviceType + "</td>";
        page += "<td style='padding: 10px;'>" + sessions[i].activePackage + "</td>";
        page += "<td style='padding: 10px;'>" + String(timeLeft) + " minutes</td>";
        page += "<td style='padding: 10px;'>" + formatDataUsage(sessions[i].dataUsed) + "</td>";
        page += "</tr>";
      }
    }
    page += "</table>";
  } else {
    page += "<p>No active sessions</p>";
  }
  
  page += R"rawliteral(
      </div>
      
      <div style="text-align: center; margin-top: 20px;">
        <a href="/" style="display: inline-block; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 5px;">Back to Home</a>
      </div>
    </div>
  </body>
  </html>
  )rawliteral";
  
  return page;
}

String makeAdminLoginPage() {
  return R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Admin Login</title>
    <style>
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        display: flex;
        justify-content: center;
        align-items: center;
        min-height: 100vh;
        margin: 0;
      }
      .login-box {
        background: white;
        padding: 40px;
        border-radius: 15px;
        box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        width: 100%;
        max-width: 400px;
      }
      .form-group {
        margin-bottom: 20px;
      }
      label {
        display: block;
        margin-bottom: 5px;
        font-weight: bold;
      }
      input {
        width: 100%;
        padding: 12px;
        border: 1px solid #ddd;
        border-radius: 5px;
        box-sizing: border-box;
      }
      .btn {
        width: 100%;
        padding: 12px;
        background: #007bff;
        color: white;
        border: none;
        border-radius: 5px;
        cursor: pointer;
        font-size: 16px;
      }
      .btn:hover {
        background: #0056b3;
      }
    </style>
  </head>
  <body>
    <div class="login-box">
      <h2 style="text-align: center; margin-bottom: 30px;">🔐 Admin Login</h2>
      <form action="/admin" method="POST">
        <div class="form-group">
          <label>Username:</label>
          <input type="text" name="user" required>
        </div>
        <div class="form-group">
          <label>Password:</label>
          <input type="password" name="pass" required>
        </div>
        <button type="submit" class="btn">Login</button>
      </form>
    </div>
  </body>
  </html>
  )rawliteral";
}

String makeAdminDashboard() {
  stats.adminLogins++;
  saveStatsToSPIFFS();
  
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Admin Dashboard</title>
    <style>
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        margin: 0;
        padding: 20px;
        background: #f8f9fa;
      }
      .container {
        max-width: 1200px;
        margin: 0 auto;
      }
      .card {
        background: white;
        padding: 20px;
        margin: 10px 0;
        border-radius: 10px;
        box-shadow: 0 2px 10px rgba(0,0,0,0.1);
      }
      .btn {
        display: inline-block;
        padding: 10px 20px;
        background: #007bff;
        color: white;
        text-decoration: none;
        border-radius: 5px;
        border: none;
        cursor: pointer;
        margin: 5px;
      }
      .btn-danger { background: #dc3545; }
      .btn-success { background: #28a745; }
      .btn-warning { background: #ffc107; color: #000; }
      table {
        width: 100%;
        border-collapse: collapse;
        margin: 10px 0;
      }
      th, td {
        padding: 12px;
        text-align: left;
        border-bottom: 1px solid #ddd;
      }
      th {
        background: #007bff;
        color: white;
      }
      .form-group {
        margin-bottom: 15px;
      }
      label {
        display: block;
        margin-bottom: 5px;
        font-weight: bold;
      }
      input {
        width: 100%;
        padding: 8px;
        border: 1px solid #ddd;
        border-radius: 4px;
        box-sizing: border-box;
      }
      .tab {
        overflow: hidden;
        border: 1px solid #ccc;
        background-color: #f1f1f1;
        border-radius: 5px 5px 0 0;
      }
      .tab button {
        background-color: inherit;
        float: left;
        border: none;
        outline: none;
        cursor: pointer;
        padding: 14px 16px;
        transition: 0.3s;
      }
      .tab button:hover {
        background-color: #ddd;
      }
      .tab button.active {
        background-color: #007bff;
        color: white;
      }
      .tabcontent {
        display: none;
        padding: 20px;
        border: 1px solid #ccc;
        border-top: none;
        border-radius: 0 0 5px 5px;
        background: white;
      }
      .stat-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
        gap: 15px;
        margin: 20px 0;
      }
      .stat-card {
        text-align: center;
        padding: 20px;
        border-radius: 8px;
        color: white;
      }
      .stat-number {
        font-size: 2em;
        font-weight: bold;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>🛠️ Admin Dashboard v1.0</h1>
      
      <div class="tab">
        <button class="tablinks active" onclick="openTab(event, 'Overview')">Overview</button>
        <button class="tablinks" onclick="openTab(event, 'Sessions')">Sessions</button>
        <button class="tablinks" onclick="openTab(event, 'Settings')">Settings</button>
        <button class="tablinks" onclick="openTab(event, 'System')">System</button>
        <button class="tablinks" onclick="openTab(event, 'Analytics')">Analytics</button>
      </div>

      <div id="Overview" class="tabcontent" style="display: block;">
        <h2>System Overview</h2>
        <div class="card">
          <h3>📈 Statistics</h3>
          <div class="stat-grid">
            <div class="stat-card" style="background: #007bff;">
              <div class="stat-number">)rawliteral" + String(stats.totalGrants) + R"rawliteral(</div>
              <div>Total Grants</div>
            </div>
            <div class="stat-card" style="background: #ffc107;">
              <div class="stat-number">)rawliteral" + String(getActiveSessionCount()) + R"rawliteral(</div>
              <div>Active Sessions</div>
            </div>
            <div class="stat-card" style="background: #28a745;">
              <div class="stat-number">)rawliteral" + String(stats.adminLogins) + R"rawliteral(</div>
              <div>Admin Logins</div>
            </div>
            <div class="stat-card" style="background: #dc3545;">
              <div class="stat-number">)rawliteral" + String(stats.errors) + R"rawliteral(</div>
              <div>System Errors</div>
            </div>
            <div class="stat-card" style="background: #6f42c1;">
              <div class="stat-number">)rawliteral" + String(stats.adsShown) + R"rawliteral(</div>
              <div>Ads Shown</div>
            </div>
            <div class="stat-card" style="background: #20c997;">
              <div class="stat-number">)rawliteral" + formatDataUsage(stats.totalDataUsed) + R"rawliteral(</div>
              <div>Total Data</div>
            </div>
          </div>
        </div>

        <div class="card">
          <h3>⚡ Quick Actions</h3>
          <button class="btn btn-warning" onclick="restartSystem()">🔄 Restart System</button>
          <button class="btn btn-danger" onclick="kickAllSessions()">🚫 Kick All Sessions</button>
          <button class="btn" onclick="refreshStats()">🔄 Refresh Stats</button>
          <a href="/" class="btn btn-success">🏠 Portal Home</a>
        </div>
      </div>

      <div id="Sessions" class="tabcontent">
        <h2>Active Sessions Management</h2>
        <div class="card">
          <h3>Connected Clients ()rawliteral" + String(getActiveSessionCount()) + R"rawliteral(</h3>
          )rawliteral";
  
  if(getActiveSessionCount() > 0) {
    html += "<table><tr><th>IP Address</th><th>Device</th><th>Package</th><th>Granted</th><th>Expires In</th><th>Data Used</th><th>Actions</th></tr>";
    
    unsigned long now = millis();
    for(int i = 0; i < MAX_SESSIONS; i++) {
      if(sessions[i].active) {
        long timeLeft = (sessions[i].expireAt - now) / 1000 / 60; // minutes
        html += "<tr>";
        html += "<td>" + sessions[i].ip + "</td>";
        html += "<td>" + sessions[i].deviceType + "</td>";
        html += "<td>" + sessions[i].activePackage + "</td>";
        html += "<td>" + String((now - sessions[i].grantTime) / 60000) + " min ago</td>";
        html += "<td>" + String(timeLeft) + " min</td>";
        html += "<td>" + formatDataUsage(sessions[i].dataUsed) + "</td>";
        html += "<td><button class='btn btn-danger' onclick=\"kickIP('" + sessions[i].ip + "')\">Kick</button></td>";
        html += "</tr>";
      }
    }
    html += "</table>";
  } else {
    html += "<p>No active sessions</p>";
  }
  
  html += R"rawliteral(
        </div>
      </div>

      <div id="Settings" class="tabcontent">
        <h2>System Settings</h2>
        <div class="card">
          <h3>📡 Access Point Settings</h3>
          <form action="/admin/update" method="POST">
            <div class="form-group">
              <label>AP SSID:</label>
              <input type="text" name="ap_ssid" value=")rawliteral" + apSSID + R"rawliteral(" required>
            </div>
            <div class="form-group">
              <label>AP Password (leave empty for open):</label>
              <input type="text" name="ap_password" value=")rawliteral" + apPassword + R"rawliteral(">
            </div>
            
            <h3>📶 Uplink Settings</h3>
            <div class="form-group">
              <label>Uplink SSID:</label>
              <input type="text" name="uplink_ssid" value=")rawliteral" + uplinkSSID + R"rawliteral(">
            </div>
            <div class="form-group">
              <label>Uplink Password:</label>
              <input type="text" name="uplink_pass" value=")rawliteral" + uplinkPASS + R"rawliteral(">
            </div>
            
            <button type="submit" class="btn">💾 Save All Settings</button>
          </form>
        </div>
      </div>

      <div id="System" class="tabcontent">
        <h2>System Information</h2>
        <div class="card">
          <h3>🖥️ Hardware Info</h3>
          <p><strong>Uptime:</strong> )rawliteral" + getSystemUptime() + R"rawliteral(</p>
          <p><strong>Memory:</strong> )rawliteral" + getMemoryInfo() + R"rawliteral(</p>
          <p><strong>AP Status:</strong> )rawliteral" + String(isAPRunning ? "✅ Running" : "❌ Stopped") + R"rawliteral(</p>
          <p><strong>STA Status:</strong> )rawliteral" + String(isSTAConnected ? "✅ Connected" : "❌ Disconnected") + R"rawliteral(</p>
          <p><strong>SPIFFS:</strong> )rawliteral" + String(isSPIFFSMounted ? "✅ Mounted" : "❌ Failed") + R"rawliteral(</p>
          <p><strong>AP IP:</strong> )rawliteral" + WiFi.softAPIP().toString() + R"rawliteral(</p>
          <p><strong>AP Clients:</strong> )rawliteral" + String(WiFi.softAPgetStationNum()) + R"rawliteral(</p>
          )rawliteral" + 
          (isSTAConnected ? 
            "<p><strong>STA IP:</strong> " + WiFi.localIP().toString() + "</p>" +
            "<p><strong>Signal Strength:</strong> " + String(WiFi.RSSI()) + " dBm</p>" +
            "<p><strong>Gateway:</strong> " + WiFi.gatewayIP().toString() + "</p>"
          : "") +
          R"rawliteral(
        </div>
      </div>

      <div id="Analytics" class="tabcontent">
        <h2>📊 Task Analytics</h2>
        <div class="card">
          <h3>Task Completion Statistics</h3>
          <div class="stat-grid">
            <div class="stat-card" style="background: #fd7e14;">
              <div class="stat-number">)rawliteral" + String(stats.simpleTasks) + R"rawliteral(</div>
              <div>Quick Tasks</div>
            </div>
            <div class="stat-card" style="background: #e83e8c;">
              <div class="stat-number">)rawliteral" + String(stats.youtubeTasks) + R"rawliteral(</div>
              <div>YouTube Tasks</div>
            </div>
            <div class="stat-card" style="background: #6f42c1;">
              <div class="stat-number">)rawliteral" + String(stats.instagramTasks) + R"rawliteral(</div>
              <div>Instagram Tasks</div>
            </div>
            <div class="stat-card" style="background: #20c997;">
              <div class="stat-number">)rawliteral" + String(stats.adsShown) + R"rawliteral(</div>
              <div>Ads Watched</div>
            </div>
          </div>
        </div>
        
        <div class="card">
          <h3>Device Distribution</h3>
          <table>
            <tr><th>Device Type</th><th>Count</th></tr>
            )rawliteral";
            
  // Count devices
  int androidCount = 0, iosCount = 0, windowsCount = 0, macCount = 0, linuxCount = 0, unknownCount = 0;
  for(int i = 0; i < MAX_SESSIONS; i++) {
    if(sessions[i].active) {
      if(sessions[i].deviceType == "Android") androidCount++;
      else if(sessions[i].deviceType == "iOS") iosCount++;
      else if(sessions[i].deviceType == "Windows") windowsCount++;
      else if(sessions[i].deviceType == "Mac") macCount++;
      else if(sessions[i].deviceType == "Linux") linuxCount++;
      else unknownCount++;
    }
  }
  
  html += "<tr><td>Android</td><td>" + String(androidCount) + "</td></tr>";
  html += "<tr><td>iOS</td><td>" + String(iosCount) + "</td></tr>";
  html += "<tr><td>Windows</td><td>" + String(windowsCount) + "</td></tr>";
  html += "<tr><td>Mac</td><td>" + String(macCount) + "</td></tr>";
  html += "<tr><td>Linux</td><td>" + String(linuxCount) + "</td></tr>";
  html += "<tr><td>Unknown</td><td>" + String(unknownCount) + "</td></tr>";
  
  html += R"rawliteral(
          </table>
        </div>
      </div>
    </div>

    <script>
      function openTab(evt, tabName) {
        var i, tabcontent, tablinks;
        tabcontent = document.getElementsByClassName("tabcontent");
        for (i = 0; i < tabcontent.length; i++) {
          tabcontent[i].style.display = "none";
        }
        tablinks = document.getElementsByClassName("tablinks");
        for (i = 0; i < tablinks.length; i++) {
          tablinks[i].className = tablinks[i].className.replace(" active", "");
        }
        document.getElementById(tabName).style.display = "block";
        evt.currentTarget.className += " active";
      }

      function kickIP(ip) {
        if(confirm('Are you sure you want to kick ' + ip + '?')) {
          fetch('/admin/kick?ip=' + encodeURIComponent(ip))
            .then(r => r.text())
            .then(result => {
              alert(result);
              location.reload();
            });
        }
      }

      function kickAllSessions() {
        if(confirm('Are you sure you want to kick ALL active sessions?')) {
          fetch('/admin/kickall')
            .then(r => r.text())
            .then(result => {
              alert(result);
              location.reload();
            });
        }
      }

      function restartSystem() {
        if(confirm('Are you sure you want to restart the system?')) {
          fetch('/admin/restart')
            .then(r => r.text())
            .then(result => {
              alert('System restarting...');
            });
        }
      }

      function refreshStats() {
        location.reload();
      }
    </script>
  </body>
  </html>
  )rawliteral";
  
  return html;
}

String makeGrantedPage() {
  String clientIP = getClientIPString();
  int sessionIdx = findSessionByIP(clientIP);
  
  String packageInfo = "";
  String timeLeft = "";
  
  if(sessionIdx >= 0) {
    unsigned long now = millis();
    long timeLeftSec = (sessions[sessionIdx].expireAt - now) / 1000;
    long minutesLeft = timeLeftSec / 60;
    long hoursLeft = minutesLeft / 60;
    
    if (hoursLeft > 0) {
      timeLeft = String(hoursLeft) + "h " + String(minutesLeft % 60) + "m";
    } else {
      timeLeft = String(minutesLeft) + "m " + String(timeLeftSec % 60) + "s";
    }
    
    packageInfo = "<p><strong>Active Package:</strong> " + sessions[sessionIdx].activePackage + "</p>";
  }
  
  return R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Access Granted</title>
    <style>
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        text-align: center;
        padding: 50px 20px;
        background: linear-gradient(135deg, #28a745 0%, #20c997 100%);
        color: white;
        min-height: 100vh;
        margin: 0;
      }
      .container {
        max-width: 500px;
        margin: 0 auto;
        background: rgba(255,255,255,0.1);
        padding: 40px;
        border-radius: 20px;
        backdrop-filter: blur(10px);
      }
      .icon {
        font-size: 4em;
        margin-bottom: 20px;
      }
      .btn {
        display: inline-block;
        padding: 12px 25px;
        background: #00ff88;
        color: #333;
        border-radius: 50px;
        text-decoration: none;
        font-weight: bold;
        margin: 10px;
        transition: all 0.3s ease;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <div class="icon">✅</div>
      <h1>Access Granted!</h1>
      <p>You have been granted premium WiFi access.</p>
      )rawliteral" + packageInfo + R"rawliteral(
      <div style="margin-top: 30px; padding: 20px; background: rgba(255,255,255,0.2); border-radius: 10px;">
        <p><strong>Your IP:</strong> )rawliteral" + getClientIPString() + R"rawliteral(</p>
        <p><strong>Time Remaining:</strong> )rawliteral" + timeLeft + R"rawliteral(</p>
        <p><strong>Status:</strong> Active and Whitelisted</p>
      </div>
      <div style="margin-top: 30px;">
        <a href="/userstatus" class="btn">📊 My Status</a>
        <a href="/tasks" class="btn" style="background: #ffc107;">🎬 Get More Time</a>
        <a href="/status" class="btn" style="background: #17a2b8;">📡 Network Status</a>
      </div>
    </div>
  </body>
  </html>
  )rawliteral";
}

// ---------- REQUEST HANDLERS ----------

void handleRoot() {
  String clientIP = getClientIPString();
  logInfo("Root request from IP: " + clientIP);

  bool allowed = isWhitelisted(clientIP);
  if (!allowed) {
    allowed = isMacGranted(getClientMAC());
  }

  if(allowed) {
    server.send(200, "text/html", makeGrantedPage());
  } else {
    server.send(200, "text/html", WELCOME_HTML);
  }
}

void handleTasks() {
  server.send(200, "text/html", makeTasksPage());
}

void handleDoTask() {
  if(server.hasArg("task")) {
    String taskId = server.arg("task");
    server.send(200, "text/html", makeDoTaskPage(taskId));
  } else {
    server.send(400, "text/plain", "Missing task parameter");
  }
}

void handleGrantTask() {
  String clientIP = getClientIPString();
  String taskId = server.arg("task");
  
  logStep("Task completion for IP: " + clientIP + ", Task: " + taskId);
  
  for (int i = 0; i < TASK_COUNT; i++) {
    if (availableTasks[i].id == taskId) {
      grantAccessWithDuration(clientIP, availableTasks[i].rewardDuration, availableTasks[i].name);

      // ✅ Instantly unlock the device and close the captive portal
      unlockClientAndRedirect(clientIP, getClientMAC());

      // Update stats
      if (availableTasks[i].type == "youtube") stats.youtubeTasks++;
      else if (availableTasks[i].type == "instagram") stats.instagramTasks++;
      else if (availableTasks[i].type == "ad") stats.adsShown++;
      else if (availableTasks[i].type == "simple") stats.simpleTasks++;
      
      saveStatsToSPIFFS();
      
      server.send(200, "text/plain", "Access granted for " + String(availableTasks[i].rewardDuration / 60000) + " minutes");
      return;
    }
  }

  server.send(400, "text/plain", "Invalid task ID");
}


void handleUserStatus() {
  server.send(200, "text/html", makeUserStatusPage());
}

void handleStatus() {
  logInfo("Status page requested");
  server.send(200, "text/html", makeStatusPage());
}

void handleGrant() {
  String clientIP = getClientIPString();
  logStep("Grant request from IP: " + clientIP);
  
  grantIP(clientIP);
  String resp = "Access granted! IP " + clientIP + " is whitelisted for 2 hours. You may need to reconnect to the network.";
  server.send(200, "text/plain", resp);
}

void handleAdminLogin() {
  if(server.method() == HTTP_POST) {
    String user = server.arg("user");
    String pass = server.arg("pass");
    
    if(user == ADMIN_USER && pass == ADMIN_PASS) {
      logInfo("Admin login successful from IP: " + getClientIPString());
      server.send(200, "text/html", makeAdminDashboard());
    } else {
      logWarning("Failed admin login attempt from IP: " + getClientIPString());
      server.send(401, "text/plain", "Unauthorized - Invalid credentials");
    }
  } else {
    server.send(200, "text/html", makeAdminLoginPage());
  }
}

void handleAdminUpdate() {
  if(server.method() == HTTP_POST) {
    logStep("Admin settings update requested");
    
    String newUplinkSSID = server.arg("uplink_ssid");
    String newUplinkPASS = server.arg("uplink_pass");
    String newAPSSID = server.arg("ap_ssid");
    String newAPPASSWORD = server.arg("ap_password");
    
    bool changed = false;
    
    if(newUplinkSSID.length() > 0 && newUplinkSSID != uplinkSSID) {
      uplinkSSID = newUplinkSSID;
      changed = true;
      logInfo("Uplink SSID updated to: " + uplinkSSID);
    }
    
    if(newUplinkPASS != uplinkPASS) {
      uplinkPASS = newUplinkPASS;
      changed = true;
      logInfo("Uplink password updated");
    }
    
    if(newAPSSID.length() > 0 && newAPSSID != apSSID) {
      apSSID = newAPSSID;
      changed = true;
      logInfo("AP SSID updated to: " + apSSID);
    }
    
    if(newAPPASSWORD != apPassword) {
      apPassword = newAPPASSWORD;
      changed = true;
      logInfo("AP password updated");
    }
    
    if(changed) {
      saveSettingsToSPIFFS();
      server.send(200, "text/plain", "Settings updated successfully. AP will restart with new settings.");
      
      // Restart AP with new settings
      delay(1000);
      WiFi.softAPdisconnect(true);
      delay(1000);
      startAccessPoint();
      
    } else {
      server.send(200, "text/plain", "No changes detected.");
    }
  } else {
    server.send(405, "text/plain", "Method not allowed");
  }
}

void handleAdminKick() {
  if(server.hasArg("ip")) {
    String ip = server.arg("ip");
    kickIP(ip);
    server.send(200, "text/plain", "Kicked IP: " + ip);
  } else {
    server.send(400, "text/plain", "Bad request - IP parameter required");
  }
}

void handleAdminKickAll() {
  kickAllSessions();
  server.send(200, "text/plain", "All sessions have been kicked");
}

void handleAdminRestart() {
  server.send(200, "text/plain", "System restarting...");
  logStep("Admin requested system restart");
  delay(1000);
  ESP.restart();
}

void handleApiStats() {
  DynamicJsonDocument doc(512);
  doc["activeSessions"] = getActiveSessionCount();
  doc["uptime"] = getSystemUptime();
  doc["memory"] = esp_get_free_heap_size();
  doc["apStatus"] = isAPRunning;
  doc["staStatus"] = isSTAConnected;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleNotFound() {
  String clientIP = getClientIPString();
  logInfo("Not found: " + server.uri() + " from IP: " + clientIP);

  // If IP has an active session, or MAC is granted, do not force captive portal
  bool allowed = isWhitelisted(clientIP);
  if (!allowed) {
    allowed = isMacGranted(getClientMAC());
  }

  if (allowed) {
    server.send(200, "text/plain", "Connected - You have internet access.");
    return;
  }

  // Non-whitelisted users get redirected to captive portal
  String redirectUrl = "http://" + WiFi.softAPIP().toString();
  server.sendHeader("Location", redirectUrl, true);
  server.send(302, "text/plain", "Redirecting to captive portal...");
}

// ---------- SERVER SETUP ----------

void startCaptiveDNS() {
  logStep("Starting DNS captive portal...");
  IPAddress apIP = WiFi.softAPIP();
  
  // Start DNS server for captive portal
  dnsServer.start(DNS_PORT, "*", apIP);
  
  logSuccess("DNS server started on port " + String(DNS_PORT));
  logInfo("All DNS requests will be redirected to: " + apIP.toString());
}

void setupServerRoutes() {
  logStep("Setting up web server routes...");
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/tasks", HTTP_GET, handleTasks);
  server.on("/dotask", HTTP_GET, handleDoTask);
  server.on("/granttask", HTTP_POST, handleGrantTask);
  server.on("/userstatus", HTTP_GET, handleUserStatus);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/grant", HTTP_POST, handleGrant);
  server.on("/admin", HTTP_ANY, handleAdminLogin);
  server.on("/admin/update", HTTP_POST, handleAdminUpdate);
  server.on("/admin/kick", HTTP_GET, handleAdminKick);
  server.on("/admin/kickall", HTTP_GET, handleAdminKickAll);
  server.on("/admin/restart", HTTP_GET, handleAdminRestart);
  server.on("/api/stats", HTTP_GET, handleApiStats);
  server.onNotFound(handleNotFound);

  
  // ✅ Add this line here — it sets up the Android/iOS captive-portal detection routes
  setupCaptiveHandlers();


  server.begin();
  logSuccess("Web server started on port 80");
}

bool initializeSPIFFS() {
  logStep("Initializing SPIFFS...");
  
  if(!SPIFFS.begin(true)) {
    logError("SPIFFS mount failed!");
    isSPIFFSMounted = false;
    return false;
  }
  
  isSPIFFSMounted = true;
  logSuccess("SPIFFS mounted successfully");
  
  // List files for debugging
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  logInfo("SPIFFS Files:");
  while(file) {
    logInfo("  " + String(file.name()) + " (" + String(file.size()) + " bytes)");
    file = root.openNextFile();
  }
  
  return true;
}

void printBanner() {
  Serial.println("\n");
  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                   JAVED'S FREEWIFI - v1.0                   ║");
  Serial.println("║              Ultimate Captive Portal System                 ║");
  Serial.println("║               Enhanced Task & Ad Management                 ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  Serial.println();
}

// ---------- DATA USAGE SIMULATION ----------

void simulateDataUsage() {
  // Simulate data usage for active sessions
  for(int i = 0; i < MAX_SESSIONS; i++) {
    if(sessions[i].active) {
      // Add random data usage between 1KB and 100KB per interval
      unsigned long dataUsed = random(1024, 102400);
      sessions[i].dataUsed += dataUsed;
    }
  }
}

// ---------- MAIN SETUP ----------

void setup() {
  // Initialize serial communication
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  
  printBanner();
  logStep("System initialization started...");
  
  // Initialize components in sequence
  if(!initializeSPIFFS()) {
    logError("Critical: SPIFFS initialization failed!");
    // Continue anyway, but with limited functionality
  }
  
  initializeStats();
  loadStatsFromSPIFFS();
  loadSettingsFromSPIFFS();
  initializeSessions();
  loadSessionsFromSPIFFS();
  
  logStep("Setting WiFi mode to AP+STA...");
  WiFi.mode(WIFI_AP_STA);
  
  // Start Access Point
  if(!startAccessPoint()) {
    logError("Critical: Failed to start Access Point! System may not function properly.");
    // Don't halt, try to continue
  }
  
  // Attempt uplink connection (non-blocking)
  logStep("Starting uplink connection attempt...");
  connectToUplink();
  
  // Start services
  startCaptiveDNS();
  setupServerRoutes();
  
  // Print final system status
  printNetworkInfo();
  
  logSuccess("=== SYSTEM READY ===");
  logInfo("Connect to WiFi: " + apSSID);
  logInfo("Portal URL: http://" + WiFi.softAPIP().toString());
  logInfo("System Uptime: " + getSystemUptime());
  logInfo("Active Sessions: " + String(getActiveSessionCount()));
  logInfo("Memory: " + String(esp_get_free_heap_size()) + " bytes free");
  logInfo("Available Tasks: " + String(TASK_COUNT) + " different options");
}

// ---------- MAIN LOOP ----------

void loop() {
  // Handle DNS requests
  dnsServer.processNextRequest();
  
  // Handle web requests
  server.handleClient();
  
  
  maintainGrants();   // ✅ Keeps checking for expired clients


  // Maintenance tasks
  static unsigned long lastMaintenance = 0;
  if(millis() - lastMaintenance > 30000) { // Every 30 seconds
    lastMaintenance = millis();
    purgeExpiredSessions();
    simulateDataUsage();
  }
  
  // Update system uptime
  systemUptime = millis();
  
  // Periodic status reporting (every 60 seconds)
  static unsigned long lastStatusReport = 0;
  if(millis() - lastStatusReport > 60000) {
    lastStatusReport = millis();
    
    int activeSessions = getActiveSessionCount();
    int connectedClients = WiFi.softAPgetStationNum();
    
    logStep("=== SYSTEM STATUS ===");
    logInfo("Uptime: " + getSystemUptime());
    logInfo("Active Sessions: " + String(activeSessions));
    logInfo("Connected Clients: " + String(connectedClients));
    logInfo("Memory: " + getMemoryInfo());
    logInfo("AP Running: " + String(isAPRunning ? "Yes" : "No"));
    logInfo("STA Connected: " + String(isSTAConnected ? "Yes" : "No"));
    
    // Auto-reconnect STA if disconnected
    if(!isSTAConnected && uplinkSSID.length() > 0) {
      logStep("Attempting STA reconnection...");
      connectToUplink();
    }
  }
  
  delay(10); // Small delay to prevent watchdog timer issues
}



// ---- ADDED: Captive-portal release & Android/iOS detection handlers ----
// Fixes the “Sign in to network” issue after a successful reward
// Works by stopping DNS redirection + returning correct HTTP 204/200 responses

// ---- FIXED CAPTIVE PORTAL RELEASE PATCH ----
// No duplicate GRANT_DURATION_MS, no getStatus() calls

#include <vector>
#include <map>

struct GrantInfo {
  uint64_t expiryMillis;
};

std::map<String, GrantInfo> grantedClients;

// We'll reuse your existing GRANT_DURATION_MS from earlier in code
bool dnsActive = true; // track if DNS is currently running

// ---- Grant client access ----
void grantClient(const String &macStr) {
  uint64_t now = millis();
  if (macStr.length() == 0) {
    Serial.println("[WARN] grantClient called with empty MAC; skipping.");
    return;
  }
  grantedClients[macStr] = { now + GRANT_DURATION_MS };
  Serial.println("[INFO] Client granted access: " + macStr);
}

// ---- Maintain grants (call in loop()) ----
void maintainGrants() {
  uint64_t now = millis();
  bool anyValid = false;
  std::vector<String> toRemove;

  for (auto &p : grantedClients) {
    if (p.second.expiryMillis > now) anyValid = true;
    else toRemove.push_back(p.first);
  }
  for (auto &k : toRemove) grantedClients.erase(k);

  // Toggle DNS captive redirection depending on whether any clients are granted
  if (anyValid && dnsActive) {
    dnsServer.stop();
    dnsActive = false;
    Serial.println("[INFO] DNS server stopped (active grants present). Real DNS flows for clients.");
  } else if (!anyValid && !dnsActive) {
    // Restart DNS if no active grants
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    dnsActive = true;
    Serial.println("[INFO] DNS server restarted (no active grants).");
  }
}

// ---- Check if client is granted ----
bool isMacGranted(const String &mac) {
  auto it = grantedClients.find(mac);
  if (it == grantedClients.end()) return false;
  return it->second.expiryMillis > millis();
}

// ---- Setup captive-portal handlers ----
void setupCaptiveHandlers() {
  // Android captive-portal check
  server.on("/generate_204", [](){
    // If the client is allowed, respond 204 so the OS closes the captive UI
    String ip = getClientIPString();
    bool allowed = isWhitelisted(ip);
    if (!allowed) allowed = isMacGranted(getClientMAC());
    if (allowed) server.send(204, "text/plain", "");
    else server.send(200, "text/html", WELCOME_HTML);
  });
  // Android 6+ connectivity check
  server.on("/connecttest.txt", [](){
    String ip = getClientIPString();
    bool allowed = isWhitelisted(ip);
    if (!allowed) allowed = isMacGranted(getClientMAC());
    if (allowed) server.send(200, "text/plain", "OK");
    else server.send(200, "text/html", WELCOME_HTML);
  });
  // iOS / macOS captive-portal check
  server.on("/hotspot-detect.html", [](){
    String ip = getClientIPString();
    bool allowed = isWhitelisted(ip);
    if (!allowed) allowed = isMacGranted(getClientMAC());
    if (allowed) server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    else server.send(200, "text/html", WELCOME_HTML);
  });
  // Windows NCSI
  server.on("/ncsi.txt", [](){
    String ip = getClientIPString();
    bool allowed = isWhitelisted(ip);
    if (!allowed) allowed = isMacGranted(getClientMAC());
    if (allowed) server.send(200, "text/plain", "Microsoft NCSI");
    else server.send(200, "text/html", WELCOME_HTML);
  });
  // Misc success endpoint
  server.on("/success.txt", [](){
    String ip = getClientIPString();
    bool allowed = isWhitelisted(ip);
    if (!allowed) allowed = isMacGranted(getClientMAC());
    if (allowed) server.send(200, "text/plain", "Success");
    else server.send(200, "text/html", WELCOME_HTML);
  });

  Serial.println("[INFO] Captive portal detection handlers set up.");
}

// ---- Unlock & redirect client after reward ----
void unlockClientAndRedirect(const String &ipStr, const String &macStr) {
  grantClient(macStr);
  // Prefer OS-specific success URL to close captive portal cleanly
  delay(150);
  String userAgent = server.header("User-Agent");
  userAgent.toLowerCase();
  String target;
  if (userAgent.indexOf("android") != -1) {
    target = "http://connectivitycheck.gstatic.com/generate_204";
  } else if (userAgent.indexOf("iphone") != -1 || userAgent.indexOf("ipad") != -1 || userAgent.indexOf("mac") != -1) {
    target = "http://captive.apple.com/hotspot-detect.html";
  } else if (userAgent.indexOf("windows") != -1) {
    target = "http://www.msftconnecttest.com/connecttest.txt";
  } else {
    target = "/userstatus"; // fallback
  }
  server.sendHeader("Location", target);
  server.send(302, "text/plain", "Redirecting...");
  Serial.println("[INFO] Client redirected to: " + target);
}

// ---- END FIXED PATCH ----
