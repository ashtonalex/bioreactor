#ifndef SECRETS_H
#define SECRETS_H

// Wi-Fi Credentials
#define USE_EDUROAM 1 // Set to 1 if you use eduroam, 0 for standard WiFi
#define WIFI_SSID "eduroam"
#define WIFI_PASS "OnakBakok13@"
#define WIFI_USER "zcabag2@ucl.ac.uk" // Only used if USE_EDUROAM is 1

// --- ThingsBoard Credentials ---
// Find these on your ThingsBoard server or cloud account
#define MQTT_SERVER "mqtt.eu.thingsboard.cloud" // Or your self-hosted server IP/domain
#define MQTT_PORT 1883

// On your ThingsBoard device page, find "Access token"
// Use the Access Token as the MQTT_USER.
#define MQTT_USER "ujp5e0v81hgvbkr2e4el"
#define MQTT_PASS "" // Leave blank when using Access Token auth

#endif
