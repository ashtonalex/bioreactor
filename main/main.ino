// configure your connection here
#include "secrets.h"
#include "WiFi.h"
#include "PHSubsystem.hpp"
#include "StirringSubsystem.hpp"
#include "heatingSubsystem.hpp"
// end of configuration

// Includes for MQTT
#include <PubSubClient.h>

#if USE_EDUROAM
#include "esp_eap_client.h"
const char* ssid = "eduroam";
const char* user = WIFI_USER;
#else
const char* ssid = WIFI_SSID;
#endif

const char* pass = WIFI_PASS;

// MQTT Client Setup
WiFiClient espClient;
PubSubClient client(espClient);
const char* mqtt_client_id = "bioreactor_ph_client"; // Unique client ID

// --- ThingsBoard Topics ---
// This is the topic we subscribe to for commands (RPC)
const char* command_topic = "v1/devices/me/rpc/request/+"; 

// Timing for publishing data
long lastPublishTime = 0;
const long PUBLISH_INTERVAL = 5000; // Publish data every 5 seconds (5000 ms)

// --- Global State ---
bool is_system_active = true; // Default to ON

// --- Function Prototypes ---
void print_wifi_info();
void wifi_connect(float timeout = 15);
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void attributes_callback(char* topic, byte* payload, unsigned int length);
void handleGlobalAttributes(JsonObject& doc);
void mqtt_reconnect();

void setup() {
  Serial.begin(115200);
  Serial.println("Booting Bioreactor pH Controller (ThingsBoard)...");

  // Setup subsystem hardware pins
  setupPH();
  setupStirring(); // <-- CALL STIRRING SETUP
  setupHeating();
  
  // Connect to WiFi
  wifi_connect();

  // Configure MQTT
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqtt_callback); // Set function to handle incoming messages

  Serial.println("Setup complete.");
}

void loop() {
  // 1. Maintain WiFi and MQTT Connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    wifi_connect();
  }
  
  if (!client.connected()) {
    mqtt_reconnect(); // Reconnect to MQTT broker if disconnected
  }

  // 2. Allow MQTT client to process incoming messages
  client.loop();

  // 3. Run the autonomous control logic for all subsystems
  executePH();
  executeStirring();
  executeHeating();

  // 4. Publish status update to ThingsBoard periodically
  if (millis() - lastPublishTime > PUBLISH_INTERVAL) {
    // Send status for PH and Stirring subsystems
    StaticJsonDocument<500> doc; // Combined JSON doc, increased size
    JsonObject root = doc.to<JsonObject>();

    getPHStatus(root);
    getStirringStatus(root);
    getPHStatus(root);
    getStirringStatus(root);
    getHeatingStatus(root);

    // Global status
    root["operational_mode"] = is_system_active;

    char buffer[500];
    serializeJson(doc, buffer);
    client.publish("v1/devices/me/telemetry", buffer);

    lastPublishTime = millis();
  }
}

/**
 * @brief Handles incoming MQTT messages (from ThingsBoard).
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  // Check if it's an attribute update or response
  if (String(topic).startsWith("v1/devices/me/attributes")) {
    attributes_callback(topic, payload, length);
    return;
  }

  // Check if it's an RPC command
  if (String(topic).startsWith("v1/devices/me/rpc/request/")) {
    // Dispatch to PH subsystem for setPump commands
    handlePHCommand(client, topic, payload, length);
    
    // Note: setRPM and setTemperature are now handled via attributes, so we don't call their command handlers.
  }
}

void attributes_callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Attributes update received");
  
  StaticJsonDocument<500> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  JsonObject shared;
  if (doc.containsKey("shared")) {
    shared = doc["shared"];
  } else {
    shared = doc.as<JsonObject>();
  }

  // Dispatch to all subsystems to check for their keys
  handleGlobalAttributes(shared);
  handlePHAttributes(shared);
  handleStirringAttributes(shared);
  handleHeatingAttributes(shared);
}

void handleGlobalAttributes(JsonObject& doc) {
  if (doc.containsKey("operational_mode")) {
    is_system_active = doc["operational_mode"];
    Serial.print("Updated operational_mode: ");
    Serial.println(is_system_active ? "ACTIVE" : "INACTIVE");
  }
}

/**
 * @brief Reconnects to the MQTT broker and subscribes to command topic.
 */
void mqtt_reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect using Access Token (as MQTT_USER)
    if (client.connect(mqtt_client_id, MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");
      // Subscribe to the ThingsBoard RPC topic
      client.subscribe(command_topic);
      // Subscribe to Attribute topics
      client.subscribe("v1/devices/me/attributes"); // Server-side updates
      client.subscribe("v1/devices/me/attributes/response/+"); // Initial request response
      
      Serial.print("Subscribed to RPC and Attributes");
      
      // Request initial attributes
      client.publish("v1/devices/me/attributes/request/1", "{\"sharedKeys\":\"target_pH,pH_tolerance,target_temperature,temp_tolerance,target_rpm\"}");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// --- WiFi Connection Functions (Unchanged as requested) ---

void print_wifi_info ()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("Connected to WiFi network: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("WiFi not connected");
  }
}

void wifi_connect ( float timeout )
{
  unsigned long deadline;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  #if USE_EDUROAM
    Serial.printf("Connecting to eduroam as user %s\n", user);
    esp_eap_client_set_identity((uint8_t *)user, strlen(user));
    esp_eap_client_set_username((uint8_t *)user, strlen(user));
    esp_eap_client_set_password((uint8_t *)pass, strlen(pass));
    esp_wifi_sta_enterprise_enable();
    WiFi.begin(ssid);
  #else
    Serial.printf("Connecting to %s\n", ssid);
    WiFi.begin(ssid, pass);
  #endif
  
  deadline = millis() + (unsigned long)(timeout * 1000);

  while ((WiFi.status() != WL_CONNECTED) && (millis() < deadline))
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  print_wifi_info();
}