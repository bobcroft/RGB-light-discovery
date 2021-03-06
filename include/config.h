/*
RGB discovery
*/

///////////////////////////////////////////////////////////////////////////
//   RGB PINS
///////////////////////////////////////////////////////////////////////////
#define RED_PIN   15    // D8
#define GREEN_PIN 12    // D6
#define BLUE_PIN  13    // D7
#define WHITE_PIN 14    // D5

///////////////////////////////////////////////////////////////////////////
//   MISC
///////////////////////////////////////////////////////////////////////////
#define FRIENDLY_NAME "MQTT RGBW disco Light"

// MQTT
#define MQTT_USERNAME     "bobmqtt"
#define MQTT_PASSWORD     "101196"
#define MQTT_SERVER       "192.168.1.196"
#define MQTT_SERVER_PORT  1883

#define MQTT_HOME_ASSISTANT_SUPPORT

#if defined(MQTT_HOME_ASSISTANT_SUPPORT)

// template: <discovery prefix>/light/<chip ID>/config, status, state or set
#define MQTT_CONFIG_TOPIC_TEMPLATE  "%s/light/%s/config"
#else
//
#endif

#define MQTT_STATE_TOPIC_TEMPLATE   "%s/rgbw/state"
#define MQTT_COMMAND_TOPIC_TEMPLATE "%s/rgbw/set"
#define MQTT_STATUS_TOPIC_TEMPLATE  "%s/rgbw/status" // MQTT connection: alive/dead

#define MQTT_HOME_ASSISTANT_DISCOVERY_PREFIX  "homeassistant"
#define MQTT_STATE_ON_PAYLOAD   "ON"
#define MQTT_STATE_OFF_PAYLOAD  "OFF"

#define MQTT_CONNECTION_TIMEOUT 15000

///////////////////////////////////////////////////////////////////////////
//   DEBUG
///////////////////////////////////////////////////////////////////////////
//#define DEBUG_TELNET
#define DEBUG_SERIAL

///////////////////////////////////////////////////////////////////////////
//   OTA
///////////////////////////////////////////////////////////////////////////
#define OTA
//#define OTA_HOSTNAME  "hostname"  // hostname esp8266-[ChipID] by default
//#define OTA_PASSWORD  "password"  // no password by default
//#define OTA_PORT      8266        // port 8266 by default
