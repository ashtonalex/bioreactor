#ifndef PHSUBSYSTEM_HPP
#define PHSUBSYSTEM_HPP

// We need PubSubClient to be able to publish
#include <PubSubClient.h>

// --- Function Declarations ---

/**
 * @brief Sets up the pin modes for pumps and the pH sensor.
 */
void setupPH();

/**
 * @brief Executes the autonomous, local bang-bang control logic.
 * Reads the pH sensor and updates the pump states based on thresholds.
 */
void executePH();

/**
 * @brief Publishes the current pH and pump status to the MQTT broker.
 * @param client The active PubSubClient instance.
 */
void publishStatus(PubSubClient& client);

/**
 * @brief Handles an incoming RPC command payload from ThingsBoard.
 * Parses JSON to manually pulse a pump and sends a response.
 * @param client The active PubSubClient instance (to send response).
 * @param topic The topic the message arrived on (to get request ID).
 * @param payload The raw message payload.
 * @param length The length of the payload.
 */
void handleCommand(PubSubClient& client, char* topic, byte* payload, unsigned int length);

#endif // PHSUBSYSTEM_HPP