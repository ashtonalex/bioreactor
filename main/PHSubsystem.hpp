#ifndef PHSUBSYSTEM_HPP
#define PHSUBSYSTEM_HPP

// We need PubSubClient to be able to publish
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- Function Declarations ---

/**
 * @brief Sets up the pin modes for pumps and the pH sensor.
 */
void setupPH();

/**
 * @brief Main execution loop for the pH subsystem.
 * Handles reading sensors and autonomous control.
 */
void executePH();

/**
 * @brief Populates the passed JSON object with the current pH status.
 * @param doc The JsonObject to populate.
 */
void getPHStatus(JsonObject& doc);

/**
#ifndef PHSUBSYSTEM_HPP
#define PHSUBSYSTEM_HPP

// We need PubSubClient to be able to publish
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- Function Declarations ---

/**
 * @brief Sets up the pin modes for pumps and the pH sensor.
 */
void setupPH();

/**
 * @brief Main execution loop for the pH subsystem.
 * Handles reading sensors and autonomous control.
 */
void executePH();

/**
 * @brief Populates the passed JSON object with the current pH status.
 * @param doc The JsonObject to populate.
 */
void getPHStatus(JsonObject& doc);

/**
 * @brief Handles incoming MQTT messages (RPC commands).
 * @param client The PubSubClient instance.
 * @param topic The message topic.
 * @param payload The raw message payload.
 * @param length The length of the payload.
 */
void handlePHCommand(PubSubClient& client, char* topic, byte* payload, unsigned int length);

/**
 * @brief Handles incoming Shared Attribute updates.
 * @param doc The JsonObject containing the attributes.
 */
void handlePHAttributes(JsonObject& doc);

#endif // PHSUBSYSTEM_HPP