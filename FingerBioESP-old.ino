#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h> // Include the WebServer library for ESP32
#include <HardwareSerial.h> // Use HardwareSerial for ESP32 to Arduino Mega communication

// --- Wi-Fi Credentials ---
const char* ssid = "valencia";  // Your WiFi SSID
const char* password = "yoshihtzu"; // Your WiFi password

// --- Server Configuration ---
// This is the IP address of your XAMPP/Laragon server (your PC's IP)
// and the path to your index.php script.
const char* serverUrl = "http://192.168.68.112/fingerprint_dashboard/index.php"; // Use your computer's actual IP
// --- Serial Communication with Arduino Mega ---
// IMPORTANT: ESP32's Hardware Serial2 (RX2/GPIO16, TX2/GPIO17) will communicate with Arduino Mega's HardwareSerial3 (pins 14, 15)
// Ensure these pins are correctly wired:
// ESP32 GPIO16 (RX2) -> Arduino Mega Pin 15 (TX3)  <--- ESP32's RX receives from Mega's TX
// ESP32 GPIO17 (TX2) -> Arduino Mega Pin 14 (RX3)  <--- ESP32's TX sends to Mega's RX
HardwareSerial SerialArduino(2); // <--- CRITICAL FIX: Use Serial2 on ESP32 (GPIO16, GPIO17)

// --- Web Server for receiving commands from api.php ---
WebServer server(80); // Create a web server on port 80

// --- Global Variables ---
String arduinoResponse = ""; // To store responses from Arduino Mega

// --- Function Prototypes ---
void handleEnroll();
void handleDelete();
void handleGetList();
void handleNotFound();
void forwardArduinoResponseToServer(String response);
void sendCommandToArduino(String command);

void setup() {
  Serial.begin(115200); // For debugging output to PC Serial Monitor
  // Initialize Hardware Serial2 for communication with Arduino Mega (Pins 16, 17)
  // Baud rate MUST match Arduino Mega's Serial3 baud rate (38400)
  SerialArduino.begin(38400, SERIAL_8N1, 16, 17); // Use GPIO16 for RX, GPIO17 for TX

  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // --- Setup Web Server Routes ---
    // These routes will be accessed by your api.php script to send commands to the ESP32.
    server.on("/enroll", HTTP_GET, handleEnroll);
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/get_list", HTTP_GET, handleGetList);
    server.onNotFound(handleNotFound); // Handle unknown routes

    server.begin(); // Start the web server
    Serial.println("HTTP server started");

    // *** NEW: Wait for Mega to signal it's ready ***
    Serial.println("Waiting for MEGA_READY signal from Arduino...");
    unsigned long startTime = millis();
    bool megaReady = false;
    while (!megaReady && (millis() - startTime < 10000)) { // Wait up to 10 seconds
      if (SerialArduino.available()) {
        String receivedData = SerialArduino.readStringUntil('\n');
        receivedData.trim();
        Serial.print("ESP32 received during handshake: '");
        Serial.print(receivedData);
        Serial.println("'");
        if (receivedData == "MEGA_READY") {
          megaReady = true;
          Serial.println("MEGA_READY received! Communication established.");
        }
      }
      delay(50); // Small delay to prevent tight loop
    }

    if (!megaReady) {
      Serial.println("Error: Did not receive MEGA_READY from Arduino within timeout.");
      // You might want to halt or reboot here in a real application
    }
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.println("Please check SSID and Password.");
    // In a real application, you might want to restart or go into a low-power mode here.
  }
}

void loop() {
  // Handle incoming data from Arduino Mega
  while (SerialArduino.available()) {
    char c = SerialArduino.read();
    arduinoResponse += c;
    if (c == '\n') { // Check for newline character as end of message
      arduinoResponse.trim(); // Remove leading/trailing whitespace
      Serial.print("Received from Arduino: ");
      Serial.println(arduinoResponse);
      forwardArduinoResponseToServer(arduinoResponse); // <--- UNCOMMENT THIS
      arduinoResponse = ""; // Clear buffer for next message
    }
  }

  // Handle incoming HTTP requests from the web dashboard (via api.php)
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient(); // <--- UNCOMMENT THIS
  }
}

// --- Web Server Command Handlers (rest of code is unchanged) ---
void handleEnroll() {
  if (server.hasArg("id")) {
    String id = server.arg("id");
    String command = "enroll," + id; // Format command for Arduino Mega
    sendCommandToArduino(command);
    server.send(200, "text/plain", "Command sent to Arduino: " + command);
  } else {
    server.send(400, "text/plain", "Error: Missing ID parameter for enroll.");
  }
}

void handleDelete() {
  if (server.hasArg("id")) {
    String id = server.arg("id");
    String command = "delete," + id; // Format command for Arduino Mega
    sendCommandToArduino(command);
    server.send(200, "text/plain", "Command sent to Arduino: " + command);
  } else {
    server.send(400, "text/plain", "Error: Missing ID parameter for delete.");
  }
}

void handleGetList() {
  sendCommandToArduino("get_list");
  server.send(200, "text/plain", "Command sent to Arduino: get_list. Awaiting response.");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void forwardArduinoResponseToServer(String response) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Standard for form data

    // --- NEW LOGIC TO PARSE ARDUINO RESPONSE ---
    int commaIndex = response.indexOf(',');
    if (commaIndex != -1) {
      String id = response.substring(0, commaIndex);
      String timestamp = response.substring(commaIndex + 1);

      // Now construct the POST data correctly with 'id' and 'timestamp' parameters
      String httpRequestData = "id=" + id + "&timestamp=" + timestamp;

      Serial.print("Sending to server: ");
      Serial.println(httpRequestData);

      int httpResponseCode = http.POST(httpRequestData);

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String serverResponse = http.getString();
        Serial.print("Server Response: ");
        Serial.println(serverResponse);
      } else {
        Serial.print("Error on HTTP request: ");
        Serial.println(httpResponseCode);
        Serial.println(http.errorToString(httpResponseCode)); // Added for more detailed error
      }
    } else {
      // Handle cases where the Arduino response doesn't contain a comma (e.g., status messages)
      // You might want to send these as a different parameter or log them differently.
      // For now, let's send it as 'arduino_message' to distinguish it.
      String httpRequestData = "arduino_message=" + response;
      Serial.print("Sending non-attendance message to server: ");
      Serial.println(httpRequestData);
      int httpResponseCode = http.POST(httpRequestData);
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code (message): ");
        Serial.println(httpResponseCode);
        String serverResponse = http.getString();
        Serial.print("Server Response (message): ");
        Serial.println(serverResponse);
      } else {
        Serial.print("Error on HTTP request (message): ");
        Serial.println(httpResponseCode);
        Serial.println(http.errorToString(httpResponseCode));
      }
    }
    // --- END NEW LOGIC ---

    http.end();
  } else {
    Serial.println("WiFi Disconnected. Cannot send data to server.");
  }
}

/*
void forwardArduinoResponseToServer(String response) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Standard for form data

    String httpRequestData = "data=" + response;

    Serial.print("Sending to server: ");
    Serial.println(httpRequestData);

    int httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String serverResponse = http.getString();
      Serial.print("Server Response: ");
      Serial.println(serverResponse);
    } else {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected. Cannot send data to server.");
  }
}
*/

void sendCommandToArduino(String command) {
  SerialArduino.println(command); // Send command followed by a newline
  Serial.print("Sent to Arduino Mega via serial: ");
  Serial.println(command);
}