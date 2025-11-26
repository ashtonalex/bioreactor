#ifndef STIRRINGSUBSYSTEM_HPP
#define STIRRINGSUBSYSTEM_HPP

#include <Arduino.h>
#include <PubSubClient.h> // Keep this as we'll need it for future MQTT publishing
#include <ArduinoJson.h>

// --- Pin Definitions ---
const byte ENCODER_PIN = 2;   // D2 -> Hall sensor
const byte MOTOR_PIN   = 10;  // D10 -> MOSFET gate (PWM)
const byte LED_RED_PIN = LED_RED; // Use the built-in red LED for visual pulse confirmation

// --- Motor & Control Parameters ---
extern float MotorSupplyVoltage; // Defined in .cpp, declared here for external access
extern int RPM_MAX;
extern float setspeed;          // Current RPM setpoint (read/write)
extern float meanmeasspeed;     // Current measured RPM (read-only)

// --- Function Prototypes ---
extern bool is_system_active;

/**
 * @brief Handles the Hall sensor interrupt. Measures pulse timing for RPM calculation.
 */
void IRAM_ATTR Tsense();

/**
 * @brief Initializes pins, PWM, and the Hall sensor interrupt.
 */
void setupStirring();

/**
 * @brief The main control loop for the motor.
 * Reads serial commands (for local control) and executes the PI controller.
 */
void executeStirring();

/**
 * @brief Populates the passed JSON object with the current RPM status.
 * @param doc The JsonObject to populate.
 */
void getStirringStatus(JsonObject& doc);

/**
 * @brief Handles incoming Shared Attribute updates.
 * @param doc The JsonObject containing the attributes.
 */
void handleStirringAttributes(JsonObject& doc);

#endif // STIRRINGSUBSYSTEM_HPP