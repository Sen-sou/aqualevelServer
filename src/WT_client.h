#pragma once
#include <WT_utils.h>

struct WTclient {
    WiFiClient client;
    String ID{""};
    String request{""};
    String command{""};
    String data{""};
    bool loadLog {false};
    int response{0};
    unsigned long sessionStartTime{0};

    // Access variables
    long lastReadByte {0};
    short logCriticalSize {10};
    bool allowLog {false};  
    byte dataGetInterval{1};
};

namespace levelClient {

    byte sensorReadSpeed {0};

    enum RequestCommand {
        IDLE,
        SETCONFIGURATION,
        GETCONFIGURATION,
        STOP
    };

    bool checkRequest(WTclient &client);
    bool checkAbruptDisconnect(WTclient &client);
    void getResponse(WTclient &client);
    void getResponseCode(WTclient &client);
    bool setConfig(WTclient &client);
    int getConfig(WTclient &client);

    unsigned int maxRequestLength {100};

    bool checkRequest(WTclient &client) {
        if (client.client.available()) { // may get RTC from active client
            client.request = client.client.readStringUntil('\n').substring(0, maxRequestLength);
            // Serial.println(client.ID + " Got: " + client.request);
            WT_utility::debugLog(client.ID + " Got: " + client.request);
            return true;
        } else {
            if (client.response != RequestCommand::STOP) client.response = RequestCommand::IDLE;
            return false;
        }
    }

    bool checkAbruptDisconnect(WTclient& client) {
        if (!client.client.connected()
         && (!client.ID.isEmpty() || !client.request.isEmpty() || !client.data.isEmpty() || client.response)) return true;
            return false;
    }

    void getResponse(WTclient &client) {
        client.command = client.request.substring(client.request.indexOf("<") + 1, client.request.indexOf("/"));
        client.data = client.request.substring(client.request.indexOf("/") + 1, client.request.indexOf(">"));
    }
    
    void getResponseCode(WTclient& client) {
        if (client.command.equalsIgnoreCase("setConfig")) {
            client.response = RequestCommand::SETCONFIGURATION;
        } else if (client.command.equalsIgnoreCase("getConfig")){
            client.response = RequestCommand::GETCONFIGURATION;
        } else if (client.command.equalsIgnoreCase("stop")) {
            client.response = RequestCommand::STOP;
        } else client.response = RequestCommand::IDLE;
    }

    bool setConfig(WTclient &client) {
        String var {""}, val{""};
        int indDel {0};
        bool res {false};
        while (!client.data.isEmpty())
        {
            // Serial.println(client.ID + " Processed: " + client.data);
            indDel = client.data.indexOf(":");
            var = client.data.substring(0, indDel);
            client.data = client.data.substring(indDel + 1);
            indDel = client.data.indexOf(":");
            if (indDel == -1) {
                val = client.data;
                client.data = "";
            }
            else {
                val = client.data.substring(0, indDel);
                client.data = client.data.substring(indDel + 1);
            }
            
            if (var.equalsIgnoreCase("dataGetInterval")) 
                client.dataGetInterval = val.toInt();
            else if (var.equalsIgnoreCase("allowLog"))
                client.allowLog = val.toInt();
            else if (var.equalsIgnoreCase("logCriticalSize"))
                client.logCriticalSize = val.toInt();
            else if (var.equalsIgnoreCase("lastReadByte"))
                client.lastReadByte = val.toInt();
            else if (var.equalsIgnoreCase("loadLog")) {
                client.loadLog = val.toInt();
                res = client.loadLog;
            }
        }
        client.sessionStartTime = millis();
        return res;
    }

    int getConfig(WTclient &client) {
        int res {0};
        int indDel = client.data.indexOf(":");
        String var = client.data.substring(0, indDel);
        String val = client.data.substring(indDel + 1);

        if (var.equalsIgnoreCase("dataGetInterval")) 
            res = client.dataGetInterval;
        else if (var.equalsIgnoreCase("allowLog"))
            res = client.allowLog;
        else if (var.equalsIgnoreCase("logCriticalSize"))
            res = client.logCriticalSize;
        else if (var.equalsIgnoreCase("lastReadByte"))
            res = client.lastReadByte;

        return res;
    }

    void reset(WTclient& client) {
        client.ID = "";
        client.request = "";
        client.command = "";
        client.data = "";
        client.response = 0;
        client.lastReadByte = 0;
        client.loadLog = false;
        client.allowLog = false;
        client.logCriticalSize = 10;
        client.dataGetInterval = 1;
        client.sessionStartTime = 0;
    }

}
