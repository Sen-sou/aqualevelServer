#pragma once
#include <WT_client.h>

namespace levelServer {

    void checkClient();
    void handleRequest();
    void handleClient();
    void sendSensorRead();
    void sendSensorLogData();
    void setupTasks();
    void setup();

    WiFiServer server(80);

    int statusDelay {0};
    byte maxClient;
    byte connectedClients {0};
    WTclient* clients;
    bool sendlogFlag {false};
    
    void setup() {
        // configure Wifi restart task 
        // client handle has low priority than get log sensor data
        // Initialize variables
        const char* ssid {WT_utility::getPropertyValue(PROPERTY::SERVER::ssid)};
        const char* password {WT_utility::getPropertyValue(PROPERTY::SERVER::password)};
        maxClient = std::stoi(WT_utility::getPropertyValue(PROPERTY::SERVER::maxClient));
        clients = new WTclient[maxClient];

        // Connect to Wi-Fi
        WiFi.begin(ssid, password);
        byte tries{10};
        while (WiFi.status() != WL_CONNECTED && tries) {
            // Serial.println("Connecting to WiFi...");
            WT_utility::debugLog("Connecting to WiFi...");
            DebugCode::wifiNotConnected();
            tries--;
        }
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
        
        // Serial.println("Connected to WiFi");
        WT_utility::debugLog("Connected to WiFi");
        WT_utility::setPropertyValue<String>(PROPERTY::SERVER::ip, WiFi.localIP().toString());
        WT_utility::setPropertyValue<String>(PROPERTY::SERVER::mac, WiFi.macAddress());
        // Serial.println(WiFi.localIP());
        // Serial.println("MAC Address: " + WiFi.macAddress());
        WT_utility::debugLog(WiFi.localIP().toString());
        WT_utility::debugLog("MAC Address: " + WiFi.macAddress());
        server.begin();   
        WT_utility::updatePropertyFile();
        setupTasks();
    }

    void checkClient() {
        for (byte i = 0; i < maxClient; i++)
        {
            // Handle Abrupt Client Disconnects
            if (levelClient::checkAbruptDisconnect(clients[i])){
                Serial.println("Client: " + clients[i].ID + " disconnected!");
                WT_utility::debugLog("Client: " + clients[i].ID + " disconnected!");
                levelClient::reset(clients[i]);
                connectedClients--;
                DebugCode::clientDisconnected();
            }
            // Check and Register new Clients
            if (!clients[i].client || !clients[i].client.connected()) { // check if server full -> relay server full message
                clients[i].client = server.accept();
                if (clients[i].client) {
                    clients[i].ID = clients[i].client.remoteIP().toString();
                    clients[i].sessionStartTime = millis();
                    Serial.println("================================================================");
                    Serial.println("New Client: " + clients[i].ID + " Connected");
                    WT_utility::debugLog("================================================================");
                    WT_utility::debugLog("New Client: " + clients[i].ID + " Connected");
                    connectedClients++;
                    Serial.print("Number of Active Clients: ");
                    Serial.println(connectedClients);
                    WT_utility::debugLog("Number of Active Clients: " + String(connectedClients));
                    Serial.println("================================================================");
                    WT_utility::debugLog("================================================================");
                    DebugCode::clientConnected();
                    
                }
            }
        }
    }

    void handleRequest() {
        for (byte i = 0; i < maxClient; i++)
        {
            if (!clients[i].client.connected()) continue;
            switch(clients[i].response) {
                case levelClient::RequestCommand::IDLE:
                    break;
                case levelClient::RequestCommand::SETCONFIGURATION:
                    sendlogFlag = levelClient::setConfig(clients[i]);
                    // Serial.println("SendlogFlag: " + String(sendlogFlag));
                    return;
                case levelClient::RequestCommand::GETCONFIGURATION:
                    clients[i].client.println("<" + clients[i].command + "-Rsp/"
                                     + String(levelClient::getConfig(clients[i]) + ">"));
                    return;
                case levelClient::RequestCommand::STOP:
                    clients[i].client.println("<stop/1>");
                    clients[i].client.abort();
                    Serial.println("Client: " + clients[i].ID + " disconnected");
                    WT_utility::debugLog("Client: " + clients[i].ID + " disconnected");
                    levelClient::reset(clients[i]);
                    connectedClients--;
                    DebugCode::clientDisconnected();
                    return;
                default:
                    clients[i].client.print("Invalid Request: ");
                    clients[i].client.println(clients[i].request);
                    continue;
            }
            if (!clients[i].loadLog && WT_utility::getTimeElapsed(clients[i].sessionStartTime) > 600000) {
                clients[i].response = levelClient::RequestCommand::STOP;
            }
        }
    }

    void handleClient() {
        for (byte i = 0; i < maxClient; i++)
        {
            if (clients[i].client.connected()) {
                if (levelClient::checkRequest(clients[i])) {
                    levelClient::getResponse(clients[i]);
                    levelClient::getResponseCode(clients[i]);
                }
                if (clients[i].allowLog && ((WT_fileHandler::sensorLogSize - clients[i].lastReadByte) > (23 * clients[i].logCriticalSize))) {
                    sendlogFlag = true;
                    clients[i].loadLog = true;
                }
            }
        }
        if (sendlogFlag) {
            WT_utility::task[WT_utility::TASKS::GET_SENSOR_LOG_DATA].enableIfNot();
            // Serial.println("SendLogFlag was true");
            sendlogFlag = false;
        }
    }

    // TASK FUNCTIONS

    void sendSensorLogData() {
        Serial.println("This ran " + String(WT_utility::task[WT_utility::TASKS::GET_SENSOR_LOG_DATA].getRunCounter()) + " times");
        long sec = (millis() / 1000) % 60;
        if (sec >= 0 && sec <= 2) return; // Skip Minute First Second
        bool loadCheck {false};
        for (byte i = 0; i < maxClient; i++)
        {
            if (!clients[i].client.connected() || !clients[i].loadLog || !connectedClients)
                continue;
            if (!WT_utility::getSensorLogData(clients[i].lastReadByte)) { // Check if Got all data
                clients[i].client.print("<historySync/" + String(WT_fileHandler::sensorLogSize) 
                                        + "/" + String(clients[i].lastReadByte) 
                                        + "/" + WT_utility::sensorLogData + ">\n");
                clients[i].loadLog = false;
            } else {
                clients[i].client.print("<historySync/" + String(WT_fileHandler::sensorLogSize) 
                                        + "/" + String(clients[i].lastReadByte) 
                                        + "/" + WT_utility::sensorLogData + ">\n");
                loadCheck = true;
            }    
        }
        if (!loadCheck) WT_utility::task[WT_utility::TASKS::GET_SENSOR_LOG_DATA].disable();
    }

    int sensorTime {0};
    void sendSensorRead() {
        levelClient::sensorReadSpeed = 0;
        for (byte i = 0; i < maxClient; i++)
        {
            if (!clients[i].client.connected() || !connectedClients) continue;
            if (clients[i].dataGetInterval == 1) 
                levelClient::sensorReadSpeed = 1;
            else if (clients[i].dataGetInterval == 5 && levelClient::sensorReadSpeed != 1) 
                levelClient::sensorReadSpeed = 5;
            else if (clients[i].dataGetInterval == 10 
                && (levelClient::sensorReadSpeed != 1 && levelClient::sensorReadSpeed != 5))
                levelClient::sensorReadSpeed = 10;
        }
        
        if (!levelClient::sensorReadSpeed || ((millis() / 1000) % levelClient::sensorReadSpeed != 0)) return;
        // sensorTime = millis();
        WT_ultraSonic::getAvgRead();
        // Serial.print("<<<<<<<<< Sensor Read Finished in: " + String(millis() - sensorTime));
        long sec {0};
        for (byte i = 0; i < maxClient; i++)
        {
            sec = (millis() / 1000) % clients[i].dataGetInterval;
            if (!clients[i].client.connected() || !connectedClients 
            || (sec > 1)) // might check for range  
                continue;
            // Serial.print(" = " + String(WT_ultraSonic::echoRead) + " >>>");
            WT_utility::debugLog("Sent Sensor Read = " + String(WT_ultraSonic::echoRead) + " >>> " + clients[i].ID);
            clients[i].client.print("<sensorRead/" + String(WT_ultraSonic::echoRead) + ">\n"); 
            // Serial.print(" Sent to " + clients[i].ID);
        }
        // Serial.println(" >>>>>>>>>");
    }

    void setupTasks() {
        // Send Sensor Read
        WT_utility::task[WT_utility::TASKS::GET_SENSOR_READ].set(TASK_SECOND, TASK_FOREVER, sendSensorRead);
        WT_utility::scheduler.addTask(WT_utility::task[WT_utility::TASKS::GET_SENSOR_READ]);
        WT_utility::task[WT_utility::TASKS::GET_SENSOR_READ].enable();

        // Send Sensor Data History TASK
        // May impact RTC update so updating every second and once might be better
        WT_utility::task[WT_utility::TASKS::GET_SENSOR_LOG_DATA].set(TASK_SECOND, TASK_FOREVER, sendSensorLogData); 
        WT_utility::scheduler.addTask(WT_utility::task[WT_utility::TASKS::GET_SENSOR_LOG_DATA]);
    }
}

