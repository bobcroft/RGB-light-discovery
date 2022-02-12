
//#define wifi_ssid "TP-Link_8A30"
//#define wifi_password "LAW-drop-comic"

#define wifi_ssid "Glebe-net"
#define wifi_password "1481belmont"

const unsigned long WIFI_CONNECTION_TIMEOUT{30000};  // Reset ESP if Wifi connection takes longer than duration (ms)
unsigned long startupTime = millis();
unsigned long wifiSetupStartTime{};  // WiFi set up time