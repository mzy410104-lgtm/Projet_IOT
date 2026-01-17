/*
 * ESP32 WiFi 嗅探器（步骤 1 - 智能过滤版）
 * * 功能：扫描 WiFi，基于 MAC 地址随机化规则过滤掉手机热点。
 * * 目标：只输出可能是固定基础设施（路由器）的 AP。
 */

#include "WiFi.h"

// --- 辅助函数：判断是否为随机 MAC 地址 ---
// 规则：检查 MAC 地址的第 2 个字符（16进制）。
// 如果是 2, 6, A, E，则表示该地址是“本地管理”的（随机生成的）。
bool isRandomMAC(String mac) {
  if (mac.length() < 2) return false; // 安全检查

  char c = mac.charAt(1); // 获取第 2 个字符 (索引为 1)
  
  // 检查是否为 2, 6, A, E (兼容大小写)
  if (c == '2' || c == '6' || 
      c == 'A' || c == 'a' || 
      c == 'E' || c == 'e') {
    return true; // 是随机 MAC (可能是热点)
  }
  return false; // 是通用 MAC (可能是固定路由器)
}

void setup() {
  Serial.begin(115200); 
  
// 设置 WiFi 为基站模式 (STA) 并断开连接 [cite: 8]
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("WiFi 嗅探器（智能过滤版）已启动。");
  Serial.println("正在过滤掉第2位是 2/6/A/E 的随机 MAC 地址...");
  Serial.println("输出格式：SSID,BSSID,RSSI,Channel");
  Serial.println("-------------------------------------------------");
}

void loop() {
  Serial.println("\n开始扫描...");
  
  int n = WiFi.scanNetworks();
  
  Serial.println("扫描完成。");

  if (n == 0) {
    Serial.println("未找到任何网络。");
  } else {
    // 计数器：记录过滤后剩下的网络数量
    int fixed_count = 0; 

    Serial.println("--- 可能是固定路由器的网络 (白名单候选) ---");
    Serial.println("SSID,BSSID,RSSI,Channel");

    for (int i = 0; i < n; ++i) {
      String bssid = WiFi.BSSIDstr(i); // 获取 MAC 地址
      
      // *** 关键修改：在这里进行筛选 ***
      // 如果不是随机 MAC，我们才打印它
      if (!isRandomMAC(bssid)) {
        
        String ssid = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        int32_t channel = WiFi.channel(i);

        Serial.print(ssid);
        Serial.print(",");
        Serial.print(bssid);
        Serial.print(",");
        Serial.print(rssi);
        Serial.print(",");
        Serial.println(channel);
        
        fixed_count++;
      }
      // 如果是随机 MAC (isRandomMAC 返回 true)，这行代码就会跳过它，不打印
    }
    
    Serial.printf("--- 统计: 原本发现 %d 个, 过滤后保留 %d 个 ---\n", n, fixed_count);
  }

  // 等待 10 秒
  delay(10000); 
}