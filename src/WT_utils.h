#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <string>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <TaskScheduler.h>
#include <WT_sensor.h>
#include <WT_fileHandler.h>

// updating properties.txt need update PROPERTY
namespace PROPERTY {
  const byte TOTAL_PROPERTIES = 3 + (9 * 2);
  enum GLOBAL : byte {
    RTC = 2,
    uptime = 4
  };
  enum SERVER : byte {
    ip = 7,
    mac = 9,
    ssid = 11,
    password = 13,
    maxClient = 15
  };
  enum SENSOR : byte {
    accuracy = 18,
    logData = 20,
  };

  String BACKUP_PROPERTIES_FILE = "@global\r\n-RTC=#\r\n-uptime=#\r\n"
                                  "@server\r\n-ip=#\r\n-mac=#\r\n-ssid=Vishwakarma4 Wifi\r\n-password=dlink355168\r\n"
                                  "-maxClient=5\r\n"
                                  "@sensor\r\n-accuracy=1\r\n-logData=0\r\n";

}

namespace WT_utility {

  const String logFilePath = "DebugLog.txt";
  const float SENSOR_HEIGHT_OFFSET {8.0f};
  const float TANK_HEIGHT {93.0f};
  const float NET_TANK_HEIGHT {TANK_HEIGHT - SENSOR_HEIGHT_OFFSET};
  float PERCENTAGE_CONSTANT {(100.0f * 10.0f) / NET_TANK_HEIGHT};

  // Property Utility Functions
  void extractProperties();
  template <typename T> void setPropertyValue(const byte propertyKey, T value);
  const char* getPropertyValue(const byte propertyKey);
  void loadBackupProperties();
  void reconstructProperties();
  void updatePropertyFile();

  // Sensor Utility Functions
  void loadSensorPropeties();
  void setSensorLogFormat(uint16_t data);
  void logSensorData();
  bool getSensorLogData(long& startPos);
  
  // Log Functions
  void debugLog(const String& log);

  // Time and Task Functions
  void updateRTC();
  unsigned long getTimeElapsed(unsigned long initialTime);
  void setTasks();

  void setup();

  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 300000);
  tmElements_t localTime;

  Scheduler scheduler;
  Task task[10]; // previously 7
  enum TASKS : byte {
    UPDATE_RTC,
    UPDATE_PROPERTIES,
    LOG_SENSOR_DATA,
    WIFI_RECONNECT_START,
    WIFI_RECONNECT_STOP,
    GET_SENSOR_READ,
    GET_SENSOR_LOG_DATA
  };

  char properties[PROPERTY::TOTAL_PROPERTIES][30];
  String propFile;

  String sensorLogFormat; // 23 bytes
  String sensorLogData;

  void setup() {
    DebugCode::setup();
    WT_fileHandler::setup();

    propFile = WT_fileHandler::getFileContent("properties.txt");
    if (propFile.isEmpty()) loadBackupProperties();
    extractProperties();

    WT_ultraSonic::setup();
    loadSensorPropeties();

    timeClient.begin();
    setTasks();
    scheduler.startNow();
  }

  // Property Utility Functions
  void extractProperties() {
    char extract;
    byte outerIndex {0};
    byte innerIndex {0};
    for (uint16 i = 0; i < propFile.length(); i++)
    {
      extract = propFile.charAt(i);
      switch (extract)
      {
      case '\r':
        i++;
      case '=':
        outerIndex++;
        innerIndex = 0;
        break;
      case '-':
        break;
      default:
        properties[outerIndex][innerIndex] = extract;
        innerIndex++;
        break;
      }
    }
  }

  template <typename T>
  void setPropertyValue(const byte propertyKey, T value) {
    strcpy(properties[propertyKey], String(value).c_str());
  }

  const char* getPropertyValue(const byte propertyKey) {
    return properties[propertyKey];
  }

  void loadBackupProperties() {
    // Serial.println("Could not find \"properties.txt\"");
    // Serial.println("Recreating the properties file....");
    File file = SD.open("properties.txt", FILE_WRITE);
    file.print(PROPERTY::BACKUP_PROPERTIES_FILE);
    file.close();
    propFile = PROPERTY::BACKUP_PROPERTIES_FILE;
    // Serial.println("properties.txt:\r\n"+propFile);
    // Serial.println("Loading from Backup");
    debugLog("Could not create Property File. Loading From Backup");
  }

  void reconstructProperties() {
    propFile.clear();
    //bool flagKey {true};
    for (byte i = 0; i < PROPERTY::TOTAL_PROPERTIES; i++)
    {
      if (properties[i][0] == '@') {
          propFile = propFile + properties[i] + "\r\n";
        } else {
          propFile = propFile + "-" + properties[i] + "=" + properties[i+1] + "\r\n";
          i++;
        }
    }
  }
 
  void updatePropertyFile() {
    // Serial.println("============UpdateProperty Ran at: " + String(millis()));
    setPropertyValue<uint32_t>(PROPERTY::GLOBAL::uptime, millis());
    reconstructProperties();
    WT_fileHandler::setFileContent(propFile, "properties.txt");
    task[TASKS::LOG_SENSOR_DATA].enableDelayed(200);
    // Serial.println("============UpdateProperty Finished at: " + String(millis()));
    debugLog("Property File Updated");
  }


  // Sensor Utility Functions
  void loadSensorPropeties() {
    WT_ultraSonic::accuracy = std::stoi(getPropertyValue(PROPERTY::SENSOR::accuracy));
    WT_ultraSonic::logData = std::stoi(getPropertyValue(PROPERTY::SENSOR::logData));
    sensorLogFormat.reserve(24);
    sensorLogData.reserve(MAX_RESULT_SIZE);
  }

  void setSensorLogFormat(uint16_t data) {
    char format[24];
    if (localTime.Minute < 100 && localTime.Hour < 100 && localTime.Day < 100 
    && localTime.Month < 100 && localTime.Year+1970 < 10000 && data < 10000) // MAX value of each data
      snprintf(format, sizeof(format), "%02d:%02d:%02d:%02d:%04d:%04d\r\n",
        localTime.Minute, localTime.Hour, localTime.Day, localTime.Month, localTime.Year+1970, data);
    sensorLogFormat = String(format);
  }

  void logSensorData() { // can use client to get to set timeclient
    // Serial.println("====================logSensorData Ran at: " + String(millis()));
    if (!timeClient.isTimeSet()) { // Handle if cant connect to the time client 
      debugLog("Log Sensor Data didnt ran because TimeClient was not set");
      task[TASKS::LOG_SENSOR_DATA].disable();                
      ESP.restart();
      return;
    }
    if (!SD.exists("SensorDataLog.txt")) {
      File file = SD.open("SensorDataLog.txt", FILE_WRITE);
      file.print("mm:hh:dD:MM:YYYY:0000\r\n");
      file.close();
      debugLog("SensorDataLog file created");
    }
    WT_ultraSonic::getAvgRead();
    if (WT_ultraSonic::echoRead < 0 && WT_ultraSonic::echoRead > TANK_HEIGHT) setSensorLogFormat(9999);
    else setSensorLogFormat((TANK_HEIGHT - WT_ultraSonic::echoRead) * PERCENTAGE_CONSTANT);
    WT_fileHandler::setFileContent(sensorLogFormat, "SensorDataLog.txt", false);
    // Serial.println("Logged Sensor Data: " + sensorLogFormat);
    debugLog("Logged Sensor Data: " + sensorLogFormat);
    task[TASKS::LOG_SENSOR_DATA].disable();
    // Serial.println("====================logSensorData Finished at: " + String(millis()));
  }

  bool getSensorLogData(long& startPos) {
    //Serial.println("====================getlogSensorData Ran at: " + String(millis()));
    sensorLogData = WT_fileHandler::getFileContent("SensorDataLog.txt", startPos);
    if (WT_fileHandler::lastReadByte == -1) {
      startPos += sensorLogData.length();
      //Serial.println("====================getlogSensorData Ran at: " + String(millis()));
      return false;
    }
    startPos = WT_fileHandler::lastReadByte;
    //Serial.println("====================getlogSensorData Ran at: " + String(millis()));
    return true;
  }

  // Debug Log Function

  void debugLog(const String& log) {
    if (!SD.exists(logFilePath)) {
      File file = SD.open(logFilePath, FILE_WRITE);
      file.close();
    }
    String logDebug {""};
    if(!timeClient.isTimeSet()) 
      logDebug = "*Time Client not Set* => " + log + "\r\n";
    else logDebug = String(localTime.Day) + ":" + 
               String(localTime.Month + 1) + ":" +
               String(localTime.Year + 1970) + ":" + 
               String(localTime.Hour) + ":" + 
               String(localTime.Minute) + " => " + log + "\r\n";
    WT_fileHandler::setFileContent(logDebug, logFilePath.c_str(), false);
  }

  // Time and Task Functions
  static bool WifiStatus = true;
  void updateRTC() {
    if (WiFi.status() != WL_CONNECTED && WifiStatus) {
      Serial.println("!! Wifi Disconnected !!");
      debugLog("!! Wifi Disconnected !!");
      WifiStatus = !WifiStatus;
    } else if (WiFi.status() == WL_CONNECTED && !WifiStatus) {
      Serial.println("!! Wifi Reconnected !!");
      debugLog("!! Wifi Reconnected !!");
      WifiStatus = !WifiStatus;
    }
    if (!timeClient.isTimeSet()) {
      DebugCode::sensorError();
      return;
    }
    time_t currentTime = timeClient.getEpochTime();
    breakTime(currentTime, localTime);
    // Serial.println(currentTime);
  }

  unsigned long getTimeElapsed(unsigned long initialTime) {
    unsigned long currentTime = millis();
    return (currentTime >= initialTime) ? currentTime - initialTime : currentTime + (0xFFFFFFFF - initialTime);
  }


  void setTasks() {
    task[TASKS::UPDATE_RTC].set(TASK_SECOND, TASK_FOREVER, updateRTC);
    scheduler.addTask(task[TASKS::UPDATE_RTC]);
    task[TASKS::UPDATE_RTC].enable();

    task[TASKS::UPDATE_PROPERTIES].set(TASK_MINUTE, TASK_FOREVER, updatePropertyFile);
    scheduler.addTask(task[TASKS::UPDATE_PROPERTIES]);
    task[TASKS::UPDATE_PROPERTIES].enable();

    if (WT_ultraSonic::logData) { // Run once after updatePropertyFile
      task[TASKS::LOG_SENSOR_DATA].set(1, TASK_FOREVER, logSensorData);
      scheduler.addTask(task[TASKS::LOG_SENSOR_DATA]);
    }

  }

}