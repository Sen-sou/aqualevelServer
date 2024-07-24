#include <Arduino.h>
#include <WT_server.h>

void setup() {
  Serial.begin(115200);
  WT_utility::setup();
  levelServer::setup();
}

void loop() {
  WT_utility::timeClient.update();
  WT_utility::scheduler.execute();
  levelServer::checkClient();
  levelServer::handleClient();
  levelServer::handleRequest();
}
