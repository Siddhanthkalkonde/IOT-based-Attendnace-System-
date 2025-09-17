function doGet(e) {
  try {
    const data = e.parameter.data;
    const action = e.parameter.action || 'register'; // Default to register for backward compatibility
    
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

// NEW FUNCTION: Check if user is registered
function checkUserRegistration(data) {
  try {
    // Extract UID from data (format could be "UID-check-check" or just "UID")
    let uid = data;
    if (data.includes('-')) {
      uid = data.split('-')[0];
    }
    
    console.log('Checking registration for UID:', uid);
    
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    // Try to find existing user sheet with any of these names
    let userSheet = ss.getSheetByName('Users') || ss.getSheetByName('Datalogger') || ss.getSheetByName('Sheet1');
    
    if (!userSheet) {
      console.log('No user sheet found');
      return 'NOT_REGISTERED';
    }
    
    const userData = userSheet.getDataRange().getValues();
    console.log('Searching in', userData.length - 1, 'user records');
    
    // Check if UID exists in the first column
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
    // Try to find existing sheet with any of these names
    let userSheet = ss.getSheetByName('Users') || ss.getSheetByName('Datalogger') || ss.getSheetByName('Sheet1');
    
    // Create Users sheet if none exist
    if (!userSheet) {
      userSheet = ss.insertSheet('Users');
      // Add headers
      userSheet.getRange(1, 1, 1, 7).setValues([['UID', 'Name', 'Department', 'Domain', 'Day', 'Date', 'Time']]);
    } else if (userSheet.getRange(1, 1).getValue() !== 'UID') {
      // Add headers if they don't exist
      userSheet.getRange(1, 1, 1, 7).setValues([['UID', 'Name', 'Department', 'Domain', 'Day', 'Date', 'Time']]);
    }
    
    const parts = data.split('-');
    if (parts.length !== 3) {
      throw new Error('Invalid data format. Expected: uid-name-department');
    }
    
    const uid = parts[0];
    const name = parts[1];
    const department = parts[2];
    
    // Check if user already exists
    const existingData = userSheet.getDataRange().getValues();
    for (let i = 1; i < existingData.length; i++) {
      if (existingData[i][0] === uid) {
        return 'User already registered with UID: ' + uid;
      }
    }
    
    // Add timestamp
    const now = new Date();
    const day = Utilities.formatDate(now, Session.getScriptTimeZone(), 'EEEE');
    const date = Utilities.formatDate(now, Session.getScriptTimeZone(), 'yyyy-MM-dd');
    const time = Utilities.formatDate(now, Session.getScriptTimeZone(), 'HH:mm:ss');
    
    // Add new user
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
    // Try to find existing user sheet with any of these names
    let userSheet = ss.getSheetByName('Users') || ss.getSheetByName('Datalogger') || ss.getSheetByName('Sheet1');
    
    // Create Attendance sheet if it doesn't exist
    if (!attendanceSheet) {
      attendanceSheet = ss.insertSheet('Attendance');
      // Add headers
      attendanceSheet.getRange(1, 1, 1, 6).setValues([['Date', 'Time', 'UID', 'Name', 'Department', 'Status']]);
    }
    
    // Create Users sheet if it doesn't exist
    if (!userSheet) {
      userSheet = ss.insertSheet('Users');
      userSheet.getRange(1, 1, 1, 7).setValues([['UID', 'Name', 'Department', 'Domain', 'Day', 'Date', 'Time']]);
    }
    
    // Data can be just UID or UID-name-department format
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
    
    // FIRST: Check if user is registered in Users sheet
    if (userSheet) {
      const userData = userSheet.getDataRange().getValues();
      console.log('Looking up UID in Users sheet:', uid);
      
      for (let i = 1; i < userData.length; i++) {
        if (!userData[i] || userData[i].length === 0) continue;
        
        // Method 1: Check if UID is in first column
        const rowUID = userData[i][0] ? userData[i][0].toString().trim() : '';
        if (rowUID === uid) {
          name = userData[i][1] ? userData[i][1].toString().trim() : 'Unknown';
          department = userData[i][2] ? userData[i][2].toString().trim() : 'Unknown';
          userFound = true;
          console.log('✓ User found in Users sheet:', name, department);
          break;
        }
        
        // Method 2: Check all columns for the UID-name-department format
        for (let j = 0; j < userData[i].length; j++) {
          const cellData = userData[i][j] ? userData[i][j].toString() : '';
          if (cellData.startsWith(uid + '-')) {
            const parts = cellData.split('-');
            if (parts.length >= 3) {
              name = parts[1] || 'Unknown';
              department = parts[2] || 'Unknown';
              userFound = true;
              console.log('✓ User found in formatted data:', name, department);
              break;
            }
          }
        }
        
        if (userFound) break;
      }
    }
    
    // If user is not found in Users sheet, deny attendance
    if (!userFound) {
      console.log('❌ User not found in Users sheet - attendance denied');
      return 'ERROR: User not registered. UID: ' + uid + ' not found in Users sheet.';
    }
    
    // User is registered, proceed with attendance logging
    console.log('✓ User is registered, proceeding with attendance logging');
    
    // Determine IN/OUT status based on last entry
    const attendanceData = attendanceSheet.getDataRange().getValues();
    let status = 'IN';
    
    // Check last entry for this UID
    for (let i = attendanceData.length - 1; i >= 1; i--) {
      if (attendanceData[i][2] === uid) {
        // If last entry was IN, make this OUT, and vice versa
        status = (attendanceData[i][5] === 'IN') ? 'OUT' : 'IN';
        break;
      }
    }
    
    // Add timestamp
    const now = new Date();
    const date = Utilities.formatDate(now, Session.getScriptTimeZone(), 'yyyy-MM-dd');
    const time = Utilities.formatDate(now, Session.getScriptTimeZone(), 'HH:mm:ss');
    
    // Log attendance
    attendanceSheet.appendRow([date, time, uid, name, department, status]);
    
    console.log('✓ Attendance logged successfully:', uid, name, status);
    return 'Attendance logged: ' + name + ' - ' + status;
    
  } catch (error) {
    console.error('Error in logAttendance:', error);
    return 'ERROR: ' + error.toString();
  }
}

// Helper function to check if user exists (kept for backward compatibility)
function userExists(uid) {
  try {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const userSheet = ss.getSheetByName('Users');
    
    if (!userSheet) return false;
    
    const userData = userSheet.getDataRange().getValues();
    for (let i = 1; i < userData.length; i++) {
      if (userData[i][0] === uid) {
        return true;
      }
    }
    return false;
  } catch (error) {
    console.error('Error in userExists:', error);
    return false;
  }
}
