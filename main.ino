// configure your connection here
#include "secrets.h"
#include "WiFi.h"
#include "PHSubsystem.hpp"
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

// --- Function Prototypes ---
void print_wifi_info();
void wifi_connect(float timeout = 15);
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_reconnect();

void setup() {
  Serial.begin(115200);
  Serial.println("Booting Bioreactor pH Controller (ThingsBoard)...");

  // Setup hardware pins
  setupPH();
  
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

  // 3. Run the autonomous pH control logic
  // This logic runs independently of MQTT
  executePH();

  // 4. Publish status update to ThingsBoard periodically
  if (millis() - lastPublishTime >= PUBLISH_INTERVAL) {
    publishStatus(client); // Send current data to ThingsBoard
    lastPublishTime = millis();
  }
}

/**
 * @brief Handles incoming MQTT messages (from ThingsBoard).
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  // Pass the topic and payload to the subsystem to handle the command
  // We pass the topic to extract the RPC Request ID
  handleCommand(client, topic, payload, length);
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
      Serial.print("Subscribed to: ");
      Serial.println(command_topic);
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

void wifi_connect ( float timeout = 15 )
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