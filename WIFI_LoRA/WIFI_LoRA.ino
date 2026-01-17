/*
 * ESP32 LoRaWAN 发送端 - 通用过滤版 (无白名单)
 * * 逻辑：
 * 1. 扫描所有 WiFi。
 * 2. 过滤掉所有“随机 MAC 地址”（通常是手机热点）。
 * 3. 在剩下的（看起来像固定路由器的）信号中，选出最强的 3 个。
 * 4. 发送 LoRaWAN 数据。
 */

#include "WiFi.h"
#include <HardwareSerial.h>

// --- LoRa E5 配置 ---
HardwareSerial loraSerial(2); // UART2: RX=16, TX=17

// --- TTN (OTAA) 凭证 ---
const char* devEUI = "70B3D57ED0073415";
const char* appEUI = "0000000000000000";
const char* appKey = "44B8F00FFE627DC84F204831CF7519C6";

// 注意：我们删除了 known_aps 数组，现在不再需要它了

// 结构体：存储找到的 AP 信息
struct APData {
  String mac;
  int8_t rssi;
};
// 存储前 3 名
APData topAPs[3] = {
  {"00:00:00:00:00:00", -127},
  {"00:00:00:00:00:00", -127},
  {"00:00:00:00:00:00", -127}
};

unsigned long previousMillis = 0;
const long interval = 60000; // 每 60 秒发送一次

// --- 函数声明 ---
void macToBytes(String macStr, byte* macBytes);
String sendATCommand(String command, int timeout);
void initializeLoRa();
bool isRandomMAC(String mac); // 核心过滤函数

void setup() {
  Serial.begin(115200);
  loraSerial.begin(9600, SERIAL_8N1, 16, 17);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("系统启动：通用过滤模式 (仅过滤随机MAC)...");
  initializeLoRa();
}

void loop() {
  if (millis() - previousMillis >= interval) {
    previousMillis = millis();
    
    // 1. 扫描并筛选
    scanAndSelectTop3();
    
    // 2. 发送数据 (如果至少找到了一个非随机 MAC)
    if (topAPs[0].rssi > -127) {
      sendLoRaData3();
    } else {
      Serial.println("本次扫描未发现固定路由器 (所有信号均为随机MAC)，跳过发送。");
    }
  }
}

// --- 核心逻辑：扫描并排序 ---
void scanAndSelectTop3() {
  Serial.println("开始 WiFi 扫描...");
  int n = WiFi.scanNetworks();
  
  // 重置前3名
  for(int i=0; i<3; i++) { 
    topAPs[i].mac = "00:00:00:00:00:00"; 
    topAPs[i].rssi = -127; 
  }

  if (n == 0) return;

  Serial.printf("发现 %d 个网络，正在筛选...\n", n);

  for (int i = 0; i < n; ++i) {
    String bssid = WiFi.BSSIDstr(i);
    // 注意：WiFi.BSSIDstr() 返回的通常是大写，但为了保险起见转大写
    bssid.toUpperCase(); 
    
    // *** 唯一的过滤条件：如果它不是随机 MAC，我们就认为它是路由器 ***
    if (!isRandomMAC(bssid)) {
      int32_t rssi = WiFi.RSSI(i);
      String ssid = WiFi.SSID(i);
      
      // 调试输出：让我们看看选了谁
      Serial.printf("  [接受] %s (%s) %d dBm\n", ssid.c_str(), bssid.c_str(), rssi);

      // 冒泡排序逻辑：插入到前3名
      if (rssi > topAPs[0].rssi) {
        topAPs[2] = topAPs[1]; topAPs[1] = topAPs[0];
        topAPs[0] = {bssid, (int8_t)rssi};
      } else if (rssi > topAPs[1].rssi) {
        topAPs[2] = topAPs[1];
        topAPs[1] = {bssid, (int8_t)rssi};
      } else if (rssi > topAPs[2].rssi) {
        topAPs[2] = {bssid, (int8_t)rssi};
      }
    } else {
      // 这是一个随机 MAC (可能是手机热点)
      // Serial.printf("  [忽略] %s (随机MAC)\n", WiFi.SSID(i).c_str());
    }
  }
}

// --- 发送逻辑 (21字节) ---
void sendLoRaData3() {
  byte payload[21];
  for(int i=0; i<3; i++) {
    macToBytes(topAPs[i].mac, &payload[i*7]);
    payload[i*7 + 6] = topAPs[i].rssi;
  }
  
  String hexPayload = "";
  for(int i = 0; i < 21; i++) {
    char hex[3];
    sprintf(hex, "%02X", payload[i]);
    hexPayload += hex;
  }
  
  Serial.println("LoRa 发送 (HEX): " + hexPayload);
  loraSerial.println("AT+MSGHEX=" + hexPayload); 
}

// --- 核心辅助函数：检测随机 MAC ---
bool isRandomMAC(String mac) {
  // MAC 格式 "XX:XX:..."
  if (mac.length() < 2) return false;

  // 获取第 2 个字符 (十六进制的低位)
  // 例如 "58:..." -> '8', "DA:..." -> 'A'
  char c = mac.charAt(1); 
  
  // 检查是否为 2, 6, A, E (大小写均可)
  if (c == '2' || c == '6' || 
      c == 'A' || c == 'a' || 
      c == 'E' || c == 'e') {
    return true; // 是随机 MAC (本地管理地址 -> 手机热点)
  }
  return false; // 是通用 MAC (全球唯一地址 -> 固定路由器)
}

void macToBytes(String macStr, byte* macBytes) {
  sscanf(macStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
         &macBytes[0], &macBytes[1], &macBytes[2], 
         &macBytes[3], &macBytes[4], &macBytes[5]);
}

String sendATCommand(String command, int timeout) {
  Serial.println("CMD: " + command);
  loraSerial.println(command);
  delay(100);
  String resp = "";
  long start = millis();
  while (millis() - start < timeout) {
    while(loraSerial.available()) resp += (char)loraSerial.read();
  }
  resp.trim();
  if(resp.length() > 0) Serial.println("RESP: " + resp);
  return resp;
}

void initializeLoRa() {
  sendATCommand("AT", 1000);
  sendATCommand("AT+MODE=LWOTAA", 1000);
  sendATCommand("AT+DR=EU868", 1000);
  sendATCommand("AT+ID=DevEui," + String(devEUI), 1000);
  sendATCommand("AT+ID=AppEui," + String(appEUI), 1000);
  sendATCommand("AT+KEY=APPKEY," + String(appKey), 1000);
  sendATCommand("AT+JOIN", 5000);
  delay(5000);
}