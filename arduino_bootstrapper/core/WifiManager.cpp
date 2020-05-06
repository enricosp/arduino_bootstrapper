/*
  WifiManager.cpp - Managing WiFi and OTA
  
  Copyright (C) 2020  Davide Perini
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of 
  this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
  copies of the Software, and to permit persons to whom the Software is 
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in 
  all copies or substantial portions of the Software.
  
  You should have received a copy of the MIT License along with this program.  
  If not, see <https://opensource.org/licenses/MIT/>.
*/

#include "WifiManager.h"


//Establishing Local server at port 80 whenever required
ESP8266WebServer server(80);
// WiFiClient
WiFiClient espClient;
// WebServer content
String content;
// WebServer status code
int statusCode;
// WebServer HTML frontend
String htmlString;


/********************************** SETUP WIFI *****************************************/
void WifiManager::setupWiFi(void (*manageDisconnections)(), void (*manageHardwareButton)()) {
  
  wifiReconnectAttemp = 0;

  // DPsoftware domotics 
  if (PRINT_TO_DISPLAY) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(5,17);
    display.drawRoundRect(0, 0, display.width()-1, display.height()-1, display.height()/4, WHITE);    
  }
  helper.smartPrintln(F("DPsoftware domotics"));
  helper.smartDisplay();
  delay(DELAY_3000);

  if (PRINT_TO_DISPLAY) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
  }
  helper.smartPrintln(F("Connecting to: "));
  helper.smartPrint(SSID); helper.smartPrintln(F("..."));
  helper.smartDisplay();
 
  delay(DELAY_2000);

  WiFi.persistent(false);   // Solve possible wifi init errors (re-add at 6.2.1.16 #4044, #4083)
  WiFi.disconnect(true);    // Delete SDK wifi config
  delay(DELAY_200);

  WiFi.mode(WIFI_STA);      // Disable AP mode
  //WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoConnect(true);  
  WiFi.config(IP_MICROCONTROLLER, IP_DNS, IP_GATEWAY);
  WiFi.hostname(WIFI_DEVICE_NAME);

  // Set wifi power in dbm range 0/0.25, set to 0 to reduce PIR false positive due to wifi power, 0 low, 20.5 max.
  WiFi.setOutputPower(WIFI_POWER);
  
  // Start wifi connection
  WiFi.begin(SSID, PASSWORD);

  // loop here until connection
  while (WiFi.status() != WL_CONNECTED) {

    manageHardwareButton();   

    delay(DELAY_500);
    Serial.print(F("."));
    wifiReconnectAttemp++;
    if (wifiReconnectAttemp > 10) {
      // if fastDisconnectionManagement we need to execute the callback immediately, 
      // example: power off a watering system can't wait MAX_RECONNECT attemps
      if (fastDisconnectionManagement) {
        manageDisconnections();
      }
      if (PRINT_TO_DISPLAY) {
        display.setCursor(0,0);
        display.clearDisplay();
      }
      helper.smartPrint(F("Wifi attemps= "));
      helper.smartPrint(wifiReconnectAttemp);
      if (wifiReconnectAttemp >= MAX_RECONNECT) {
        helper.smartPrintln(F("Max retry reached, powering off peripherals."));
        manageDisconnections();
      }
      helper.smartDisplay();
    } else if (wifiReconnectAttemp > 10000) {
      wifiReconnectAttemp = 0;
    }

  }

  helper.smartPrintln(F("WIFI CONNECTED"));
  IP = WiFi.localIP().toString();
  MAC = WiFi.macAddress();
  helper.smartPrintln(IP);

  delay(DELAY_1500);

  // reset the lastWIFiConnection to off, will be initialized by next time update
  lastWIFiConnection = OFF_CMD;

}

/********************************** SETUP OTA *****************************************/
void WifiManager::setupOTAUpload() {

  //OTA SETUP
  ArduinoOTA.setPort(OTA_PORT);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(WIFI_DEVICE_NAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)OTAPASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println(F("Starting"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  ArduinoOTA.begin();

}

/*
   Return the quality (Received Signal Strength Indicator) of the WiFi network.
   Returns a number between 0 and 100 if WiFi is connected.
   Returns -1 if WiFi is disconnected.
*/
int WifiManager::getQuality() {
  if (WiFi.status() != WL_CONNECTED) {
    return -1;
  }    
  int dBm = WiFi.RSSI();
  if (dBm <= -100) {
    return 0;
  }    
  if (dBm >= -50) {
    return 100;
  }    
  return 2 * (dBm + 100);
}

// check if wifi is correctly configured
bool WifiManager::isWifiConfigured() {

  if (strcmp(SSID, "XXX") != 0) {
    return true;
  }
  return false;

}

// if no ssid available, launch web server to get config params via browser
void WifiManager::launchWebServerForOTAConfig() {
  
  WiFi.disconnect();
  Serial.println("Turning the HotSpot On");
  launchWeb();
  setupAP();
  
  while ((WiFi.status() != WL_CONNECTED)) {
    Serial.print(".");
    delay(100);
    server.handleClient();
  }
  
}

void WifiManager::launchWeb() {

  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
  }    
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  server.begin();
  Serial.println("Server started");

}

void WifiManager::setupAP(void) {

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(DELAY_200);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  htmlString = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    htmlString += "<li>";
    htmlString += WiFi.SSID(i);
    htmlString += " (";
    htmlString += WiFi.RSSI(i);

    htmlString += ")";
    htmlString += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    htmlString += "</li>";
  }
  htmlString += "</ol>";
  delay(100);
  WiFi.softAP("ArduinoStar", "");
  launchWeb();

}

void WifiManager::createWebServer() {
  {
    server.on("/", []() {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
      content += ipStr;
      content += "<p>";
      content += htmlString;
      content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=235><input name='OTApass' length=64><input name='mqttuser' length=64><input name='mqttpass' length=64><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });

    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      String OTApass = server.arg("OTApass");
      String mqttuser = server.arg("mqttuser");
      String mqttpass = server.arg("mqttpass");

      if (qsid.length() > 0 && qpass.length() > 0 && OTApass.length() > 0 && mqttuser.length() > 0 && mqttpass.length() > 0) {
        
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");
        Serial.println(OTApass);
        Serial.println("");
        Serial.println(mqttuser);
        Serial.println("");
        Serial.println(mqttpass);
        Serial.println("");

        // writeConfigToSpiffs(qsid, qpass, OTApass, mqttuser, mqttpass);    
        DynamicJsonDocument doc(1024);
        doc["qsid"] = qsid;     
        doc["qpass"] = qpass;     
        doc["OTApass"] = OTApass;     
        doc["mqttuser"] = mqttuser;     
        doc["mqttpass"] = mqttpass;  

        // Write SPIFFS
        if (SPIFFS.begin()) {
          Serial.println(F("\nSaving setup.json\n"));
          // SPIFFS.format();
          File configFile = SPIFFS.open("/setup.json", "w");
          if (!configFile) {
            Serial.println(F("Failed to open config file for writing"));
          }
          serializeJsonPretty(doc, Serial);
          serializeJson(doc, configFile);
          configFile.close();
          Serial.println(F("\nConfig saved\n"));
        } else {
          Serial.println(F("Failed to mount FS for write"));
        }
        
        delay(DELAY_200);
        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        ESP.reset();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);
    });

  } 
}
