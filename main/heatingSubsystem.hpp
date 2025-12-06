#ifndef HEATINGSUBSYSTEM_HPP
#define HEATINGSUBSYSTEM_HPP

#include <ArduinoJson.h>
#include <PubSubClient.h>

extern bool is_system_active;

void setupHeating();
void executeHeating();
void getHeatingStatus(JsonObject& doc);
void handleHeatingAttributes(JsonObject& doc);
void handleHeatingCommand(PubSubClient& client, char* topic, byte* payload, unsigned int length);

#endif

