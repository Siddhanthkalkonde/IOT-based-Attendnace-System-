# ESP32 RFID Attendance System

A comprehensive RFID-based attendance management system built with ESP32 that integrates with Google Sheets for data storage and RemoteXY for mobile app control.

## 🌟 Features

- **Dual Mode Operation**: Attendance logging and user registration modes
- **RFID Integration**: Support for MFRC522 RFID reader
- **Google Sheets Integration**: Automatic data synchronization with cloud storage
- **LCD Display Support**: 20x4 I2C LCD with optional operation (works without LCD)
- **Mobile App Control**: RemoteXY-based mobile interface for user registration
- **WiFi Connectivity**: Automatic reconnection and status monitoring
- **Encoder Navigation**: Rotary encoder for mode selection
- **Real-time Clock**: DS1302 RTC for accurate timestamping
- **Registration Validation**: Ensures only registered users can log attendance

## 🛠️ Hardware Requirements

### Essential Components
- **ESP32 Development Board** (ESP32-WROOM-32 or similar)
- **MFRC522 RFID Reader Module**
- **DS1302 Real Time Clock Module**
- **Rotary Encoder** (with push button)
- **Push Button** (for mode switching)
- **RFID Cards/Tags** (13.56MHz)

### Optional Components
- **20x4 I2C LCD Display** (Address: 0x27 or 0x3F)
- **Breadboard/PCB** for connections
- **Jumper Wires**
- **Power Supply** (5V/3.3V)

## 🔌 Wiring Diagram

### MFRC522 RFID Module
| MFRC522 Pin | ESP32 Pin |
|-------------|-----------|
| VCC         | 3.3V      |
| GND         | GND       |
| RST         | GPIO 4    |
| SDA (SS)    | GPIO 5    |
| MOSI        | GPIO 23   |
| MISO        | GPIO 19   |
| SCK         | GPIO 18   |

### DS1302 RTC Module
| DS1302 Pin | ESP32 Pin |
|------------|-----------|
| VCC        | 3.3V      |
| GND        | GND       |
| CE         | GPIO 32   |
| IO         | GPIO 33   |
| SCLK       | GPIO 25   |

### Rotary Encoder
| Encoder Pin | ESP32 Pin |
|-------------|-----------|
| CLK         | GPIO 26   |
| DT          | GPIO 27   |
| SW          | GPIO 14   |
| VCC         | 3.3V      |
| GND         | GND       |

### I2C LCD (Optional)
| LCD Pin | ESP32 Pin |
|---------|-----------|
| VCC     | 5V        |
| GND     | GND       |
| SDA     | GPIO 21   |
| SCL     | GPIO 22   |

## 📚 Software Requirements

### Arduino IDE Libraries
Install the following libraries through Arduino IDE Library Manager:

```
1. MFRC522 by GithubCommunity (v1.4.10+)
2. DS1302 by Timur Maksimov (v1.3.0+)
3. Encoder by Paul Stoffregen (v1.4.2+)
4. LiquidCrystal_I2C by Frank de Brabander (v1.1.2+)
5. WiFi (ESP32 built-in)
6. HTTPClient (ESP32 built-in)
7. RemoteXY (v3.1.8+)
```

### Installation Steps
1. Open Arduino IDE
2. Go to **Tools > Manage Libraries**
3. Search and install each library listed above
4. Ensure ESP32 board package is installed

## 📱 RemoteXY Mobile App Configuration

### 1. Download RemoteXY App
- use the zip file provided for gui configuration , and the code provided 
- **Android**: [Google Play Store](https://play.google.com/store/apps/details?id=com.smartprojects.remotexy)
- **iOS**: [App Store](https://apps.apple.com/app/remotexy/id1012876026)
- **How to Connect to GUI** - open app > go to add icon or '+' icon > select wifi Access Point > search for "Volta Attendnace" or any such AP 

### 2. Connect to ESP32
1. Power on your ESP32 device
2. Open RemoteXY app on your phone
3. Look for WiFi network: **"VOLTA ATTENDANCE"**
4. Connect to this network (no password required)
5. The app will automatically detect the ESP32

### 3. App Interface
The mobile app provides:
- **Name Field**: Enter user's full name
- **Department Field**: Enter user's department
- **Domain Dropdown**: Select from available domains:
  - Mechanical
  - Designing (CAD)
  - PCB Designing
  - Electronics and Programming
  - Social Media
  - Outsourcing
  - Management
- **UID Display**: Shows scanned RFID tag UID
- **Save Button**: Register the user in the system

## ☁️ Google Sheets Integration

### 1. Create Google Sheets Document
1. Go to [Google Sheets](https://sheets.google.com)
2. Create a new spreadsheet
3. Name it "Attendance System" or similar

### 2. Deploy Google Apps Script

#### Step 1: Open Apps Script
1. In your Google Sheet, go to **Extensions > Apps Script**
2. Delete any existing code in the editor

#### Step 2: Add the Script
Copy and paste the following Google Apps Script code:

```javascript
function doGet(e) {
  try {
    const data = e.parameter.data;
    const action = e.parameter.action || 'register';
    
    if (!data) {
      return ContentService.createTextOutput('No data received');
    }
    
    console.log('Received data:', data);
    console.log('Action:', action);
    
    let result;
    if (action === 'attendance') {
      result = logAttendance(data);
    } else if (action === 'check') {
      result = checkUserRegistration(data);
    } else {
      result = registerUser(data);
    }
    
    return ContentService.createTextOutput(result);
    
  } catch (error) {
    console.error('Error in doGet:', error);
    return ContentService.createTextOutput('Error: ' + error.toString());
  }
}

function checkUserRegistration(data) {
  try {
    let uid = data;
    if (data.includes('-')) {
      uid = data.split('-')[0];
    }
    
    console.log('Checking registration for UID:', uid);
    
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    let userSheet = ss.getSheetByName('Users') || ss.getSheetByName('Datalogger') || ss.getSheetByName('Sheet1');
    
    if (!userSheet) {
      console.log('No user sheet found');
      return 'NOT_REGISTERED';
    }
    
    const userData = userSheet.getDataRange().getValues();
    console.log('Searching in', userData.length - 1, 'user records');
    
    for (let i = 1; i < userData.length; i++) {
      if (!userData[i] || userData[i].length === 0) continue;
      
      const rowUID = userData[i][0] ? userData[i][0].toString().trim() : '';
      if (rowUID === uid) {
        const userName = userData[i][1] ? userData[i][1].toString().trim() : 'Unknown';
        console.log('User found:', userName);
        return 'REGISTERED: ' + userName;
      }
    }
    
    console.log('User not found');
    return 'NOT_REGISTERED';
    
  } catch (error) {
    console.error('Error in checkUserRegistration:', error);
    return 'ERROR: ' + error.toString();
  }
}

function registerUser(data) {
  try {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    let userSheet = ss.getSheetByName('Users') || ss.getSheetByName('Datalogger') || ss.getSheetByName('Sheet1');
    
    if (!userSheet) {
      userSheet = ss.insertSheet('Users');
      userSheet.getRange(1, 1, 1, 7).setValues([['UID', 'Name', 'Department', 'Domain', 'Day', 'Date', 'Time']]);
    } else if (userSheet.getRange(1, 1).getValue() !== 'UID') {
      userSheet.getRange(1, 1, 1, 7).setValues([['UID', 'Name', 'Department', 'Domain', 'Day', 'Date', 'Time']]);
    }
    
    const parts = data.split('-');
    if (parts.length !== 3) {
      throw new Error('Invalid data format. Expected: uid-name-department');
    }
    
    const uid = parts[0];
    const name = parts[1];
    const department = parts[2];
    
    const existingData = userSheet.getDataRange().getValues();
    for (let i = 1; i < existingData.length; i++) {
      if (existingData[i][0] === uid) {
        return 'User already registered with UID: ' + uid;
      }
    }
    
    const now = new Date();
    const day = Utilities.formatDate(now, Session.getScriptTimeZone(), 'EEEE');
    const date = Utilities.formatDate(now, Session.getScriptTimeZone(), 'yyyy-MM-dd');
    const time = Utilities.formatDate(now, Session.getScriptTimeZone(), 'HH:mm:ss');
    
    userSheet.appendRow([uid, name, department, '', day, date, time]);
    
    console.log('User registered successfully:', uid, name, department);
    return 'User registered successfully: ' + name;
    
  } catch (error) {
    console.error('Error in registerUser:', error);
    throw error;
  }
}

function logAttendance(data) {
  try {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    let attendanceSheet = ss.getSheetByName('Attendance');
    let userSheet = ss.getSheetByName('Users') || ss.getSheetByName('Datalogger') || ss.getSheetByName('Sheet1');
    
    if (!attendanceSheet) {
      attendanceSheet = ss.insertSheet('Attendance');
      attendanceSheet.getRange(1, 1, 1, 6).setValues([['Date', 'Time', 'UID', 'Name', 'Department', 'Status']]);
    }
    
    if (!userSheet) {
      userSheet = ss.insertSheet('Users');
      userSheet.getRange(1, 1, 1, 7).setValues([['UID', 'Name', 'Department', 'Domain', 'Day', 'Date', 'Time']]);
    }
    
    let uid, name = 'Unknown', department = 'Unknown';
    let userFound = false;
    
    if (data.includes('-')) {
      const parts = data.split('-');
      uid = parts[0];
      if (parts.length >= 2) name = parts[1];
      if (parts.length >= 3) department = parts[2];
    } else {
      uid = data;
    }
    
    console.log('Attempting to log attendance for UID:', uid);
    
    if (userSheet) {
      const userData = userSheet.getDataRange().getValues();
      console.log('Looking up UID in Users sheet:', uid);
      
      for (let i = 1; i < userData.length; i++) {
        if (!userData[i] || userData[i].length === 0) continue;
        
        const rowUID = userData[i][0] ? userData[i][0].toString().trim() : '';
        if (rowUID === uid) {
          name = userData[i][1] ? userData[i][1].toString().trim() : 'Unknown';
          department = userData[i][2] ? userData[i][2].toString().trim() : 'Unknown';
          userFound = true;
          console.log('✓ User found in Users sheet:', name, department);
          break;
        }
      }
    }
    
    if (!userFound) {
      console.log('❌ User not found in Users sheet - attendance denied');
      return 'ERROR: User not registered. UID: ' + uid + ' not found in Users sheet.';
    }
    
    console.log('✓ User is registered, proceeding with attendance logging');
    
    const attendanceData = attendanceSheet.getDataRange().getValues();
    let status = 'IN';
    
    for (let i = attendanceData.length - 1; i >= 1; i--) {
      if (attendanceData[i][2] === uid) {
        status = (attendanceData[i][5] === 'IN') ? 'OUT' : 'IN';
        break;
      }
    }
    
    const now = new Date();
    const date = Utilities.formatDate(now, Session.getScriptTimeZone(), 'yyyy-MM-dd');
    const time = Utilities.formatDate(now, Session.getScriptTimeZone(), 'HH:mm:ss');
    
    attendanceSheet.appendRow([date, time, uid, name, department, status]);
    
    console.log('✓ Attendance logged successfully:', uid, name, status);
    return 'Attendance logged: ' + name + ' - ' + status;
    
  } catch (error) {
    console.error('Error in logAttendance:', error);
    return 'ERROR: ' + error.toString();
  }
}
```

#### Step 3: Deploy the Script
1. Click **Deploy > New deployment**
2. Choose **Type**: Web app
3. Set **Execute as**: Me
4. Set **Who has access**: Anyone
5. Click **Deploy**
6. **Copy the deployment URL** - you'll need this for the ESP32 code

#### Step 4: Grant Permissions
1. Click **Authorize access**
2. Choose your Google account
3. Click **Advanced** if you see a warning
4. Click **Go to [Your Project Name] (unsafe)**
5. Click **Allow**

## ⚙️ ESP32 Configuration

### 1. WiFi Settings
Open the ESP32 code and modify these lines:

```cpp
// WiFi credentials - REPLACE WITH YOUR VALUES
const char* WIFI_SSID = "Your_WiFi_Network_Name";
const char* WIFI_PASSWORD = "Your_WiFi_Password";
```

**Example:**
```cpp
const char* WIFI_SSID = "HomeWiFi";
const char* WIFI_PASSWORD = "mypassword123";
```

### 2. Google Sheets URL
Replace the Google Apps Script URL:

```cpp
// Google Sheets configuration - REPLACE WITH YOUR SCRIPT URL
const char* GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/YOUR_DEPLOYMENT_ID/exec";
```

### 3. RemoteXY Configuration (Optional)
To change the RemoteXY WiFi network name:

```cpp
#define REMOTEXY_WIFI_SSID "VOLTA ATTENDANCE"  // Change this name
#define REMOTEXY_WIFI_PASSWORD ""              // Add password if needed
```

## 🚀 Quick Start Guide

### 1. Hardware Setup
1. Connect all components according to the wiring diagram
2. Double-check all connections
3. Power the ESP32 via USB

### 2. Software Setup
1. Install required Arduino libraries
2. Configure WiFi credentials in the code
3. Deploy Google Apps Script and get the URL
4. Update the Google Script URL in ESP32 code
5. Upload code to ESP32

### 3. First Run
1. Power on the system
2. Wait for WiFi connection (check Serial Monitor)
3. System starts in **Attendance Mode** by default
4. Ready to scan RFID tags!

### 4. Register Users
1. Turn the rotary encoder to select **Mode 1 (Registration)**
2. Press the encoder button to confirm
3. Connect to "VOLTA ATTENDANCE" WiFi with your phone
4. Open RemoteXY app
5. Scan RFID tag → Enter user details → Save

### 5. Log Attendance
1. Return to **Mode NONE (Attendance)** by double-pressing encoder button
2. Scan registered RFID tags
3. System will show IN/OUT status
4. Check Google Sheets for logged data

## 🔧 Troubleshooting

### WiFi Connection Issues
- Check SSID and password spelling
- Ensure WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Try moving closer to the router

### LCD Not Working
- System works without LCD (all messages go to Serial Monitor)
- Check I2C address (try 0x27 or 0x3F)
- Verify wiring connections

### Google Sheets Not Updating
- Verify the deployment URL is correct
- Check Google Apps Script permissions
- Monitor Serial output for HTTP error codes

### RFID Not Reading
- Check SPI connections
- Ensure RFID cards are 13.56MHz compatible
- Try different RFID cards

### RemoteXY App Issues
- Ensure ESP32 WiFi hotspot is active
- Check phone WiFi settings
- Restart ESP32 if needed

## 📊 System Operation

### Mode Selection
- **Mode NONE**: Default attendance logging mode
- **Mode 1**: User registration mode
- **Mode 2-4**: Reserved for future features

### Attendance Logic
- First scan: **IN**
- Second scan: **OUT**  
- Third scan: **IN** (toggles automatically)

### Data Storage
- **Users Sheet**: Stores registered user information
- **Attendance Sheet**: Logs all attendance records with timestamps

## 🔒 Security Features

- **Registration Validation**: Only registered users can log attendance
- **Double-Check System**: Both ESP32 and Google Sheets verify user registration
- **Error Handling**: Graceful failure with informative messages
- **WiFi Security**: RemoteXY operates on isolated network

## 📝 License

This project is open source and available under the [MIT License](LICENSE).

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📞 Support

For issues and questions:
1. Check the troubleshooting section
2. Review Serial Monitor output
3. Create an issue on GitHub with detailed information

## 🆕 Version History

- **v1.0.0**: Initial release with basic attendance functionality
- **v1.1.0**: Added registration validation and error handling
- **v1.2.0**: Enhanced LCD support with optional operation
- **v1.3.0**: Improved WiFi connectivity and RemoteXY integration

---

**Happy Tracking! 🎯**
