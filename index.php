<?php
// index.php - Combined Web Dashboard (Frontend) and API (Backend)

// --- Configuration ---
// IMPORTANT: Replace with the ACTUAL IP address of your ESP32 module.
// You_can_find_your_ESP32's_IP_address_from_your_router's_connected_devices_list,
// or from the ESP32's serial monitor output when it connects to Wi-Fi.
$esp32_ip = "192.168.68.110"; // This is the IP of your ESP32 module itself. CONFIRM THIS IS STILL YOUR ESP32'S IP
$esp32_base_url = "http://{$esp32_ip}";

// --- File Paths for Data Storage (relative to this index.php file) ---
$arduino_status_file = 'arduino_status.txt';
$attendance_log_file = 'attendance_log.csv';
$esp32_inbox_log_file = 'esp32_inbox.log'; // For debugging raw ESP32 POSTs

// --- Error Reporting for Debugging ---
// **** IMPORTANT: KEEP THESE ON WHILE DEBUGGING. REMOVE IN PRODUCTION ****
error_reporting(E_ALL);
ini_set('display_errors', 1);

// --- Headers for API Endpoints ---
// This ensures your JavaScript can fetch data from this file.
// Only apply these if it's an AJAX request, not a full page load.
if (isset($_SERVER['HTTP_X_REQUESTED_WITH']) && strtolower($_SERVER['HTTP_X_REQUESTED_WITH']) == 'xmlhttprequest') {
    header('Content-Type: application/json');
    header('Access-Control-Allow-Origin: *'); // Allow requests from any origin (for local development)
    header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
}

// --- Backend Logic: Handle Incoming Requests ---

// Handle POST requests from ESP32
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $rawData = file_get_contents('php://input');
    parse_str($rawData, $postData); // Parse x-www-form-urlencoded data

    // --- DEBUGGING START ---
    // Output all received POST data directly to the browser/client for immediate inspection
    // This will appear on your ESP32 Serial Monitor if the ESP32 reads the response,
    // or if you open the index.php directly and send a POST.
    echo "<pre>--- DEBUGGING PHP POST RECEIPT ---\n";
    echo "Raw Data Received: " . htmlspecialchars($rawData) . "\n";
    echo "Parsed POST Data (postData array):\n";
    print_r($postData);
    echo "-----------------------------------\n</pre>";
    // --- DEBUGGING END ---

    // Log all incoming data for debugging to a file
    // Check if esp32_inbox.log file is writable
    if (is_writable(dirname(__FILE__))) { // Check if the directory is writable
        file_put_contents($esp32_inbox_log_file, date('Y-m-d H:i:s') . " - Received: " . $rawData . " | Parsed: " . print_r($postData, true) . "\n", FILE_APPEND | LOCK_EX);
    } else {
        error_log("CRITICAL ERROR: Directory '" . dirname(__FILE__) . "' is not writable for esp32_inbox.log. Check permissions.");
        // Consider echoing an error here to the ESP32 if this is a critical logging failure
    }


    if (isset($postData['arduino_response'])) { // Updated from 'data' to 'arduino_response' as per ESP32 code
        $receivedData = trim($postData['arduino_response']); // Trim whitespace

        if (strpos($receivedData, 'arduino_status:') === 0) {
            $status = substr($receivedData, strlen('arduino_status:'));
            file_put_contents($arduino_status_file, $status, LOCK_EX);
            echo json_encode(['success' => true, 'message' => 'Status updated']);
        } else if (strpos($receivedData, 'enrollment_success:') === 0 || strpos($receivedData, 'enrollment_failed:') === 0) {
            file_put_contents($arduino_status_file, $receivedData, LOCK_EX);
            echo json_encode(['success' => true, 'message' => 'Enrollment result received']);
        } else if (strpos($receivedData, 'deleted_success:') === 0 || strpos($receivedData, 'deleted_failed:') === 0) {
            file_put_contents($arduino_status_file, $receivedData, LOCK_EX);
            echo json_encode(['success' => true, 'message' => 'Deletion result received']);
        } else if (strpos($receivedData, 'fingerprint_list_trigger_ok:') === 0 || strpos($receivedData, 'arduino_error:List_Request_Failed:') === 0) {
            file_put_contents($arduino_status_file, $receivedData, LOCK_EX);
            echo json_encode(['success' => true, 'message' => 'List command processed confirmation received']);
        } else if (strpos($receivedData, 'arduino_error:') === 0) {
            file_put_contents($arduino_status_file, $receivedData, LOCK_EX);
            echo json_encode(['success' => true, 'message' => 'Arduino error received']);
        } else {
            // Log unrecognized arduino_response data for debugging
            file_put_contents($esp32_inbox_log_file, date('Y-m-d H:i:s') . " - Unrecognized arduino_response: " . $receivedData . "\n", FILE_APPEND | LOCK_EX);
            echo json_encode(['success' => false, 'message' => 'Unrecognized arduino_response data received']);
        }
    } else if (isset($postData['id']) && isset($postData['timestamp'])) { // Handle attendance directly
        $id = $postData['id'];
        $timestamp = $postData['timestamp'];
        $logEntry = $timestamp . "," . $id . "\n";

        // Check if the file is writable *before* attempting to write
        if (!file_exists($attendance_log_file) && !is_writable(dirname($attendance_log_file))) {
            error_log("CRITICAL ERROR: Directory '" . dirname($attendance_log_file) . "' is not writable for {$attendance_log_file}. Cannot create file. Check permissions.");
            echo json_encode(['success' => false, 'message' => 'Failed to log attendance: Directory not writable.']);
        } else if (file_exists($attendance_log_file) && !is_writable($attendance_log_file)) {
            error_log("CRITICAL ERROR: Attendance log file '{$attendance_log_file}' is not writable. Check permissions.");
            echo json_encode(['success' => false, 'message' => 'Failed to log attendance: File not writable.']);
        } else {
            if (file_put_contents($attendance_log_file, $logEntry, FILE_APPEND | LOCK_EX) !== false) {
                echo json_encode(['success' => true, 'message' => 'Attendance logged']);
            } else {
                // This error_log will go to your Apache/Nginx/PHP error log (e.g., C:\xampp\apache\logs\error.log)
                error_log("Failed to write attendance to {$attendance_log_file}. This usually indicates a file permission issue. Check permissions.");
                echo json_encode(['success' => false, 'message' => 'Failed to log attendance on server. Check permissions.']);
            }
        }
    } else {
        echo json_encode(['success' => false, 'message' => 'No recognized data received from ESP32']);
    }
    exit(); // Stop execution after handling POST
}


// Handle GET requests (from Dashboard AJAX or direct browser access)
if ($_SERVER['REQUEST_METHOD'] === 'GET' && isset($_GET['action'])) {
    $action = $_GET['action'];
    $id = $_GET['id'] ?? null;

    switch ($action) {
        case 'enroll':
            if ($id) {
                $command_url = "{$esp32_base_url}/enroll?id={$id}";
                $response = @file_get_contents($command_url);
                if ($response !== false) {
                    echo json_encode(['success' => true, 'message' => "Command sent to ESP32: {$command_url}", 'esp_response' => $response]);
                } else {
                    echo json_encode(['success' => false, 'message' => "Failed to send command to ESP32. Is ESP32 online and reachable at {$esp32_ip}?", 'error_detail' => error_get_last()]);
                }
            } else {
                echo json_encode(['success' => false, 'message' => 'ID is required for enrollment']);
            }
            break;

        case 'delete':
            if ($id) {
                $command_url = "{$esp32_base_url}/delete?id={$id}";
                $response = @file_get_contents($command_url);
                if ($response !== false) {
                    echo json_encode(['success' => true, 'message' => "Command sent to ESP32: {$command_url}", 'esp_response' => $response]);
                } else {
                    echo json_encode(['success' => false, 'message' => "Failed to send command to ESP32. Is ESP32 online and reachable at {$esp32_ip}?", 'error_detail' => error_get_last()]);
                }
            } else {
                echo json_encode(['success' => false, 'message' => 'ID is required for deletion']);
            }
            break;

        case 'get_list':
            $command_url = "{$esp32_base_url}/get_list";
            $response = @file_get_contents($command_url);
            echo json_encode(['success' => true, 'message' => 'List refresh command sent to ESP32.', 'esp_trigger_response' => $response]);
            break;

        case 'get_status':
            $status = file_exists($arduino_status_file) ? file_get_contents($arduino_status_file) : 'No status received yet.';
            echo json_encode(['success' => true, 'status' => trim($status)]);
            break;

        case 'get_attendance':
            $attendance = file_exists($attendance_log_file) ? file_get_contents($attendance_log_file) : '';
            $lines = explode("\n", trim($attendance));
            $data = [];
            foreach ($lines as $line) {
                if (!empty($line)) {
                    $parts = explode(',', $line);
                    if (count($parts) >= 2) {
                        $data[] = [$parts[0], $parts[1]]; // [timestamp, id]
                    }
                }
            }
            echo json_encode(['success' => true, 'attendance' => $data]);
            break;

        case 'clear_attendance':
            if (file_exists($attendance_log_file)) {
                // Check if the file is writable before clearing
                if (!is_writable($attendance_log_file)) {
                    error_log("CRITICAL ERROR: Attendance log file '{$attendance_log_file}' is not writable. Cannot clear. Check permissions.");
                    echo json_encode(['success' => false, 'message' => 'Failed to clear attendance log: File not writable. Check permissions.']);
                } else if (file_put_contents($attendance_log_file, '', LOCK_EX) !== false) {
                    echo json_encode(['success' => true, 'message' => 'Attendance log cleared.']);
                } else {
                    error_log("Failed to clear attendance log: {$attendance_log_file}. Check permissions.");
                    echo json_encode(['success' => false, 'message' => 'Failed to clear attendance log on server. Check permissions.']);
                }
            } else {
                echo json_encode(['success' => true, 'message' => 'Attendance log file does not exist, nothing to clear.']);
            }
            break;

        default:
            echo json_encode(['success' => false, 'message' => 'Invalid action']);
            break;
    }
    exit(); // Stop execution after handling GET AJAX
}

// --- Frontend HTML, CSS, JavaScript ---
// This part is displayed when the index.php is accessed directly in a browser.
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Fingerprint Biometrics Dashboard</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: #f0f2f5;
            margin: 0;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: flex-start;
            min-height: 100vh;
        }
        .container {
            background-color: #ffffff;
            padding: 30px;
            border-radius: 15px;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1);
            width: 100%;
            max-width: 900px;
            box-sizing: border-box;
        }
        h1, h2 {
            color: #333;
            text-align: center;
            margin-bottom: 25px;
        }
        h1 {
            font-size: 2.5em;
            color: #2c3e50;
        }
        h2 {
            font-size: 1.8em;
            color: #34495e;
            border-bottom: 1px solid #eee;
            padding-bottom: 10px;
            margin-bottom: 20px;
        }
        .grid-container {
            display: grid;
            grid-template-columns: 1fr;
            gap: 25px;
            margin-bottom: 30px;
        }
        @media (min-width: 768px) {
            .grid-container {
                grid-template-columns: 1fr 1fr;
            }
        }
        .card {
            background-color: #f9f9f9;
            padding: 20px;
            border-radius: 10px;
            border: 1px solid #e0e0e0;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.05);
        }
        .input-group {
            display: flex;
            flex-direction: column;
            gap: 10px;
            margin-bottom: 15px;
        }
        .input-group label {
            font-weight: bold;
            color: #555;
            font-size: 0.9em;
        }
        .input-group input[type="number"] {
            padding: 12px;
            border: 1px solid #ccc;
            border-radius: 8px;
            font-size: 1em;
            width: 100%;
            box-sizing: border-box;
        }
        .btn {
            background-color: #007bff;
            color: white;
            padding: 12px 20px;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-size: 1em;
            transition: background-color 0.3s ease;
            width: 100%;
            box-sizing: border-box;
        }
        .btn:hover {
            background-color: #0056b3;
        }
        .btn-enroll { background-color: #28a745; }
        .btn-enroll:hover { background-color: #218838; }
        .btn-delete { background-color: #dc3545; }
        .btn-delete:hover { background-color: #c82333; }
        .btn-refresh {
            background-color: #6c757d;
            margin-top: 10px;
        }
        .btn-refresh:hover {
            background-color: #5a6268;
        }
        .btn-clear {
            background-color: #ffc107;
            color: #333;
            margin-top: 10px;
        }
        .btn-clear:hover {
            background-color: #e0a800;
        }
        .status-box {
            background-color: #e9f7ef;
            color: #218838;
            padding: 15px;
            border-radius: 10px;
            border: 1px solid #28a745;
            margin-bottom: 25px;
            font-size: 1.1em;
            font-weight: bold;
            text-align: center;
        }
        .list-box {
            background-color: #f0f8ff;
            border: 1px solid #a8d1ff;
            padding: 15px;
            border-radius: 10px;
            max-height: 200px;
            overflow-y: auto;
            margin-top: 15px;
        }
        .list-box ul {
            list-style-type: none;
            padding: 0;
            margin: 0;
        }
        .list-box li {
            background-color: #e3f2fd;
            margin-bottom: 8px;
            padding: 10px;
            border-radius: 5px;
            font-weight: bold;
            color: #0d47a1;
        }
        .message-box {
            padding: 15px;
            margin-bottom: 20px;
            border-radius: 10px;
            font-weight: bold;
            text-align: center;
        }
        .message-success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .message-error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .message-info {
            background-color: #d1ecf1;
            color: #0c5460;
            border: 1px solid #bee5eb;
        }
        .attendance-log {
            background-color: #fff3e0;
            border: 1px solid #ffcc80;
            padding: 15px;
            border-radius: 10px;
            max-height: 300px;
            overflow-y: auto;
            margin-top: 20px;
        }
        .attendance-log table {
            width: 100%;
            border-collapse: collapse;
            font-size: 0.9em;
        }
        .attendance-log th, .attendance-log td {
            border: 1px solid #ffb74d;
            padding: 8px;
            text-align: left;
        }
        .attendance-log th {
            background-color: #ff9800;
            color: white;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Fingerprint Biometrics Admin Dashboard</h1>

        <div id="message-area" class="message-box" style="display: none;"></div>

        <div class="status-box">
            Arduino Status: <span id="arduino-status">Loading...</span>
        </div>

        <div class="grid-container">
            <div class="card">
                <h2>Enroll Fingerprint</h2>
                <div class="input-group">
                    <label for="enroll-id">Enter ID (1-254):</label>
                    <input type="number" id="enroll-id" min="1" max="254" placeholder="e.g., 1">
                </div>
                <button class="btn btn-enroll" onclick="sendCommand('enroll')">Enroll Finger</button>
                <p style="font-size:0.85em; color:#666; margin-top:10px;">
                    Enter an ID and click 'Enroll'. Then follow prompts on Arduino LCD.
                </p>
            </div>

            <div class="card">
                <h2>Delete Fingerprint</h2>
                <div class="input-group">
                    <label for="delete-id">Enter ID to delete:</label>
                    <input type="number" id="delete-id" min="1" max="254" placeholder="e.g., 1">
                </div>
                <button class="btn btn-delete" onclick="sendCommand('delete')">Delete Finger</button>
                <p style="font-size:0.85em; color:#666; margin-top:10px;">
                    Enter the ID of the fingerprint you wish to delete.
                </p>
            </div>
        </div>

        <div class="card" style="margin-top:25px;">
            <h2>Recent Attendance Log</h2>
            <button class="btn btn-refresh" onclick="fetchAttendanceLog()">Refresh Attendance</button>
            <button class="btn btn-clear" onclick="clearAttendanceLog()">Clear Attendance Log</button>
            <div class="attendance-log">
                <table id="attendance-table">
                    <thead>
                        <tr>
                            <th>ID</th>
                            <th>Timestamp</th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr><td colspan="2">Loading...</td></tr>
                    </tbody>
                </table>
            </div>
        </div>
    </div>

    <script>
        // JavaScript for Frontend Interaction
        const API_URL = window.location.href; // This file itself is the API endpoint
        const arduinoStatusElement = document.getElementById('arduino-status');
        const attendanceTableBody = document.querySelector('#attendance-table tbody');
        const messageArea = document.getElementById('message-area');

        // --- Message Display Function ---
        function showMessage(msg, type = 'info', duration = 3000) {
            messageArea.textContent = msg;
            messageArea.className = 'message-box'; // Reset classes
            messageArea.classList.add('message-' + type);
            messageArea.style.display = 'block';
            setTimeout(() => {
                messageArea.style.display = 'none';
            }, duration);
        }

        // --- API Communication Functions ---

        async function fetchArduinoStatus() {
            try {
                const response = await fetch(API_URL + '?action=get_status');
                const data = await response.json();
                if (data.success) {
                    arduinoStatusElement.textContent = data.status;
                } else {
                    console.error("Failed to fetch status:", data.message);
                    arduinoStatusElement.textContent = 'Error: Could not fetch status.';
                }
            } catch (error) {
                console.error("Error fetching Arduino status:", error);
                arduinoStatusElement.textContent = 'Error: Network or server issue.';
            }
        }

        async function fetchAttendanceLog() {
            try {
                showMessage('Refreshing attendance log...', 'info', 1500);
                const response = await fetch(API_URL + '?action=get_attendance');
                const data = await response.json();
                attendanceTableBody.innerHTML = ''; // Clear existing rows
                if (data.success && data.attendance && data.attendance.length > 0) {
                    // Sort attendance data by timestamp (assuming timestamp is entry[0]) in descending order (most recent first)
                    data.attendance.sort((a, b) => new Date(b[0]) - new Date(a[0]));

                    data.attendance.forEach(entry => {
                        const row = attendanceTableBody.insertRow();
                        const idCell = row.insertCell(); // First column in HTML table
                        const tsCell = row.insertCell(); // Second column in HTML table

                        // Corrected: The CSV is timestamp,id. So entry[0] is timestamp, entry[1] is ID.
                        // HTML table header is ID, Timestamp. So map ID to first HTML column, Timestamp to second.
                        idCell.textContent = entry[1]; // ID from data (entry[1]) goes to the first HTML column
                        tsCell.textContent = entry[0]; // Timestamp from data (entry[0]) goes to the second HTML column
                    });
                } else {
                    const row = attendanceTableBody.insertRow();
                    const cell = row.insertCell();
                    cell.colSpan = 2;
                    cell.textContent = 'No attendance recorded yet.';
                }
            } catch (error) {
                console.error("Error fetching attendance log:", error);
                const row = attendanceTableBody.insertRow();
                const cell = row.insertCell();
                cell.colSpan = 2;
                cell.textContent = 'Error: Network or server issue fetching attendance.';
            }
        }

        async function clearAttendanceLog() {
            if (!confirm('Are you sure you want to clear the entire attendance log? This action cannot be undone.')) {
                return;
            }

            try {
                showMessage('Clearing attendance log...', 'info', 2000);
                const response = await fetch(API_URL + '?action=clear_attendance');
                const data = await response.json();
                if (data.success) {
                    showMessage(data.message, 'success');
                    fetchAttendanceLog(); // Refresh table after clearing
                } else {
                    showMessage(`Error clearing log: ${data.message}`, 'error');
                }
            } catch (error) {
                console.error("Error clearing attendance log:", error);
                showMessage('Network error while trying to clear attendance log.', 'error');
            }
        }

        async function sendCommand(action) {
            let id = '';
            let inputElement;

            if (action === 'enroll') {
                inputElement = document.getElementById('enroll-id');
                id = inputElement.value;
                if (!id || isNaN(id) || parseInt(id) <= 0 || parseInt(id) > 254) {
                    showMessage('Please enter a valid ID (1-254) for enrollment.', 'error');
                    return;
                }
            } else if (action === 'delete') {
                inputElement = document.getElementById('delete-id');
                id = inputElement.value;
                if (!id || isNaN(id) || parseInt(id) <= 0 || parseInt(id) > 254) {
                    showMessage('Please enter a valid ID (1-254) for deletion.', 'error');
                    return;
                }
            } else {
                showMessage('Unknown command.', 'error');
                return;
            }

            showMessage(`Sending ${action} command for ID ${id}...`, 'info');

            try {
                const response = await fetch(`${API_URL}?action=${action}&id=${id}`);
                const data = await response.json();

                if (data.success) {
                    showMessage(`${action} command sent successfully! Check Arduino LCD for prompts.`, 'success');
                    inputElement.value = ''; // Clear input
                } else {
                    showMessage(`Error sending ${action} command: ${data.message}`, 'error');
                }
            } catch (error) {
                console.error(`Error during ${action} command:`, error);
                showMessage(`Network error during ${action} command.`, 'error');
            }
        }

        // --- Initial Load and Polling ---
        document.addEventListener('DOMContentLoaded', () => {
            fetchArduinoStatus();
            fetchAttendanceLog(); // Initial load for attendance

            // Set up polling for status and attendance updates
            setInterval(fetchArduinoStatus, 2000); // Poll status every 2 seconds
            setInterval(fetchAttendanceLog, 5000); // Poll attendance every 5 seconds
        });
    </script>
</body>
</html>