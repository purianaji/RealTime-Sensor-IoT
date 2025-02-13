#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>

// ==================== Configuration Parameters ====================

// Debugging
const bool DEBUG = true; // Set to false to minimize logging

// Wi-Fi credentials
const char* ssid     = "pooria";      // Replace with your Wi-Fi SSID
const char* password = "poria123P";   // Replace with your Wi-Fi password

// MQTT broker settings
const char* mqtt_server   = "broker.emqx.io";
const int   mqtt_port     = 1883;
const char* mqtt_user     = "pooria";
const char* mqtt_password = "pooria";
const char* mqtt_topic    = "Primula";

// NTP server settings
const char* localNTPServer     = "192.168.137.1"; // Primary Local NTP Server
const int   ntpPort            = 123;             // NTP uses UDP port 123
const long  utcOffsetInSeconds = 0;               // Adjust for your timezone (e.g., UTC+2 = 7200)

// UDP instance
WiFiUDP ntpUDP;
const int udpLocalPort = 2390; // Dedicated UDP port for NTP

// NTP packet size
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE]; // Buffer to hold incoming and outgoing packets

// Define GPIO pins
const int ledPin = 7;   // LED GPIO pin
const int adcPin = 28;  // ADC input pin

// Global variables to track current time
time_t currentEpochTime = 0;            // Stores the last synchronized epoch time
unsigned long lastSyncMillis = 0;       // Stores the millis() value at last synchronization

// Global variables
WiFiClient wifiClient;
PubSubClient client(wifiClient);
unsigned long lastActivityTime = 0;
const unsigned long inactivityTimeout = 60000; // 60 seconds
unsigned long nextWifiAttempt = 0;             // Initialize to 0
const unsigned long wifiRetryInterval = 30000;  // 30 seconds

// Simulated button press flag
bool simulatedButtonPress = false;  // Initially false, set true when simulating a button press

// ==================== Function Prototypes ====================
void setup_wifi();
void reconnect_mqtt();
bool synchronize_time(time_t &epochTime);
void sendMQTTMessage(time_t epochTime, int reading);
void callback(char* topic, byte* payload, unsigned int length);
void timerCheck();
void collectAndQueueReadings();
bool checkInternetConnection();
void clearUdpBuffer();
struct tm getCurrentTime();

// ==================== Function Definitions ====================

// Function to clear UDP buffer
void clearUdpBuffer() {
  while (ntpUDP.parsePacket()) {
    ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);
  }
}

// Function to retrieve current time
struct tm getCurrentTime() {
  if (currentEpochTime == 0) {
    // Time not yet synchronized
    struct tm tm_zero = {0};
    return tm_zero;
  }
  
  unsigned long elapsedSeconds = (millis() - lastSyncMillis) / 1000;
  time_t currentTime = currentEpochTime + elapsedSeconds;
  return *gmtime(&currentTime);
}

// Function to get current epoch time from NTP server with retries
bool synchronize_time(time_t &epochTime) {
  // Attempt synchronization with the local NTP server
  if (DEBUG) {
    Serial.printf("Attempting NTP synchronization with local server: %s\n", localNTPServer);
  }

  // Initialize NTP request packet
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;            // Stratum, or type of clock
  packetBuffer[2] = 6;            // Polling Interval
  packetBuffer[3] = 0xEC;         // Peer Clock Precision

  const int maxAttempts = 5;
  unsigned long retryDelay = 200; // Initial retry delay in ms
  const unsigned long maxRetryDelay = 2000;  // Maximum retry delay in ms

  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    Serial.printf("Attempt %d: Sending NTP request to %s:%d\n", attempt, localNTPServer, ntpPort);

    // Clear any residual data in the UDP buffer
    clearUdpBuffer();

    // Send NTP request
    ntpUDP.beginPacket(localNTPServer, ntpPort);
    ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
    ntpUDP.endPacket();

    // Wait for response (extended timeout)
    unsigned long startWait = millis();
    bool received = false;
    while (millis() - startWait < 2000) { // 2-second timeout
      int size = ntpUDP.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        ntpUDP.read(packetBuffer, NTP_PACKET_SIZE); // Read packet into buffer

        // Extract the timestamp starting at byte 40 of the received packet
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord  = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = highWord << 16 | lowWord;

        // Unix time starts on Jan 1 1970. In seconds, that's 2208988800
        const unsigned long seventyYears = 2208988800UL;
        unsigned long epoch = secsSince1900 - seventyYears + utcOffsetInSeconds;

        Serial.printf("Received NTP response: Epoch Time = %lu\n", epoch);
        epochTime = epoch;

        // Update global time variables
        currentEpochTime = epochTime;
        lastSyncMillis = millis();

        return true;
      }
      delay(100); // Short delay before checking again
    }

    Serial.printf("Attempt %d failed: No response received.\n", attempt);
    if (attempt < maxAttempts) {
      Serial.printf("Retrying in %lu ms...\n", retryDelay);
      delay(retryDelay);
      retryDelay = min(retryDelay * 2, maxRetryDelay); // Exponential backoff
    }
  }

  // If all attempts fail, return false
  Serial.println("All NTP synchronization attempts failed.");
  return false;
}

// ==================== Wi-Fi Setup ====================

void setup_wifi() {
  if (DEBUG) {
    Serial.printf("[%02d:%02d:%02d] Connecting to %s\n",
                  0, 0, 0, ssid); // Initial time is 00:00:00
  }

  // Disconnect if already connected
  WiFi.disconnect();
  WiFi.begin(ssid, password);

  // Wait for connection with timeout
  int retry_count = 0;
  const int max_retries = 20; // 10 seconds total (20 * 500ms)
  while (WiFi.status() != WL_CONNECTED && retry_count < max_retries) {
    delay(500);
    if (DEBUG) {
      Serial.print(".");
    }
    retry_count++;
  }

  struct tm currentTime = getCurrentTime();

  if (WiFi.status() == WL_CONNECTED) {
    if (currentTime.tm_year >= 70) { // tm_year is years since 1900
      Serial.printf("\n[%02d:%02d:%02d] Wi-Fi connected\n",
                    currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
    } else {
      Serial.printf("\n[00:00:00] Wi-Fi connected\n");
    }
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    lastActivityTime = millis(); // Update activity time on successful Wi-Fi connect
  } else {
    if (currentTime.tm_year >= 70) {
      Serial.printf("\n[%02d:%02d:%02d] Failed to connect to Wi-Fi.\n",
                    currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
    } else {
      Serial.printf("\n[00:00:00] Failed to connect to Wi-Fi.\n");
    }
    // Optional: Implement additional handling here if Wi-Fi connection fails
  }
}

// ==================== MQTT Reconnection ====================

void reconnect_mqtt() {
  // Loop until reconnected
  while (!client.connected()) {
    struct tm currentTime = getCurrentTime();
    if (currentTime.tm_year >= 70) {
      Serial.printf("[%02d:%02d:%02d] Attempting MQTT connection...\n",
                    currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
    } else {
      Serial.printf("[00:00:00] Attempting MQTT connection...\n");
    }

    String clientId = "RPiPicoWClient-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      currentTime = getCurrentTime();
      if (currentTime.tm_year >= 70) {
        Serial.printf("[%02d:%02d:%02d] MQTT connected\n",
                      currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
      } else {
        Serial.printf("[00:00:00] MQTT connected\n");
      }

      // Subscribe to topic with QoS 1
      if (client.subscribe(mqtt_topic, 1)) {
        if (currentTime.tm_year >= 70) {
          Serial.printf("[%02d:%02d:%02d] Subscribed to topic: %s\n",
                        currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec, mqtt_topic);
        } else {
          Serial.printf("[00:00:00] Subscribed to topic: %s\n", mqtt_topic);
        }
      } else {
        if (currentTime.tm_year >= 70) {
          Serial.printf("[%02d:%02d:%02d] Failed to subscribe to topic: %s\n",
                        currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec, mqtt_topic);
        } else {
          Serial.printf("[00:00:00] Failed to subscribe to topic: %s\n", mqtt_topic);
        }
      }
    } else {
      currentTime = getCurrentTime();
      if (currentTime.tm_year >= 70) {
        Serial.printf("[%02d:%02d:%02d] MQTT connection failed, rc=%d. Retrying in 5 seconds...\n",
                      currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec, client.state());
      } else {
        Serial.printf("[00:00:00] MQTT connection failed, rc=%d. Retrying in 5 seconds...\n",
                      client.state());
      }
      delay(5000);
    }
  }
}

// ==================== MQTT Callback ====================

void callback(char* topic, byte* payload, unsigned int length) {
  struct tm currentTime = getCurrentTime();
  if (currentTime.tm_year >= 70) {
    Serial.printf("[%02d:%02d:%02d] Message arrived [%s] ", 
                  currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec, topic);
  } else {
    Serial.printf("[00:00:00] Message arrived [%s] ", topic);
  }

  String message;
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();

  // Prevent handling duplicate messages
  static String lastMessage = "";
  String incomingMessage = "";
  for (unsigned int i = 0; i < length; i++) {
    incomingMessage += (char)payload[i];
  }
  if (incomingMessage == lastMessage) {
    if (DEBUG) {
      Serial.println("Duplicate message detected, ignoring...");
    }
    return;
  }
  lastMessage = incomingMessage;

  lastActivityTime = millis(); // Reset inactivity timeout on message receive
}

// ==================== MQTT Messaging ====================

void sendMQTTMessage(time_t epochTime, int reading) {
  // Ensure MQTT client is connected
  if (!client.connected()) {
    reconnect_mqtt();
  }

  // Convert epoch time to struct tm
  struct tm *timeinfo = gmtime(&epochTime);

  // Create the MQTT message payload
  char mqttMessage[200]; // Adjust the buffer size based on your payload length
  sprintf(mqttMessage, "{\"Real Time\":\"%04d-%02d-%02dT%02d:%02d:%02dZ\",\"node_id\":\"%s\",\"reading\":%d}",
          timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
          timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
          "RPiPicoWClient", reading);

  // Publish the message to the MQTT topic with QoS 1 and retain set to false
  if (client.publish(mqtt_topic, (uint8_t*)mqttMessage, strlen(mqttMessage), false)) {
    struct tm currentTime = getCurrentTime();
    if (currentTime.tm_year >= 70) {
      Serial.printf("[%02d:%02d:%02d] MQTT Message Sent:\n", 
                    currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
    } else {
      Serial.printf("[00:00:00] MQTT Message Sent:\n");
    }
    Serial.println(mqttMessage);
    lastActivityTime = millis(); // Update activity time on successful send
  } else {
    struct tm currentTime = getCurrentTime();
    if (currentTime.tm_year >= 70) {
      Serial.printf("[%02d:%02d:%02d] Failed to send MQTT message.\n", 
                    currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
    } else {
      Serial.printf("[00:00:00] Failed to send MQTT message.\n");
    }
    // Optional: Implement retry logic or message queuing here
  }
}

// ==================== Inactivity Timer Check ====================

void timerCheck() {
  // Check for inactivity timeout every second
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 1000) {
    lastCheck = millis();
    if (millis() - lastActivityTime >= inactivityTimeout) {
      struct tm currentTime = getCurrentTime();
      if (currentTime.tm_year >= 70) {
        Serial.printf("[%02d:%02d:%02d] Inactivity timeout reached after %lu ms.\n",
                      currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec, millis() - lastActivityTime);
      } else {
        Serial.printf("[00:00:00] Inactivity timeout reached after %lu ms.\n", millis() - lastActivityTime);
      }
      // Optional: Implement inactivity handling here (e.g., enter low-power mode)
    }
  }
}

// ==================== Collect and Queue Readings ====================

void collectAndQueueReadings() {
  // Simulate a button press every 10 seconds for testing
  if (millis() - lastActivityTime >= 10000 && !simulatedButtonPress) {
    simulatedButtonPress = true;
  }

  // Handle simulated button press
  if (simulatedButtonPress) {
    // Log the simulated button press
    struct tm currentTime = getCurrentTime();
    if (currentTime.tm_year >= 70) { // tm_year is years since 1900
      Serial.printf("[%02d:%02d:%02d] Simulated Button pressed\n",
                    currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
    } else {
      Serial.printf("[00:00:00] Simulated Button pressed\n");
    }
    lastActivityTime = millis(); // Reset inactivity timeout

    // Check internet connection before collecting data
    if (!checkInternetConnection()) {
      if (DEBUG) {
        Serial.println("No internet connection available. Skipping data collection.");
      }
      simulatedButtonPress = false;
      return;
    }

    // Start collecting ADC reading and store in FIFO buffer
    unsigned long startADC = micros(); // Use micros for higher precision
    int adcValue = analogRead(adcPin);
    unsigned long endADC = micros();
    if (DEBUG) {
      Serial.printf("Total ADC acquisition time: %lu µs\n", endADC - startADC);
    }

    // Add the reading to the FIFO buffer
    // (Assuming you have implemented a FIFO buffer elsewhere; otherwise, this is a placeholder)
    // adcBuffer[headIndex] = adcValue;
    // headIndex = (headIndex + 1) % BUFFER_SIZE;
    // if (headIndex == tailIndex) {
    //   tailIndex = (tailIndex + 1) % BUFFER_SIZE;
    // }

    // Now get NTP time
    time_t epochTime;
    unsigned long startNTP = millis();
    if (synchronize_time(epochTime)) {
      unsigned long endNTP = millis();
      
      // Retrieve current time after synchronization
      currentTime = getCurrentTime();
      
      if (DEBUG) {
        Serial.printf("[%02d:%02d:%02d] NTP synchronization took: %lu ms\n",
                      currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec, endNTP - startNTP);
      }

      // Now send MQTT message for the latest reading
      unsigned long startMQTT = millis();
      sendMQTTMessage(epochTime, adcValue);
      unsigned long endMQTT = millis();
      if (DEBUG) {
        Serial.printf("[%02d:%02d:%02d] MQTT message sending took: %lu ms\n",
                      currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec, endMQTT - startMQTT);
      }

      digitalWrite(ledPin, HIGH); // Indicate message sent
      delay(1000);     // Prevent rapid consecutive sends
      digitalWrite(ledPin, LOW);
    } else {
      // NTP synchronization failed
      struct tm currentTimeFailed = getCurrentTime();
      if (currentTimeFailed.tm_year >= 70) {
        Serial.printf("[%02d:%02d:%02d] Failed to get NTP time. Message not sent.\n",
                      currentTimeFailed.tm_hour, currentTimeFailed.tm_min, currentTimeFailed.tm_sec);
      } else {
        Serial.printf("[00:00:00] Failed to get NTP time. Message not sent.\n");
      }
    }

    // Simulated button press is now done, set flag to false
    simulatedButtonPress = false;

    // Add delay to prevent immediate re-trigger
    delay(500); // This delay will help to avoid quickly repeating button press actions
  }
}

// ==================== Check Internet Connection ====================

bool checkInternetConnection() {
  WiFiClient testClient;
  if (testClient.connect("8.8.8.8", 53)) { // Ping Google's public DNS server
    testClient.stop();
    return true;
  } else {
    return false;
  }
}

// ==================== Setup Function ====================

void setup() {
  Serial.begin(115200);
  delay(1000); // Ensure Serial is ready
  if (DEBUG) {
    Serial.println("Module started");
    Serial.print("Inactivity Timeout is set to: ");
    Serial.print(inactivityTimeout);
    Serial.println(" ms");
  }

  // Set up LED pin as output
  pinMode(ledPin, OUTPUT);
  pinMode(adcPin, INPUT); // Set ADC pin as input

  // Start UDP for NTP
  ntpUDP.begin(udpLocalPort);

  // Connect to Wi-Fi
  setup_wifi();

  // Initialize MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Attempt to connect to MQTT broker
  reconnect_mqtt();

  // Set the last activity time
  lastActivityTime = millis();
}

// ==================== Main Loop ====================

void loop() {
  // Check for inactivity timeout
  timerCheck();

  // Ensure Wi-Fi is connected with cooldown
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (currentMillis >= nextWifiAttempt) {
      setup_wifi();
      nextWifiAttempt = currentMillis + wifiRetryInterval;
    }
  }

  // Reconnect to MQTT if disconnected
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  // Collect and queue readings
  collectAndQueueReadings();
}
