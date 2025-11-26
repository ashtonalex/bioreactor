#ifndef HEATINGSUBSYSTEM_HPP
#define HEATINGSUBSYSTEM_HPP

#include <ArduinoJson.h>
#include <PubSubClient.h>

void setupHeating();
void executeHeating();
void getHeatingStatus(JsonObject& doc);
void handleHeatingAttributes(JsonObject& doc);

#endif
