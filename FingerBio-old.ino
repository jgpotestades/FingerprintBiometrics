  #include <Adafruit_Fingerprint.h>
  #include <RTClib.h>
  #include <SD.h>
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>

  // --- Pin Definitions ---
  //const int SD_CS_PIN = 8;       // SD card CS pin
  const int LCD_ADDR = 0x27;      // I2C address of the LCD (check yours!)
  const int FINGER_RX_PIN = 14;   // Fingerprint sensor RX pin (Mega RX2)
  const int FINGER_TX_PIN = 15;   // Fingerprint sensor TX pin (Mega TX2)

  LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2); // 16 columns and 2 rows
  Adafruit_Fingerprint finger(&Serial2); // Use Hardware Serial2 for fingerprint
  RTC_DS3231 rtc;

  // --- Wi-Fi Credentials (These are for reference only, used by ESP32) ---
  // (Not used by Mega, kept for context)
  const char* WIFI_SSID = "valencia";
  const char* WIFI_PASSWORD = "yoshihtzu";
  const char* SERVER_IP = "192.168.68.112";
  const int SERVER_PORT = 80;

  // --- Function Prototypes ---
  void initializeModules();
  bool enrollFinger(uint8_t id);
  int verifyFinger();
  void logAttendance(int id);
  //void logToSD(const char* filename, const char* data);
  void sendToEsp(const char* data);
  DateTime getTimestamp();
  void displayMessage(const char* message, int duration = 0);
  void displayMessage(const char* line1, const char* line2, int duration = 0);
  void deleteFingerprint(uint8_t id);
  uint8_t getNextAvailableFingerprintId();
  String getFingerprintList(); // Placeholder logic for now
  void processEspCommand();
  //void handleAttendanceMode(); // Added prototype for better practice
  char timestampBuffer[20];

  void setup() {
    Serial.begin(57600); // For debugging output to PC
    delay(100);
    Serial.println("Arduino Mega Initialized!");

    displayMessage("System Starting");

    initializeModules(); // Initializes fingerprint, LCD, RTC, and ESP serial

    // Initialize SD card
  // if (!SD.begin(SD_CS_PIN)) {
    // displayMessage("SD Card", "Error!");
    // Serial.println("SD card initialization failed!");
    //} else {
    // Serial.println("SD card initialized.");
    //}

    // Initialize RTC
    if (!rtc.begin()) {
      displayMessage("RTC", "Error!");
      Serial.println("Couldn't find RTC!");
    } else if (rtc.lostPower()) {
      displayMessage("RTC", "Lost Power!");
      Serial.println("RTC lost power, setting time...");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    } else {
      displayMessage("RTC", "OK");
    }
    delay(2000); // This delay is fine here, ESP32 has a timeout

    // Signal to ESP32 that Mega is ready to receive commands
    Serial3.println("MEGA_READY"); // Uses Mega's TX3 (Pin 15)
    Serial.println("Sent MEGA_READY signal to ESP32.");
  }

  void loop() {
    // Always handle attendance mode
    handleAttendanceMode(); // This function will block while waiting for a finger scan

    // Continuously check for commands from ESP32
    processEspCommand();
    // Removed delay(10); - good, no blocking here
  }

  /*void handleAttendanceMode() {
    displayMessage("Scan Finger", "");
    int fingerId = verifyFinger();

    if (fingerId > 0) {
      displayMessage("ID:", String(fingerId).c_str(), 2000);
      DateTime now = getTimestamp();
      String timestamp = now.toString("%Y-%m-%d %H:%M:%S");
      String logData = String(fingerId) + "," + timestamp;
      logAttendance(fingerId);
      //logToSD("attendance.csv", logData.c_str());
      sendToEsp(logData.c_str()); // Send to ESP32 via serial
    } else if (fingerId == -1) {
      displayMessage("No finger", "detected", 1000);
    } else if (fingerId == -2) {
      displayMessage("Finger", "not found", 1000);
    } else if (fingerId == -3) {
      displayMessage("Error", "reading sensor", 1000);
    } else {
      displayMessage("Error", "verifying", 1000);
    }
  }
*/
  void handleAttendanceMode() {
    displayMessage("Scan Finger", "");
    int fingerId = verifyFinger();

    if (fingerId > 0) {
      displayMessage("ID:", String(fingerId).c_str(), 2000);
      DateTime now = getTimestamp();

      // Use strftime to format the timestamp into the buffer
      // now.toString() is better for Print/Serial directly.
      // For String manipulation, it's safer to use strftime or similar.

      // This is the correct way to format DateTime into a C-string
      sprintf(timestampBuffer, "%04d-%02d-%02d %02d:%02d:%02d",
              now.year(), now.month(), now.day(),
              now.hour(), now.minute(), now.second());

      String timestampString = String(timestampBuffer); // Convert C-string to String object
      String logData = String(fingerId) + "," + timestampString;
      logAttendance(fingerId);
      sendToEsp(logData.c_str()); // Send to ESP32 via serial
    } else if (fingerId == -1) {
      displayMessage("No finger", "detected", 1000);
    } else if (fingerId == -2) {
      displayMessage("Finger", "not found", 1000);
    } else if (fingerId == -3) {
      displayMessage("Error", "reading sensor", 1000);
    } else {
      displayMessage("Error", "verifying", 1000);
    }
  }

  void initializeModules() {
    Serial.println("Initializing modules...");
    Serial2.begin(57600); // Initialize Hardware Serial2 for fingerprint
    finger.begin(57600); // Added baud rate argument

    if (finger.verifyPassword()) {
      Serial.println("Fingerprint sensor found!");
    } else {
      Serial.println("Did not find fingerprint sensor :(");
      displayMessage("Fingerprint", "Error");
      while (1) {
        delay(1);
      } // Halt if no sensor
    }

    Serial.print("Sensor template count: ");
    Serial.println(finger.templateCount);

    Wire.begin();       // Initialize I2C bus for LCD and RTC
    lcd.init();         // Initialize LCD
    lcd.backlight();    // Turn on backlight
    lcd.clear();

    // MEGA's Serial3 (Pins 14/15) for ESP32
    Serial3.begin(38400); // Initialize Hardware Serial3 for ESP32 communication
  }

  bool enrollFinger(uint8_t id) {
    int p = -1;
    Serial.print("Enrolling fingerprint ID: ");
    Serial.println(id);
    sendToEsp(("arduino_status:Ready to enroll ID " + String(id) + ". Place finger.").c_str());

    while (p != FINGERPRINT_OK) {
      displayMessage("Place finger", "to enroll");
      Serial.println("Place finger on sensor to enroll...");
      p = finger.getImage();
      switch (p) {
        case FINGERPRINT_OK:
          displayMessage("Image taken", "");
          Serial.println("Image taken");
          break;
        case FINGERPRINT_NOFINGER:
          displayMessage("", "");
          break;
        case FINGERPRINT_PACKETRECIEVEERR:
          displayMessage("Error", "getting image");
          Serial.println("Error getting image");
          sendToEsp("enrollment_failed:Error getting image");
          return false;
        default:
          displayMessage("Unknown", "error");
          Serial.println("Unknown error (getImage): " + String(p));
          sendToEsp("enrollment_failed:Unknown error getting image");
          return false;
      }
      delay(500);
    }

    p = finger.image2Tz(1);
    switch (p) {
      case FINGERPRINT_OK:
        displayMessage("Image", "processed");
        Serial.println("Image converted to template 1");
        break;
      case FINGERPRINT_IMAGEMESS: // CORRECTED TYPO HERE!
        displayMessage("Image too", "messy");
        Serial.println("Image too messy");
        sendToEsp("enrollment_failed:Image too messy");
        return false;
      case FINGERPRINT_PACKETRECIEVEERR:
        displayMessage("Packet", "error");
        Serial.println("Packet receive error (image2Tz 1)");
        sendToEsp("enrollment_failed:Packet error (image2Tz 1)");
        return false;
      case FINGERPRINT_FEATUREFAIL:
        displayMessage("Could not", "find features");
        Serial.println("Could not find features");
        sendToEsp("enrollment_failed:Could not find features");
        return false;
      case FINGERPRINT_INVALIDIMAGE:
        displayMessage("Invalid", "image");
        Serial.println("Invalid image (image2Tz 1)");
        sendToEsp("enrollment_failed:Invalid image");
        return false;
      default:
        displayMessage("Unknown", "error");
        Serial.println("Unknown error (image2Tz 1): " + String(p));
        sendToEsp("enrollment_failed:Unknown error image2Tz 1");
        return false;
      }

    displayMessage("Remove finger", "");
    Serial.println("Remove finger from sensor.");
    delay(2000);
    p = 0;
    while (p != FINGERPRINT_NOFINGER) {
      p = finger.getImage();
      delay(100);
    }

    displayMessage("Place same", "finger again");
    Serial.println("Place the *same* finger on the sensor again.");
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        displayMessage("Image taken", "");
        Serial.println("Second image taken");
        break;
      case FINGERPRINT_NOFINGER:
        displayMessage("", "");
        Serial.println("No finger detected for second image. Enrollment failed.");
        sendToEsp("enrollment_failed:No second finger detected");
        return false;
      case FINGERPRINT_PACKETRECIEVEERR:
        displayMessage("Error", "getting image");
        Serial.println("Error getting second image");
        sendToEsp("enrollment_failed:Error getting second image");
        return false;
      default:
        displayMessage("Unknown", "error");
        Serial.println("Unknown error (second getImage): " + String(p));
        sendToEsp("enrollment_failed:Unknown error second image");
        return false;
    }

    p = finger.image2Tz(2);
    switch (p) {
      case FINGERPRINT_OK:
        displayMessage("Image", "processed");
        Serial.println("Image converted to template 2");
        break;
      case FINGERPRINT_IMAGEMESS: // CORRECTED TYPO HERE!
        displayMessage("Image too", "messy");
        Serial.println("Second image too messy");
        sendToEsp("enrollment_failed:Second image too messy");
        return false;
      case FINGERPRINT_PACKETRECIEVEERR:
        displayMessage("Packet", "error");
        Serial.println("Packet receive error (image2Tz 2)");
        sendToEsp("enrollment_failed:Packet error (image2Tz 2)");
        return false;
      case FINGERPRINT_FEATUREFAIL:
        displayMessage("Could not", "find features");
        Serial.println("Could not find features in second image");
        sendToEsp("enrollment_failed:Could not find features second image");
        return false;
      case FINGERPRINT_INVALIDIMAGE:
        displayMessage("Invalid", "image");
        Serial.println("Invalid image (image2Tz 2)");
        sendToEsp("enrollment_failed:Invalid second image");
        return false;
      default:
        displayMessage("Unknown", "error");
        Serial.println("Unknown error (image2Tz 2): " + String(p));
        sendToEsp("enrollment_failed:Unknown error image2Tz 2");
        return false;
    }

    displayMessage("Creating", "model...");
    Serial.println("Creating model from two templates...");
    sendToEsp("arduino_status:Creating fingerprint model...");

    p = finger.createModel();
    if (p == FINGERPRINT_OK) {
      displayMessage("Model", "created");
      Serial.println("Model created successfully!");
    } else {
      displayMessage("Model", "failed");
      Serial.println("Model creation failed: " + String(p));
      sendToEsp("enrollment_failed:Model creation failed");
      return false;
    }

    displayMessage("Storing", "fingerprint...");
    Serial.print("Storing fingerprint ID #");
    Serial.print(id);
    Serial.print("...");
    sendToEsp(("arduino_status:Storing fingerprint ID " + String(id) + "...").c_str());
    p = finger.storeModel(id);
    if (p == FINGERPRINT_OK) {
      displayMessage("Stored!", "");
      Serial.println("Stored successfully!");
      sendToEsp(("enrollment_success:" + String(id)).c_str());
    } else {
      if (p == FINGERPRINT_BADLOCATION) {
        displayMessage("Could not", "store there");
        Serial.println("Could not store in that location (ID already exists or invalid)");
        sendToEsp(("enrollment_failed:ID " + String(id) + " already exists or invalid location").c_str());
      } else if (p == FINGERPRINT_DBRANGEFAIL) {
        displayMessage("Database", "full");
        Serial.println("Database full");
        sendToEsp("enrollment_failed:Database full");
      } else {
        Serial.println("Unknown error during storage: " + String(p));
        displayMessage("Unknown", "error");
        sendToEsp(("enrollment_failed:Unknown storage error " + String(p)).c_str());
      }
      return false;
    }
    return true;
  }

  int verifyFinger() {
    uint8_t p = finger.getImage();
    if (p != FINGERPRINT_OK) {
      if (p == FINGERPRINT_NOFINGER) {
        return -1; // No finger detected
      }
      Serial.println("Error getting image for verification: " + String(p));
      return -3; // Error reading sensor
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) {
      Serial.println("Error converting image for verification: " + String(p));
      return -3; // Error reading sensor
    }

    p = finger.fingerSearch();
    if (p == FINGERPRINT_OK) {
      Serial.print("Found fingerprint ID #");
      Serial.println(finger.fingerID);
      return finger.fingerID;
    } else if (p == FINGERPRINT_NOFINGER) {
      return -2; // No finger detected (shouldn't happen here if getImage was OK)
    } else if (p == FINGERPRINT_NOTFOUND) {
      Serial.println("Did not find a match.");
      return -2; // Finger not found
    } else {
      Serial.print("Unknown error during search: ");
      Serial.println(p);
      return -3; // Error verifying
    }
  }

  void logAttendance(int id) {
    Serial.print("Attendance logged for ID: ");
    Serial.println(id);
    DateTime now = getTimestamp();
    Serial.print("Timestamp: ");
    // Now, correctly print the formatted timestamp to Serial Monitor
    char serialTimestampBuffer[20]; // Local buffer for Serial.println
    sprintf(serialTimestampBuffer, "%04d-%02d-%02d %02d:%02d:%02d",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());
    Serial.println(serialTimestampBuffer); // Print the formatted C-string
  }
  //void logToSD(const char* filename, const char* data) {
  // if (!SD.begin(SD_CS_PIN)) {
    //  Serial.println("SD card not initialized.  Skipping SD log.");
    // return;
    //}
    //File dataFile = SD.open(filename, FILE_WRITE);
    //if (dataFile) {
      //dataFile.println(data);
    // dataFile.close();
      //Serial.println("Data written to SD card");
    //} else {
    // Serial.println("Error opening " + String(filename));
    //}
    // SD.end(); // <<< CRITICAL FIX: COMMENT OUT THIS LINE for continuous logging
  //}

  void sendToEsp(const char* data) {
    Serial3.println(data);
    Serial.print("Sent to ESP32 via serial: ");
    Serial.println(data);
  }

  DateTime getTimestamp() {
    if (!rtc.begin()) {
      Serial.println("RTC not found!");
      return DateTime(F(__DATE__), F(__TIME__)); // Fallback to compile time if RTC not found
    }
    return rtc.now();
  }

  void displayMessage(const char* message, int duration) {
    lcd.clear();
    lcd.print(message);
    if (duration > 0) {
      delay(duration);
    }
  }

  void displayMessage(const char* line1, const char* line2, int duration) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
    if (duration > 0) {
      delay(duration);
    }
  }

  void deleteFingerprint(uint8_t id) {
    uint8_t p = finger.deleteModel(id);
    if (p == FINGERPRINT_OK) {
      Serial.println("Deleted!");
      displayMessage("Fingerprint", "Deleted");
      sendToEsp(("deleted_success:" + String(id)).c_str());
    } else if (p == FINGERPRINT_NOMATCH) {
      Serial.println("ID mismatch");
      displayMessage("ID", "Mismatch");
      sendToEsp(("deleted_failed:ID " + String(id) + " not found").c_str());
    } else {
      Serial.print("Error deleting: ");
      Serial.println(p);
      displayMessage("Error", "Deleting");
      sendToEsp(("deleted_failed:Unknown error deleting " + String(p)).c_str());
    }
  }

  uint8_t getNextAvailableFingerprintId() {
    Serial.println("WARNING: getNextAvailableFingerprintId() is a placeholder.");
    // This is a placeholder. You'll implement the actual logic later.
    return 1;
  }

  String getFingerprintList() {
    Serial.println("WARNING: getFingerprintList() is a placeholder again for testing.");
    String enrolledIDs = "";
    /*
    // This block was previously uncommented but is now temporarily disabled
    // to isolate the serial communication issue.
    for (uint8_t i = 1; i <= finger.capacity; i++) {
      // Note: finger.loadModel() is generally used to load a template into Tz1 buffer.
      // If it returns FINGERPRINT_OK for an ID, it means a template exists at that ID.
      if (finger.loadModel(i) == FINGERPRINT_OK) { // If loading succeeds, ID is used
        if (enrolledIDs.length() > 0) {
          enrolledIDs += ","; // Add a comma separator if not the first ID
        }
        enrolledIDs += String(i); // Add the found ID to the string
      }
    }
    */
    return "fingerprint_list:" + enrolledIDs;
  }

  void processEspCommand() {
    if (Serial3.available()) {
      String command = "";
      char incomingChar;

      // Read characters one by one until newline or no more characters
      while (Serial3.available()) {
        incomingChar = Serial3.read();
        command += incomingChar;
        if (incomingChar == '\n') {
          break; // Stop reading if newline is found
        }
        // delay(1); // Optional: uncomment if you suspect issues with very fast incoming data, but try without first.
      }

      Serial.print("DEBUG: Raw command received (before trim): '");
      Serial.print(command);
      Serial.println("'");

      command.trim(); // Trim whitespace, including the newline if present
      Serial.print("Received command from ESP32: ");
      Serial.println(command);

      if (command == "MEGA_READY") {
        Serial.println("Handshake received from ESP32.");
        // The ESP32 sends MEGA_READY *after* it receives it from the Mega,
        // so this condition handles the ESP32 echoing it back for some reason.
        // It's not part of the initial handshake for the ESP32.
        return; 
      }

      if (command.startsWith("enroll,")) {
        uint8_t id = command.substring(command.indexOf(',') + 1).toInt();
        if (id > 0 && id < 255) {
          enrollFinger(id);
        } else {
          sendToEsp("enrollment_failed:Invalid ID received from web");
        }
      } else if (command.startsWith("delete,")) {
        uint8_t id = command.substring(command.indexOf(',') + 1).toInt();
        if (id > 0 && id < 255) {
          deleteFingerprint(id);
        } else {
          sendToEsp("deleted_failed:Invalid ID received from web");
        }
      } else if (command == "get_list") {
        sendToEsp(getFingerprintList().c_str());
      } else {
        sendToEsp("arduino_error:Unknown command received");
      }
    }
  }