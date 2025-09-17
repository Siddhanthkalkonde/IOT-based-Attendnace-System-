#define ENCODER_DO_NOT_USE_INTERRUPTS
#define REMOTEXY_MODE__WIFI_POINT
#include <Encoder.h>
#include <DS1302.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  // Make sure to use ESP32-compatible version
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <RemoteXY.h>

// WiFi credentials - REPLACE WITH YOUR VALUES
const char* WIFI_SSID = "Sid";
const char* WIFI_PASSWORD = "azzd9478";

// Google Sheets configuration - REPLACE WITH YOUR SCRIPT URL
const char* GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbz_It4LIN_Ocn70k9ijfeahpATr0x8bmPfno5B6h7EuiqH32a2IOrnwaj5wQCQCc0WZ1w/exec" ;
// RemoteXY configuration
#define REMOTEXY_WIFI_SSID "VOLTA ATTENDANCE"
#define REMOTEXY_WIFI_PASSWORD ""
#define REMOTEXY_SERVER_PORT 6377

#pragma pack(push, 1)  
uint8_t RemoteXY_CONF[] = { 
  255,64,0,16,0,178,0,19,0,0,0,0,31,1,106,200,1,1,5,0,
  7,11,53,87,9,100,192,2,26,2,31,7,10,78,87,9,100,192,2,26,
  2,31,67,13,12,82,17,69,2,26,16,12,11,103,87,10,198,30,26,83,
  101,108,101,99,116,0,77,101,99,104,97,110,105,99,97,108,0,68,101,115,
  105,103,110,105,110,103,32,40,67,65,68,41,0,80,67,66,32,68,101,115,
  105,103,110,105,110,103,0,108,101,99,116,114,111,110,105,99,115,32,97,110,
  100,32,80,114,111,103,114,97,109,109,105,110,103,0,83,111,99,105,97,108,
  32,77,101,100,105,97,32,0,79,117,116,115,111,117,114,99,105,110,103,0,
  77,97,110,97,103,101,109,101,110,116,32,0,1,23,130,60,13,1,2,31,
  83,97,118,101,0 
};

struct {
  char Name[31];
  char Department[31];
  uint8_t Domian;
  uint8_t Save;
  char UID_TAG[16];
  uint8_t connect_flag;
} RemoteXY;   
#pragma pack(pop)

// Function declarations
String dayAsString(const Time::Day day);
void printToLCD(String message);
void clearAllForms();
void read_tag_default();
void checkRFID_mode1();
void read_wifi();
void combine_To_String();
void sendAttendanceToGoogleSheets(String uid, String name = "", String department = "");
void sendUserRegistrationToGoogleSheets(String data);
String urlEncode(String str);
void testGoogleSheetsConnection();
void handleSwitch();
void resetMode();
void toggleModeSelection();
void printModeLCD();
bool initLCD();
bool checkIfUserRegistered(String uid);

struct uid_details {
  String name;
  String department;
  String uid;
  String Domain;
} uid_details;

// Initialize LCD with proper ESP32-compatible settings
LiquidCrystal_I2C lcd(0x27, 20, 4);  // Address 0x27, 20 columns, 4 rows
bool lcdAvailable = false;  // Track if LCD is available

// RFID Configuration
#define RST_PIN 4
#define SS_PIN 5
MFRC522 mfrc522(SS_PIN, RST_PIN);

// RTC Configuration
namespace {
  const int kCePin = 32;
  const int kIoPin = 33;
  const int kSclkPin = 25;
  DS1302 rtc(kCePin, kIoPin, kSclkPin);
}

// Encoder Configuration
Encoder myEnc(26, 27);

// Mode Configuration
enum Mode { MODE_NONE = 0, MODE_1 = 1, MODE_2 = 2, MODE_3 = 3, MODE_4 = 4 };
const int SWITCH_PIN = 14;

// Global Variables
Mode currentMode = MODE_NONE;
Mode selectedMode = MODE_NONE;
bool modeSelected = true;  // Changed to true - start with attendance mode selected
bool switchPressed = false;
Mode lastPrintedMode = MODE_NONE;
unsigned long lastPressTime = 0;
const int doublePressDelay = 500;
bool modeLocked = false;
unsigned long modeLockStartTime = 0;
const int modeLockDuration = 1000;
long oldPosition = 0;
long previousIntervalPosition = 0;
const int interval = 15;
const int maxPosition = interval * 3;
bool previousSaveState = 0;
unsigned long clearTimeout = 0;
bool needToClear = false;
bool encoderMoved = false;  // Track if encoder has been moved since startup

void setup() {
  Serial.begin(115200);  // Increased baud rate for better performance
  delay(1000);

  // Initialize RemoteXY
  RemoteXY_Init();
  strcpy(RemoteXY.UID_TAG, "TAG UID");

  // Try to initialize LCD with error handling
  lcdAvailable = initLCD();
  
  if (lcdAvailable) {
    Serial.println("LCD initialized successfully");
  } else {
    Serial.println("LCD initialization failed - continuing without LCD");
  }

  printToLCD("Starting system...");
  
  // Initialize WiFi with improved connection handling
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  printToLCD("Connecting WiFi...");
  Serial.println("Connecting to WiFi");
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    printToLCD("WiFi connected");
    delay(1000);
    String ipMessage = "IP: " + WiFi.localIP().toString();
    printToLCD(ipMessage);
    delay(2000);
  } else {
    Serial.println("\nWiFi connection failed");
    printToLCD("WiFi failed");
    delay(2000);
  }

  // Initialize other components
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  SPI.begin();
  mfrc522.PCD_Init();
  rtc.time();

  // Print startup message - start directly in attendance mode
  printToLCD("System Ready");
  delay(1000);
  printToLCD("Attendance Mode");
  delay(1000);
  printToLCD("Scan tag for attendance");
  
  // Set initial state for attendance mode
  selectedMode = MODE_NONE;  // MODE_NONE is attendance mode
  modeSelected = true;       // Mode is selected (attendance)
}

void loop() {
  // Handle encoder position
  long newPosition = constrain(myEnc.read(), 0, maxPosition);
  myEnc.write(newPosition);

  RemoteXY_Handler();

  // Clear forms if timeout reached
  if (needToClear && millis() > clearTimeout) {
    clearAllForms();
    needToClear = false;
  }

  // Calculate current interval position
  long currentIntervalPosition = newPosition / interval;

  // Check if encoder has been moved from initial position
  if (newPosition != 0 && !encoderMoved) {
    encoderMoved = true;
    modeSelected = false;  // Allow mode selection when encoder is moved
    printToLCD("Select Mode");
    Serial.println("Encoder moved - enabling mode selection");
  }

  // Update mode if position changed and encoder has been moved
  if (currentIntervalPosition != previousIntervalPosition && encoderMoved && !modeSelected && !modeLocked) {
    currentMode = static_cast<Mode>(currentIntervalPosition + 1);
    printModeLCD();
    oldPosition = newPosition;
    previousIntervalPosition = currentIntervalPosition;
  }
  
  // Handle switch press
  handleSwitch();
  
  // Unlock mode after duration
  if (modeLocked && (millis() - modeLockStartTime >= modeLockDuration)) {
    modeLocked = false;
    printToLCD("Mode unlocked");
    delay(1000);
    printToLCD("Attendance Mode");
    delay(1000);
    printToLCD("Scan tag for attendance");
  }
  
  // Get active mode
  Mode activeMode = modeSelected ? selectedMode : currentMode;
  
  // Handle modes
  if (activeMode == MODE_NONE) {
    read_tag_default(); // This is now attendance logging mode
  }
  
  if (activeMode == MODE_1 && modeSelected) {
    checkRFID_mode1(); // This is user registration mode
    
    if (RemoteXY.Save && !previousSaveState) {
      read_wifi();
      combine_To_String();
      clearTimeout = millis() + 2000;
      needToClear = true;
    }
    previousSaveState = RemoteXY.Save;
  }
  
  delay(5);
}

// LCD Initialization with error handling
bool initLCD() {
  try {
    // Scan for I2C devices first
    Wire.begin();
    Wire.beginTransmission(0x27);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      // LCD found at address 0x27
      lcd.init();
      lcd.backlight();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LCD Test");
      delay(500);
      lcd.clear();
      return true;
    } else {
      // Try alternative address 0x3F
      Wire.beginTransmission(0x3F);
      error = Wire.endTransmission();
      
      if (error == 0) {
        // Reinitialize LCD with different address
        LiquidCrystal_I2C lcd_alt(0x3F, 20, 4);
        lcd = lcd_alt;
        lcd.init();
        lcd.backlight();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("LCD Test");
        delay(500);
        lcd.clear();
        return true;
      }
    }
  } catch (...) {
    Serial.println("LCD initialization exception caught");
  }
  
  Serial.println("No LCD found at common addresses (0x27, 0x3F)");
  return false;
}

// Helper Functions

String dayAsString(const Time::Day day) {
  switch (day) {
    case Time::kSunday: return "Sunday";
    case Time::kMonday: return "Monday";
    case Time::kTuesday: return "Tuesday";
    case Time::kWednesday: return "Wednesday";
    case Time::kThursday: return "Thursday";
    case Time::kFriday: return "Friday";
    case Time::kSaturday: return "Saturday";
  }
  return "(unknown day)";
}

void printToLCD(String message) {
  // Always print to Serial for debugging
  Serial.println("LCD: " + message);
  
  // Only attempt LCD operations if LCD is available
  if (!lcdAvailable) {
    return;
  }
  
  try {
    lcd.clear();
    if (message.length() <= 20) {
      lcd.setCursor(0, 0);
      lcd.print(message);
    } else {
      // Split message into multiple lines if needed
      int lines = (message.length() + 19) / 20;
      for (int i = 0; i < min(4, lines); i++) {
        lcd.setCursor(0, i);
        int startPos = i * 20;
        int endPos = min((int)message.length(), (i + 1) * 20);
        lcd.print(message.substring(startPos, endPos));
      }
    }
  } catch (...) {
    Serial.println("LCD operation failed - LCD may be disconnected");
    lcdAvailable = false; // Disable further LCD operations
  }
}

void clearAllForms() {
  memset(RemoteXY.Name, 0, sizeof(RemoteXY.Name));
  memset(RemoteXY.Department, 0, sizeof(RemoteXY.Department));
  RemoteXY.Domian = 0;
  strcpy(RemoteXY.UID_TAG, "TAG UID");
  
  uid_details.name = "";
  uid_details.department = "";
  uid_details.Domain = "";
  
  printToLCD("Forms cleared");
  delay(1000);
  printToLCD("Ready for next scan");
}

// Function to check if user is registered
bool checkIfUserRegistered(String uid) {
  // Check WiFi connection first
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for registration check");
    return false;
  }

  HTTPClient http;
  
  // Enable following redirects
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  // Send just the UID for checking
  String encodedUID = urlEncode(uid);
  String serverPath = String(GOOGLE_SCRIPT_URL) + "?action=check&data=" + encodedUID;
  
  Serial.println("=== REGISTRATION CHECK DEBUG ===");
  Serial.println("Checking UID: " + uid);
  Serial.println("Full URL: " + serverPath);
  
  // Begin HTTP connection
  http.begin(serverPath.c_str());
  
  // Add timeout
  http.setTimeout(10000);
  
  // Send HTTP GET request
  int httpResponseCode = http.GET();
  bool isRegistered = false;
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code for check: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println("Raw Response Length: " + String(payload.length()));
    Serial.println("Raw Response: '" + payload + "'");
    
    // Convert payload to uppercase for case-insensitive comparison
    String upperPayload = payload;
    upperPayload.toUpperCase();
    
    // Check if the response indicates user is registered
    if (upperPayload.indexOf("REGISTERED") >= 0) {
      isRegistered = true;
      Serial.println("✓ User IS registered");
    } else if (upperPayload.indexOf("NOT_REGISTERED") >= 0) {
      isRegistered = false;
      Serial.println("✗ User is NOT registered");
    } else {
      // If we get an unexpected response, assume not registered for safety
      Serial.println("⚠ Unexpected response format - assuming NOT registered");
      isRegistered = false;
    }
  } else {
    Serial.print("✗ HTTP Error checking registration: ");
    Serial.println(httpResponseCode);
    // For HTTP errors, assume not registered for safety
    Serial.println("⚠ HTTP error - assuming NOT registered");
    isRegistered = false;
  }
  
  Serial.println("=== END REGISTRATION CHECK ===");
  
  // Free resources
  http.end();
  return isRegistered;
}

// Google Sheets Integration Functions

// Function for attendance logging - now with registration check
void sendAttendanceToGoogleSheets(String uid, String name, String department) {
  Serial.println("\n=== ATTENDANCE PROCESS START ===");
  Serial.println("UID: " + uid);
  
  // Check if user is registered first
  printToLCD("Checking user...");
  bool isRegistered = checkIfUserRegistered(uid);
  
  if (!isRegistered) {
    Serial.println("❌ ATTENDANCE DENIED - User not registered");
    printToLCD("User not registered!");
    delay(2000);
    printToLCD("Please register first");
    delay(2000);
    printToLCD("Scan next tag");
    return;
  }
  
  Serial.println("✓ User is registered - proceeding with attendance");
  
  // Check WiFi connection and reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Attempting to reconnect...");
    printToLCD("WiFi reconnecting...");
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int connectionAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && connectionAttempts < 10) {
      delay(500);
      Serial.print(".");
      connectionAttempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to reconnect to WiFi");
      printToLCD("WiFi connection failed");
      return;
    } else {
      Serial.println("\nReconnected to WiFi");
      printToLCD("WiFi reconnected");
    }
  }

  HTTPClient http;
  
  // Enable following redirects
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  // Prepare data for attendance logging
  String attendanceData;
  if (name != "" && department != "") {
    attendanceData = uid + "-" + name + "-" + department;
  } else {
    attendanceData = uid; // Just UID, script will look up name
  }
  
  // URL encode the data
  String encodedData = urlEncode(attendanceData);
  String serverPath = String(GOOGLE_SCRIPT_URL) + "?action=attendance&data=" + encodedData;
  
  Serial.println("Sending attendance to: " + serverPath);
  printToLCD("Logging attendance...");
  
  // Begin HTTP connection
  http.begin(serverPath.c_str());
  
  // Add timeout
  http.setTimeout(10000);
  
  // Send HTTP GET request
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println("Attendance Response: " + payload);
    
    // Check if Google Sheets also says user is not registered (double-check)
    if (payload.indexOf("ERROR") >= 0 && payload.indexOf("not registered") >= 0) {
      Serial.println("❌ Google Sheets also reports user not registered");
      printToLCD("User not in system!");
      delay(2000);
      printToLCD("Register first");
      delay(2000);
      printToLCD("Scan next tag");
      return;
    }
    
    // Show status on LCD
    if (payload.indexOf("IN") >= 0) {
      printToLCD("✓ Checked IN");
    } else if (payload.indexOf("OUT") >= 0) {
      printToLCD("✓ Checked OUT");
    } else if (payload.indexOf("Attendance logged") >= 0) {
      printToLCD("✓ Attendance logged");
    } else {
      printToLCD("✓ Status updated");
    }
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    printToLCD("Attendance error");
  }
  
  Serial.println("=== ATTENDANCE PROCESS END ===\n");
  
  // Free resources
  http.end();
}

// Function for user registration
void sendUserRegistrationToGoogleSheets(String data) {
  // Check WiFi connection and reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Attempting to reconnect...");
    printToLCD("WiFi reconnecting...");
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int connectionAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && connectionAttempts < 10) {
      delay(500);
      Serial.print(".");
      connectionAttempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to reconnect to WiFi");
      printToLCD("WiFi connection failed");
      return;
    } else {
      Serial.println("\nReconnected to WiFi");
      printToLCD("WiFi reconnected");
    }
  }

  HTTPClient http;
  
  // Enable following redirects
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  // URL encode the data before sending
  String encodedData = urlEncode(data);
  String serverPath = String(GOOGLE_SCRIPT_URL) + "?action=register&data=" + encodedData;
  
  Serial.println("Sending user registration to: " + serverPath);
  
  // Begin HTTP connection
  http.begin(serverPath.c_str());
  
  // Send HTTP GET request
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println("Registration Response: " + payload);
    printToLCD("User registered");
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    printToLCD("Registration error");
  }
  
  // Free resources
  http.end();
}

// URL encoding function
String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isAlphaNumeric(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// RFID Functions

// Modified for attendance logging (default mode) - now with registration check
void read_tag_default() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    char hexString[3];
    sprintf(hexString, "%02X", mfrc522.uid.uidByte[i]);
    uidString += String(hexString);
  }
  
  Serial.println("Tag scanned for attendance: " + uidString);
  printToLCD("Checking registration...");
  
  // Send to attendance sheet (includes registration check)
  sendAttendanceToGoogleSheets(uidString, "", "");
  
  delay(2000);
  printToLCD("Scan next tag");
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(1000);
}

// For user registration (Mode 1)
void checkRFID_mode1() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    char hexString[3];
    sprintf(hexString, "%02X", mfrc522.uid.uidByte[i]);
    uidString += String(hexString);
  }
  
  uid_details.uid = uidString;
  memset(RemoteXY.UID_TAG, 0, sizeof(RemoteXY.UID_TAG));
  uidString.toCharArray(RemoteXY.UID_TAG, min(uidString.length() + 1, (unsigned int)sizeof(RemoteXY.UID_TAG)));
  
  printToLCD("Tag detected: " + uidString);
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

// Data Handling Functions

void read_wifi() {
  uid_details.name = String(RemoteXY.Name);
  
  switch(RemoteXY.Domian) {
    case 0: uid_details.Domain = "Select"; break;
    case 1: uid_details.Domain = "Mechanical"; break;
    case 2: uid_details.Domain = "Designing (CAD)"; break;
    case 3: uid_details.Domain = "PCB Designing"; break;
    case 4: uid_details.Domain = "Electronics and Programming"; break;
    case 5: uid_details.Domain = "Social Media"; break;
    case 6: uid_details.Domain = "Outsourcing"; break;
    case 7: uid_details.Domain = "Management"; break;
    default: uid_details.Domain = "Unknown"; break;
  }
  
  uid_details.department = String(RemoteXY.Department);
  
  printToLCD("Details received");
  delay(500);
}

// Modified for user registration
void combine_To_String() {
  Time t = rtc.time();
  String day = dayAsString(t.day);
  
  String dateString = String(t.yr) + "-" + 
                     (t.mon < 10 ? "0" : "") + String(t.mon) + "-" + 
                     (t.date < 10 ? "0" : "") + String(t.date);
  
  String timeString = (t.hr < 10 ? "0" : "") + String(t.hr) + ":" + 
                     (t.min < 10 ? "0" : "") + String(t.min) + ":" + 
                     (t.sec < 10 ? "0" : "") + String(t.sec);
  
  String outputString = uid_details.uid + "," + 
                       uid_details.name + "," + 
                       uid_details.department + "," + 
                       uid_details.Domain + "," + 
                       day + "," + 
                       dateString + "," + 
                       timeString;

  Serial.println(outputString);
  
  // Create the output string for user registration
  String outputString2 = uid_details.uid + "-" + uid_details.name + "-" + uid_details.department;
  
  Serial.println("Registering user: " + outputString2);
  sendUserRegistrationToGoogleSheets(outputString2); // Send to Users sheet
  
  printToLCD("User registered: " + uid_details.name);
  delay(1000);
  printToLCD("Registration complete");
}

// Test function for Google Sheets connection
void testGoogleSheetsConnection() {
  String testData = "TEST123-Test Name-Test Dept";
  Serial.println("Testing Google Sheets connection...");
  printToLCD("Testing connection");
  sendUserRegistrationToGoogleSheets(testData);
  delay(2000);
  sendAttendanceToGoogleSheets("TEST123", "", "");
}

// Mode Handling Functions

void handleSwitch() {
  bool currentSwitchState = (digitalRead(SWITCH_PIN) == LOW);
  
  if (currentSwitchState && !switchPressed) {
    delay(50);
    if (digitalRead(SWITCH_PIN) == LOW) {
      switchPressed = true;
      
      if (millis() - lastPressTime <= doublePressDelay) {
        resetMode();
      } else {
        toggleModeSelection();
      }
      
      lastPressTime = millis();
    }
  } else if (!currentSwitchState && switchPressed) {
    switchPressed = false;
    delay(50);
  }
}

void resetMode() {
  selectedMode = MODE_NONE;
  currentMode = MODE_NONE;
  modeSelected = true;  // Reset to attendance mode (selected)
  modeLocked = true;
  modeLockStartTime = millis();
  oldPosition = 0;
  previousIntervalPosition = 0;
  myEnc.write(0);
  encoderMoved = false;  // Reset encoder moved flag
  strcpy(RemoteXY.UID_TAG, "TAG UID");
  
  String resetMsg = "Mode reset - Attendance";
  printToLCD(resetMsg);
}

void toggleModeSelection() {
  if (!modeSelected && encoderMoved) {  // Only allow selection if encoder has been moved
    selectedMode = currentMode;
    modeSelected = true;
    
    String modeMsg = "Mode " + String(selectedMode) + " selected!";
    printToLCD(modeMsg);
    
    if (selectedMode == MODE_1) {
      delay(1500);
      printToLCD("Registration Mode");
      delay(1000);
      printToLCD("Scan RFID Tag");
      strcpy(RemoteXY.UID_TAG, "TAG UID");
    }
  } else if (modeSelected) {
    selectedMode = MODE_NONE;
    modeSelected = false;
    oldPosition = 0;
    previousIntervalPosition = 0;
    myEnc.write(0);
    encoderMoved = false;  // Reset encoder moved flag
    strcpy(RemoteXY.UID_TAG, "TAG UID");
    
    printToLCD("Mode selection canceled");
    printModeLCD();
  }
}

void printModeLCD() {
  Mode modeToPrint = modeSelected ? selectedMode : currentMode;
  
  if (modeToPrint != lastPrintedMode) {
    String modeString = "Mode: ";
    
    switch (modeToPrint) {
      case MODE_NONE: modeString += "NONE (Attendance)"; break;
      case MODE_1: modeString += "1 (Registration)"; break;
      case MODE_2: modeString += "2"; break;
      case MODE_3: modeString += "3"; break;
      case MODE_4: modeString += "4"; break;
      default: modeString += "UNKNOWN"; break;
    }
    
    Serial.println(modeString);
    printToLCD(modeString);
    lastPrintedMode = modeToPrint;
  }
}