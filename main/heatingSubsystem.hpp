#ifndef HEATINGSUBSYSTEM_HPP
#define HEATINGSUBSYSTEM_HPP

#include <ArduinoJson.h>
#include <PubSubClient.h>

void setupHeating();
void executeHeating();
void getHeatingStatus(JsonObject& doc);
void handleHeatingCommand(PubSubClient& client, char* topic, byte* payload, unsigned int length);

#endif
