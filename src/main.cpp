/*
RGB and white LED with discovery

*/

#include <Arduino.h>
#include "ha_mqtt_rgbw_light_with_discovery.h"
#include "globals.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <Streaming.h>

//****************************************

//
#pragma once
#ifndef _RGBW_
#define _RGBW_

//#include "config.h"

#define COLOR_TEMP_HA_MIN_IN_MIRED 154 // Home Assistant minimum color temperature
#define COLOR_TEMP_HA_MAX_IN_MIRED 500 // Home Assistant maximum color temperature
#define COLOR_TEMP_MIN_IN_KELVIN 1000  // minimum color temperature
#define COLOR_TEMP_MAX_IN_KELVIN 15000 // maximum color temperature

typedef struct Color
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t white;
};

enum CMD
{
  CMD_NOT_DEFINED,
  CMD_STATE_CHANGED,
};

#define EFFECT_NOT_DEFINED_NAME "None"
#define EFFECT_RAMBOW_NAME "Rainbow"
#define EFFECT_RAINBOW_DELAY 10

#define EFFECT_LIST EFFECT_RAMBOW_NAME

enum EFFECT
{
  EFFECT_NOT_DEFINED,
  EFFECT_RAMBOW,
};

class AIRGBWBulb
{
public:
  AIRGBWBulb(void);

  void init(void);
  void loop(void);

  bool getState(void);
  bool setState(bool p_state);

  uint8_t getBrightness(void);
  bool setBrightness(uint8_t p_brightness);

  Color getColor(void);
  bool setColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue);

  bool setWhite(uint8_t p_white);

  uint16_t getColorTemperature(void);
  bool setColorTemperature(uint16_t p_colorTemperature);

  bool setEffect(const char *p_effect);

private:
  bool m_state;
  uint8_t m_brightness;
  Color m_color;
  uint16_t m_colorTemperature;

  bool setColor();

  void rainbowEffect(uint8_t p_index);
};

#endif

//*************************************

#if defined(DEBUG_TELNET)
WiFiServer telnetServer(23);
WiFiClient telnetClient;
#define DEBUG_PRINT(x) telnetClient.print(x)
#define DEBUG_PRINTLN(x) telnetClient.println(x)

#elif defined(DEBUG_SERIAL)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

#if defined(MQTT_HOME_ASSISTANT_SUPPORT)
//**StaticJsonBuffer<256> staticJsonBuffer;
DynamicJsonDocument root (1024); // ******* Called globally, use this instead of local ?????
char jsonBuffer[512] = {0};
#endif

volatile uint8_t cmd = CMD_NOT_DEFINED; // start up value

// instantiates
AIRGBWBulb bulb;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

///////////////////////////////////////////////////////////////////////////
//   TELNET
///////////////////////////////////////////////////////////////////////////
/*
   Function called to handle Telnet clients
   https://www.youtube.com/watch?v=j9yW10OcahI
*/
#if defined(DEBUG_TELNET)
void handleTelnet(void)
{
  if (telnetServer.hasClient())
  {
    if (!telnetClient || !telnetClient.connected())
    {
      if (telnetClient)
      {
        telnetClient.stop();
      }
      telnetClient = telnetServer.available();
    }
    else
    {
      telnetServer.available().stop();
    }
  }
}
#endif

///////////////////////////////////////////////////////////////////////////
//   WiFi
///////////////////////////////////////////////////////////////////////////
/*
   Function called to handle WiFi events
*/
void handleWiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case WIFI_EVENT_STAMODE_GOT_IP:
    DEBUG_PRINTLN(F("\nINFO: WiFi connected"));
    DEBUG_PRINT(F("INFO: IP address: "));
    DEBUG_PRINTLN(WiFi.localIP());
    break;
  case WIFI_EVENT_STAMODE_DISCONNECTED:
    DEBUG_PRINTLN(F("\nERROR: WiFi losts connection"));
    /*
       TODO: Do something smarter than rebooting the device
    */
    delay(15000);
    ESP.restart();
    break;
  default:
    DEBUG_PRINT(F("\nINFO: WiFi event: "));
    DEBUG_PRINTLN(event);
    break;
  }
}

/*
   Function called to set up the connection to the WiFi AP
*/
void setupWiFi()
{
   // IPAddress local_IP(192, 168, 1, 156); // Witty cloud 1
  IPAddress local_IP(192, 168, 1, 61); // Wemos D1 Mini - 9
  IPAddress gateway(192, 168, 1, 255);
  IPAddress subnet(255, 255, 0, 0);
  IPAddress primaryDNS(192, 168, 1, 139); // optional
  IPAddress secondaryDNS(1, 1, 1, 1);     // optional

  const unsigned long WIFI_CONNECTION_TIMEOUT{30000}; // Reset ESP if Wifi connection takes longer than duration (ms)
  unsigned long startupTime = millis();
  unsigned long wifiSetupStartTime{}; // WiFi set up time

  wifiSetupStartTime = millis();
  delay(10);
  // * Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet))
  {
    Serial << "\n Static IP: Failed to configure"
           << "\n\n";
  }
  // * If WIFI already setup it must not be set up again. Otherwise it won't reconnect after deepsleep

  if (WiFi.SSID() != wifi_ssid)
  {
    DEBUG_PRINT(F("\n\nINFO: WiFi connecting to: "));
    DEBUG_PRINTLN(wifi_ssid);
    /// Serial << "\n Connecting to " << (wifi_ssid) << '\n';
    /// WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_password);
  }
  else
  {
    Serial << "\n WiFi already known: " << (wifi_ssid) << '\n';
  }

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(200);
    Serial << (".");

    if (millis() - wifiSetupStartTime > WIFI_CONNECTION_TIMEOUT)
    {
      Serial << "\nWifi connection took longer than " << (WIFI_CONNECTION_TIMEOUT) << '\n';
      // goToSleepAndWakeUp();
    }
  }

  Serial << "\n\n WiFi connected - IP address: " << (WiFi.localIP()) << '\n';
  Serial << "\n Wifi connection duration (ms): " << (millis() - wifiSetupStartTime) << '\n';
}

///////////////////////////////////////////////////////////////////////////
//   OTA
///////////////////////////////////////////////////////////////////////////
#if defined(OTA)
/*
   Function called to setup OTA updates
*/
void setupOTA()
{
#if defined(OTA_HOSTNAME)
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  DEBUG_PRINT(F("\nINFO: OTA hostname sets to: "));
  DEBUG_PRINTLN(OTA_HOSTNAME);
#endif

#if defined(OTA_PORT)
  ArduinoOTA.setPort(OTA_PORT);
  DEBUG_PRINT(F("\nINFO: OTA port sets to: "));
  DEBUG_PRINTLN(OTA_PORT);
#endif

#if defined(OTA_PASSWORD)
  ArduinoOTA.setPassword((const char *)OTA_PASSWORD);
  DEBUG_PRINT(F("\nINFO: OTA password sets to: "));
  DEBUG_PRINTLN(OTA_PASSWORD);
#endif

  ArduinoOTA.onStart([]()
                     { DEBUG_PRINTLN(F("\nINFO: OTA starts")); });
  ArduinoOTA.onEnd([]()
                   { DEBUG_PRINTLN(F("\nINFO: OTA ends")); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
    DEBUG_PRINT(F("\nINFO: OTA progresses: "));
    DEBUG_PRINT(progress / (total / 100));
    DEBUG_PRINTLN(F("%")); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    DEBUG_PRINT(F("ERROR: OTA error: "));
    DEBUG_PRINTLN(error);
    if (error == OTA_AUTH_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA auth failed"));
    else if (error == OTA_BEGIN_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA begin failed"));
    else if (error == OTA_CONNECT_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA connect failed"));
    else if (error == OTA_RECEIVE_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA receive failed"));
    else if (error == OTA_END_ERROR)
      DEBUG_PRINTLN(F("ERROR: OTA end failed")); });
  ArduinoOTA.begin();
}

/*
   Function called to handle OTA updates
*/
void handleOTA()
{
  ArduinoOTA.handle();
}
#endif

///////////////////////////////////////////////////////////////////////////
//   MQTT
///////////////////////////////////////////////////////////////////////////

char MQTT_CLIENT_ID[7] = {0};
#if defined(MQTT_HOME_ASSISTANT_SUPPORT)
char MQTT_CONFIG_TOPIC[sizeof(MQTT_HOME_ASSISTANT_DISCOVERY_PREFIX) + sizeof(MQTT_CLIENT_ID) + sizeof(MQTT_CONFIG_TOPIC_TEMPLATE) - 4] = {0};
#else

#endif

char MQTT_STATE_TOPIC[sizeof(MQTT_CLIENT_ID) + sizeof(MQTT_STATE_TOPIC_TEMPLATE) - 2] = {0};
char MQTT_COMMAND_TOPIC[sizeof(MQTT_CLIENT_ID) + sizeof(MQTT_COMMAND_TOPIC_TEMPLATE) - 2] = {0};
char MQTT_STATUS_TOPIC[sizeof(MQTT_CLIENT_ID) + sizeof(MQTT_STATUS_TOPIC_TEMPLATE) - 2] = {0};

volatile unsigned long lastMQTTConnection = MQTT_CONNECTION_TIMEOUT;
/*
   Function called when a MQTT message has arrived
   @param p_topic   The topic of the MQTT message
   @param p_payload The payload of the MQTT message
   @param p_length  The length of the payload
*/
void handleMQTTMessage(char *p_topic, byte *p_payload, unsigned int p_length)
{
  // concatenates the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++)
  {
    payload.concat((char)p_payload[i]);
  }

  DEBUG_PRINTLN(F("\nINFO: New MQTT message received"));
  DEBUG_PRINT(F("\nINFO: MQTT topic: "));
  DEBUG_PRINTLN(p_topic);
  DEBUG_PRINT(F("\nINFO: MQTT payload: "));
  DEBUG_PRINTLN(payload);

  if (String(MQTT_COMMAND_TOPIC).equals(p_topic))
  {
    // DynamicJsonBuffer dynamicJsonBuffer;
    DynamicJsonDocument root(1024);
    // JsonObject &root = dynamicJsonBuffer.parseObject(p_payload);
    bool check = deserializeJson(root, p_payload, p_length);

    if (!check)
    {
      DEBUG_PRINTLN(F("\nERROR: parseObject() failed"));
      return;
    }

    if (root.containsKey("state"))
    {
      if (strcmp(root["state"], MQTT_STATE_ON_PAYLOAD) == 0)
      {
        if (bulb.setState(true))
        {
          DEBUG_PRINT(F("\nINFO: State changed to: "));
          DEBUG_PRINTLN(bulb.getState());
          cmd = CMD_STATE_CHANGED;
        }
      }
      else if (strcmp(root["state"], MQTT_STATE_OFF_PAYLOAD) == 0)
      {
        // stops the possible current effect
        bulb.setEffect(EFFECT_NOT_DEFINED_NAME);

        if (bulb.setState(false))
        {
          DEBUG_PRINT(F("\nINFO: State changed to: "));
          DEBUG_PRINTLN(bulb.getState());
          cmd = CMD_STATE_CHANGED;
        }
      }
    }

    if (root.containsKey("color"))
    {
      // stops the possible current effect
      bulb.setEffect(EFFECT_NOT_DEFINED_NAME);

      uint8_t r = root["color"]["r"];
      uint8_t g = root["color"]["g"];
      uint8_t b = root["color"]["b"];

      if (bulb.setColor(r, g, b))
      {
        DEBUG_PRINT(F("\nINFO: Color changed to: "));
        DEBUG_PRINT(bulb.getColor().red);
        DEBUG_PRINT(F(", "));
        DEBUG_PRINT(bulb.getColor().green);
        DEBUG_PRINT(F(", "));
        DEBUG_PRINTLN(bulb.getColor().blue);
        cmd = CMD_STATE_CHANGED;
      }
    }

    if (root.containsKey("brightness"))
    {
      if (bulb.setBrightness(root["brightness"]))
      {
        DEBUG_PRINT(F("\nINFO: Brightness changed to: "));
        DEBUG_PRINTLN(bulb.getBrightness());
        cmd = CMD_STATE_CHANGED;
      }
    }

    if (root.containsKey("white_value"))
    {
      // stops the possible current effect
      bulb.setEffect(EFFECT_NOT_DEFINED_NAME);

      if (bulb.setWhite(root["white_value"]))
      {
        DEBUG_PRINT(F("\nINFO: White changed to: "));
        DEBUG_PRINTLN(bulb.getColor().white);
        cmd = CMD_STATE_CHANGED;
      }
    }

    if (root.containsKey("color_temp"))
    {
      // stops the possible current effect
      bulb.setEffect(EFFECT_NOT_DEFINED_NAME);

      if (bulb.setColorTemperature(root["color_temp"]))
      {
        DEBUG_PRINT(F("\nINFO: Color temperature changed to: "));
        DEBUG_PRINTLN(bulb.getColorTemperature());
        cmd = CMD_STATE_CHANGED;
      }
    }

    if (root.containsKey("effect"))
    {
      const char *effect = root["effect"];
      if (bulb.setEffect(effect))
      {
        DEBUG_PRINTLN(F("\nINFO: Effect started"));
        cmd = CMD_NOT_DEFINED;
      }
    }
  }
}

/*
  Function called to subscribe to a MQTT topic
*/
void subscribeToMQTT(char *p_topic)
{
  if (mqttClient.subscribe(p_topic))
  {
    DEBUG_PRINT(F("\nINFO: Sending the MQTT subscribe succeeded for topic: "));
    DEBUG_PRINTLN(p_topic);
  }
  else
  {
    DEBUG_PRINT(F("ERROR: Sending the MQTT subscribe failed for topic: "));
    DEBUG_PRINTLN(p_topic);
  }
}

/*
  Function called to publish to a MQTT topic with the given payload
*/
void publishToMQTT(char *p_topic, char *p_payload)
{
  if (mqttClient.publish(p_topic, p_payload, true))
  {
    DEBUG_PRINT(F("\nINFO: MQTT message published successfully, topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(", payload: "));
    DEBUG_PRINTLN(p_payload);
  }
  else
  {
    DEBUG_PRINTLN(F("\nERROR: MQTT message not published, either connection lost, or message too large. Topic: "));
    DEBUG_PRINT(p_topic);
    DEBUG_PRINT(F(" , payload: "));
    DEBUG_PRINTLN(p_payload);
    DEBUG_PRINTLN();
  }
}

/*
  Function called to connect/reconnect to the MQTT broker modded by Bob to use while instead of time limit
*/
void connectToMQTT()
{
  //**if (!mqttClient.connected())
  while (!mqttClient.connected())
  {
    //**if (lastMQTTConnection + MQTT_CONNECTION_TIMEOUT < millis())
    //**{
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD))
    // if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, MQTT_STATUS_TOPIC, 0, 1, "dead"))
    {
      DEBUG_PRINTLN(F("\nINFO: The client is successfully connected to the MQTT broker"));
      publishToMQTT(MQTT_STATUS_TOPIC, "alive");

      /*
      // ** Function - Configure a light as a device
     void SendCfg_Light1()
     {
       DynamicJsonDocument configPayload(1024);
       const string LIGHT_ID{"Step_1"};

       configPayload["stat_t"] = "ha/light/step_1/state";
       configPayload["cmd_t"] = "ha/light/step_1/cmd";
       configPayload["expire_after"] = EXPIRE_AFTER;
       configPayload["dev"]["name"] = "Step lights";
       configPayload["dev"]["mdl"] = "IOTBear-4"; // [model]
       configPayload["dev"]["mf"] = "GlebeTech";  // [manuafucturer]

       configPayload["name"] = "Step light 1";
       configPayload["uniq_id"] = "SL1"; // add a bit of MAC
       configPayload["device_class"] = "light";
       // configPayload["schema"] = "json";
       // configPayload["brightness"] = "true";

       JsonArray identifiers{configPayload["dev"].createNestedArray("ids")};
       // identifiers.add(IPmsg);
       // identifiers.add(sMAC);
       identifiers.add("test_light");

       const string configTPC{"homeassistant/light/" + LIGHT_ID  + "/config"};
       // const string configTPC{"homeassistant/light/steplight/config"};
       char configPL[1024]{}; // array
       bool PubConfigOK{0};

       serializeJson(configPayload, configPL);

       if (mqttClient.publish(configTPC.c_str(), configPL, true))
         PubConfigOK = 1;
       const string publishMessage{
           "\nconfig " + LIGHT_ID + (PubConfigOK ? " " : " NOT ") + "published\n"};
       Serial.println(publishMessage.c_str());

        Serial.println(configTPC.c_str());
        Serial.println(configPL);
        Serial << "\n\n";

     } // End of config light 1
     */

#if defined(MQTT_HOME_ASSISTANT_SUPPORT)
      // MQTT discovery for Home Assistant
      //**JsonObject &root = staticJsonBuffer.createObject();
      //DynamicJsonDocument root(1024);
      root["name"] = FRIENDLY_NAME;
      root["platform"] = "mqtt_json";
      root["state_topic"] = MQTT_STATE_TOPIC;
      root["command_topic"] = MQTT_COMMAND_TOPIC;
      root["dev"]["name"] = "Step lights 2";
      root["dev"]["mdl"] = "Wemos Mini 9"; // [model]
      root["dev"]["mf"] = "GlebeTech";     // [manuafucturer]
      root["dev"]["ids"] = "abcd1234";     // [identifier]

      root["uniq_id"] = "SL1234";          // add a bit of MAC
      root["device_class"] = "light";
      root["schema"] = "json";
      root["brightness"] = true;
      root["rgb"] = true;
      root["white_value"] = true;
      root["color_temp"] = true;
      root["effect"] = true;
      root["effect_list"] = EFFECT_LIST;

      // identifiers.add("test_light");
      //-------------------------------------

      //**root.printTo(jsonBuffer, sizeof(jsonBuffer));

      // char configPL[1024]{}; // array

      serializeJson(root, jsonBuffer);

      publishToMQTT(MQTT_CONFIG_TOPIC, jsonBuffer);
#endif

      subscribeToMQTT(MQTT_COMMAND_TOPIC);
    }
    else
    {
      DEBUG_PRINTLN(F("ERROR: The connection to the MQTT broker failed"));
      DEBUG_PRINT(F("\nINFO: MQTT username: "));
      DEBUG_PRINTLN(MQTT_USERNAME);
      DEBUG_PRINT(F("\nINFO: MQTT password: "));
      DEBUG_PRINTLN(MQTT_PASSWORD);
      DEBUG_PRINT(F("\nINFO: MQTT broker: "));
      DEBUG_PRINTLN(MQTT_SERVER);
    }
    //**lastMQTTConnection = millis();
    //**} // End of 'if'
  } // End of 'while'
} // End of MQTT connect

///////////////////////////////////////////////////////////////////////////
//  CMD
///////////////////////////////////////////////////////////////////////////

void handleCMD()
{
  switch (cmd)
  {
  case CMD_NOT_DEFINED:
    break;
  case CMD_STATE_CHANGED:
    cmd = CMD_NOT_DEFINED;

    //**DynamicJsonBuffer dynamicJsonBuffer;
    DynamicJsonDocument root(1024);
    //**JsonObject &root = dynamicJsonBuffer.createObject();

    root["state"] = bulb.getState() ? MQTT_STATE_ON_PAYLOAD : MQTT_STATE_OFF_PAYLOAD;
    root["brightness"] = bulb.getBrightness();

    //**********************
    //**JsonObject &color = root.createNestedObject("color");
    root["color"]["r"] = bulb.getColor().red;
    root["color"]["g"] = bulb.getColor().green;
    root["color"]["b"] = bulb.getColor().blue;
    root["white_value"] = bulb.getColor().white;
    root["color_temp"] = bulb.getColorTemperature();

    serializeJson(root, jsonBuffer);

    //**root.printTo(jsonBuffer, sizeof(jsonBuffer));
    publishToMQTT(MQTT_STATE_TOPIC, jsonBuffer);
    break;
  }
}

///////////////////////////////////////////////////////////////////////////
//  SETUP() AND LOOP()
///////////////////////////////////////////////////////////////////////////

void setup()
{
#if defined(DEBUG_SERIAL)
  Serial.begin(115200);
#elif defined(DEBUG_TELNET)
  telnetServer.begin();
  telnetServer.setNoDelay(true);
#endif

  setupWiFi();

  sprintf(MQTT_CLIENT_ID, "%06X", ESP.getChipId());
#if defined(MQTT_HOME_ASSISTANT_SUPPORT)
  sprintf(MQTT_CONFIG_TOPIC, MQTT_CONFIG_TOPIC_TEMPLATE, MQTT_HOME_ASSISTANT_DISCOVERY_PREFIX, MQTT_CLIENT_ID);
  DEBUG_PRINT(F("\nINFO: MQTT config topic: "));
  DEBUG_PRINTLN(MQTT_CONFIG_TOPIC);
#else

#endif

  sprintf(MQTT_STATE_TOPIC, MQTT_STATE_TOPIC_TEMPLATE, MQTT_CLIENT_ID);
  sprintf(MQTT_COMMAND_TOPIC, MQTT_COMMAND_TOPIC_TEMPLATE, MQTT_CLIENT_ID);
  sprintf(MQTT_STATUS_TOPIC, MQTT_STATUS_TOPIC_TEMPLATE, MQTT_CLIENT_ID);

  DEBUG_PRINT(F("\nINFO: MQTT state topic: "));
  DEBUG_PRINTLN(MQTT_STATE_TOPIC);
  DEBUG_PRINT(F("\nINFO: MQTT command topic: "));
  DEBUG_PRINTLN(MQTT_COMMAND_TOPIC);
  DEBUG_PRINT(F("\nINFO: MQTT status topic: "));
  DEBUG_PRINTLN(MQTT_STATUS_TOPIC);

  mqttClient.setBufferSize(512); //** Added by Bob
  mqttClient.setServer(MQTT_SERVER, MQTT_SERVER_PORT);
  mqttClient.setCallback(handleMQTTMessage);

  connectToMQTT();

#if defined(OTA)
  setupOTA();
#endif

  bulb.init();

  cmd = CMD_STATE_CHANGED;
}

void loop()
{
#if defined(DEBUG_TELNET)
  // handle the Telnet connection
  handleTelnet();
#endif

  yield();

#if defined(OTA)
  handleOTA();
#endif

  yield();

  connectToMQTT();
  mqttClient.loop();

  yield();

  handleCMD(); // Called every loop but only actioned if a new command is received.

  yield();

  bulb.loop();
}