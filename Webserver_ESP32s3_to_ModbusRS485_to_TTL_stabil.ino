#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

// Pin definitions sesuai dengan setup Anda
#define RO_PIN         16  // Receiver Output dari RS485 module
#define DI_PIN         17  // Driver Input ke RS485 module  
#define DE_RE_PIN      18  // Driver Enable & Receiver Enable (dijumper)

// Weather station settings
#define SLAVE_ADDRESS  0x90
#define BAUD_RATE      9600

// Access Point credentials
const char* ssid = "WeatherStation_ESP32";
const char* password = "weather123";

// Register addresses dari dokumentasi
#define REG_LIGHT         0x0165
#define REG_UVI           0x0166
#define REG_TEMPERATURE   0x0167
#define REG_HUMIDITY      0x0168
#define REG_WIND_SPEED    0x0169
#define REG_GUST_SPEED    0x016A
#define REG_WIND_DIR      0x016B
#define REG_RAINFALL      0x016C
#define REG_ABS_PRESSURE  0x016D
#define REG_RAIN_COUNTER  0x016E

HardwareSerial rs485Serial(1);
WebServer server(80);
DNSServer dnsServer;

// DNS Server settings
const byte DNS_PORT = 53;

// Weather data structure
struct WeatherData {
  float temperature = 0.0;
  float humidity = 0.0;
  float pressure = 0.0;
  float windSpeed = 0.0;
  float gustSpeed = 0.0;
  int windDirection = 0;
  float rainfall = 0.0;
  float light = 0.0;
  float uvi = 0.0;
  bool dataValid = false;
  unsigned long lastUpdate = 0;
};

WeatherData currentWeather;

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32-S3 Weather Station AP Mode ===");
  
  // Setup RS485 control pin
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW); // Start in receive mode
  
  // Initialize RS485 serial communication
  rs485Serial.begin(BAUD_RATE, SERIAL_8N1, RO_PIN, DI_PIN);
  
  // Setup Access Point
  setupAccessPoint();
  
  // Setup DNS Server
  setupDNSServer();
  
  // Setup web server routes
  setupWebServer();
  
  Serial.println("Setup completed!");
  Serial.printf("Connect to WiFi: %s\n", ssid);
  Serial.printf("Password: %s\n", password);
  Serial.println("Open browser and go to: http://192.168.4.1");
  
  delay(2000);
}

void loop() {
  // Handle DNS requests
  dnsServer.processNextRequest();
  
  // Handle web server requests
  server.handleClient();
  
  // Check AP status every 30 seconds
  static unsigned long lastAPCheck = 0;
  if (millis() - lastAPCheck >= 30000) {
    checkAPStatus();
    lastAPCheck = millis();
  }
  
  // Read weather data every 5 seconds
  static unsigned long lastReading = 0;
  if (millis() - lastReading >= 5000) {
    readAllWeatherData();
    lastReading = millis();
  }
  
  delay(10);
}

void setupAccessPoint() {
  // Disconnect dari WiFi jika sebelumnya terhubung
  WiFi.disconnect();
  delay(100);
  
  // Set mode ke Access Point
  WiFi.mode(WIFI_AP);
  delay(100);
  
  // Konfigurasi Access Point dengan parameter lengkap
  bool result = WiFi.softAP(ssid, password, 1, 0, 4); // channel 1, hidden 0, max connections 4
  
  if (result) {
    Serial.println("Access Point created successfully!");
  } else {
    Serial.println("Failed to create Access Point!");
    // Coba dengan parameter berbeda
    result = WiFi.softAP(ssid, password);
    if (result) {
      Serial.println("Access Point created with default settings!");
    }
  }
  
  // Set IP configuration
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  
  delay(1000); // Wait for AP to be fully initialized
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("=== Access Point Information ===");
  Serial.printf("SSID: %s\n", ssid);
  Serial.printf("Password: %s\n", password);
  Serial.printf("IP address: %s\n", IP.toString().c_str());
  Serial.printf("MAC address: %s\n", WiFi.softAPmacAddress().c_str());
  Serial.println("===============================");
}

void setupWebServer() {
  // Serve main dashboard page
  server.on("/", HTTP_GET, [](){
    server.send(200, "text/html", getMainPage());
  });
  
  // Captive portal - redirect semua request ke dashboard
  server.onNotFound([](){
    server.send(200, "text/html", getMainPage());
  });
  
  // API endpoint for weather data
  server.on("/api/weather", HTTP_GET, [](){
    String json = getWeatherJSON();
    server.send(200, "application/json", json);
  });
  
  // API endpoint for system info
  server.on("/api/system", HTTP_GET, [](){
    String json = getSystemJSON();
    server.send(200, "application/json", json);
  });
  
  // Handle common captive portal detection URLs
  server.on("/generate_204", HTTP_GET, [](){
    server.send(200, "text/html", getMainPage());
  });
  
  server.on("/fwlink", HTTP_GET, [](){
    server.send(200, "text/html", getMainPage());
  });
  
  server.on("/connecttest.txt", HTTP_GET, [](){
    server.send(200, "text/html", getMainPage());
  });
  
  // Handle CORS for API requests
  server.enableCORS(true);
  
  server.begin();
  Serial.println("Web server started with captive portal");
}

void readAllWeatherData() {
  Serial.println("Reading weather data...");
  
  currentWeather.temperature = readTemperature();
  delay(100);
  currentWeather.humidity = readHumidity();
  delay(100);
  currentWeather.pressure = readPressure();
  delay(100);
  currentWeather.windSpeed = readWindSpeed();
  delay(100);
  currentWeather.gustSpeed = readGustSpeed();
  delay(100);
  currentWeather.windDirection = readWindDirection();
  delay(100);
  currentWeather.rainfall = readRainfall();
  delay(100);
  currentWeather.light = readLight();
  delay(100);
  currentWeather.uvi = readUVI();
  
  currentWeather.lastUpdate = millis();
  currentWeather.dataValid = true;
  
  Serial.println("Weather data updated");
}

float readTemperature() {
  uint16_t value = readModbusRegister(REG_TEMPERATURE);
  if (value != 0xFFFF) {
    return (value - 400) / 10.0;
  }
  return 0.0;
}

float readHumidity() {
  uint16_t value = readModbusRegister(REG_HUMIDITY);
  if (value != 0xFFFF) {
    return (float)value;
  }
  return 0.0;
}

float readPressure() {
  uint16_t value = readModbusRegister(REG_ABS_PRESSURE);
  if (value != 0xFFFF) {
    return value * 0.1;
  }
  return 0.0;
}

float readWindSpeed() {
  uint16_t value = readModbusRegister(REG_WIND_SPEED);
  if (value != 0xFFFF) {
    return value * 0.1;
  }
  return 0.0;
}

float readGustSpeed() {
  uint16_t value = readModbusRegister(REG_GUST_SPEED);
  if (value != 0xFFFF) {
    return value * 0.1;
  }
  return 0.0;
}

int readWindDirection() {
  uint16_t value = readModbusRegister(REG_WIND_DIR);
  if (value != 0xFFFF) {
    return (int)value;
  }
  return 0;
}

float readRainfall() {
  uint16_t value = readModbusRegister(REG_RAINFALL);
  if (value != 0xFFFF) {
    return value * 0.1;
  }
  return 0.0;
}

float readLight() {
  uint16_t value = readModbusRegister(REG_LIGHT);
  if (value != 0xFFFF) {
    return value * 10.0;
  }
  return 0.0;
}

float readUVI() {
  uint16_t value = readModbusRegister(REG_UVI);
  if (value != 0xFFFF) {
    return value / 10.0;
  }
  return 0.0;
}

uint16_t readModbusRegister(uint16_t registerAddress) {
  // Modbus RTU frame untuk membaca 1 register
  uint8_t frame[8];
  
  frame[0] = SLAVE_ADDRESS;           // Slave address
  frame[1] = 0x03;                    // Function code (Read Holding Registers)
  frame[2] = (registerAddress >> 8) & 0xFF;  // Start address high byte
  frame[3] = registerAddress & 0xFF;          // Start address low byte
  frame[4] = 0x00;                    // Number of registers high byte
  frame[5] = 0x01;                    // Number of registers low byte
  
  // Calculate CRC
  uint16_t crc = calculateCRC(frame, 6);
  frame[6] = crc & 0xFF;              // CRC low byte
  frame[7] = (crc >> 8) & 0xFF;       // CRC high byte
  
  // Set to transmit mode
  digitalWrite(DE_RE_PIN, HIGH);
  delayMicroseconds(10);
  
  // Send frame
  rs485Serial.write(frame, 8);
  rs485Serial.flush();
  
  // Set to receive mode
  delayMicroseconds(10);
  digitalWrite(DE_RE_PIN, LOW);
  
  // Wait for response (timeout 1000ms)
  unsigned long startTime = millis();
  while (rs485Serial.available() < 7 && (millis() - startTime) < 1000) {
    delay(1);
  }
  
  if (rs485Serial.available() >= 7) {
    uint8_t response[7];
    rs485Serial.readBytes(response, 7);
    
    // Verify response
    if (response[0] == SLAVE_ADDRESS && response[1] == 0x03 && response[2] == 0x02) {
      // Extract data (16-bit value)
      uint16_t value = (response[3] << 8) | response[4];
      
      // Verify CRC
      uint16_t receivedCRC = response[5] | (response[6] << 8);
      uint16_t calculatedCRC = calculateCRC(response, 5);
      
      if (receivedCRC == calculatedCRC) {
        return value;
      }
    }
  }
  
  // Clear any remaining bytes
  while (rs485Serial.available()) {
    rs485Serial.read();
  }
  
  return 0xFFFF; // Invalid value
}

uint16_t calculateCRC(uint8_t* data, uint8_t length) {
  uint16_t crc = 0xFFFF;
  
  for (uint8_t i = 0; i < length; i++) {
    crc ^= data[i];
    
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  
  return crc;
}

String getWeatherJSON() {
  DynamicJsonDocument doc(512);
  
  doc["temperature"] = currentWeather.temperature;
  doc["humidity"] = currentWeather.humidity;
  doc["pressure"] = currentWeather.pressure;
  doc["windSpeed"] = currentWeather.windSpeed;
  doc["gustSpeed"] = currentWeather.gustSpeed;
  doc["windDirection"] = currentWeather.windDirection;
  doc["rainfall"] = currentWeather.rainfall;
  doc["light"] = currentWeather.light;
  doc["uvi"] = currentWeather.uvi;
  doc["dataValid"] = currentWeather.dataValid;
  doc["lastUpdate"] = currentWeather.lastUpdate;
  
  String output;
  serializeJson(doc, output);
  return output;
}

String getSystemJSON() {
  DynamicJsonDocument doc(256);
  
  doc["uptime"] = millis();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["chipModel"] = ESP.getChipModel();
  doc["chipRevision"] = ESP.getChipRevision();
  doc["cpuFreq"] = ESP.getCpuFreqMHz();
  doc["connectedClients"] = WiFi.softAPgetStationNum();
  
  String output;
  serializeJson(doc, output);
  return output;
}

String getWindDirection(int degrees) {
  const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int index = (degrees + 11) / 22;
  return directions[index % 16];
}

void checkAPStatus() {
  Serial.println("=== Access Point Status ===");
  Serial.printf("AP Status: %s\n", WiFi.getMode() == WIFI_AP ? "Active" : "Inactive");
  Serial.printf("Connected clients: %d\n", WiFi.softAPgetStationNum());
  Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("MAC: %s\n", WiFi.softAPmacAddress().c_str());
  
  // Restart AP if not working
  if (WiFi.getMode() != WIFI_AP) {
    Serial.println("Restarting Access Point...");
    setupAccessPoint();
  }
  Serial.println("==========================");
}

void setupDNSServer() {
  // Setup DNS server untuk captive portal
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("DNS Server started for captive portal");
}

String getMainPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Weather Station Dashboard</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #0c0c0c 0%, #1a1a2e 50%, #16213e 100%);
            color: #ffffff;
            min-height: 100vh;
            overflow-x: hidden;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
        }

        .header {
            text-align: center;
            margin-bottom: 40px;
            padding: 20px;
            background: rgba(255, 255, 255, 0.1);
            border-radius: 20px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.2);
        }

        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            background: linear-gradient(45deg, #00d4ff, #0099cc);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }

        .header p {
            font-size: 1.1em;
            opacity: 0.8;
        }

        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }

        .card {
            background: rgba(255, 255, 255, 0.1);
            border-radius: 20px;
            padding: 25px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.2);
            transition: all 0.3s ease;
            position: relative;
            overflow: hidden;
        }

        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 20px 40px rgba(0, 212, 255, 0.3);
        }

        .card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            height: 3px;
            background: linear-gradient(90deg, #00d4ff, #0099cc);
        }

        .card-header {
            display: flex;
            align-items: center;
            margin-bottom: 15px;
        }

        .card-icon {
            font-size: 2em;
            margin-right: 15px;
        }

        .card-title {
            font-size: 1.2em;
            font-weight: 600;
        }

        .card-value {
            font-size: 2.5em;
            font-weight: bold;
            margin: 10px 0;
            background: linear-gradient(45deg, #ffffff, #e0e0e0);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
        }

        .card-unit {
            font-size: 0.9em;
            opacity: 0.7;
        }

        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
        }

        .status-online {
            background: #00ff88;
            box-shadow: 0 0 10px #00ff88;
        }

        .status-offline {
            background: #ff4444;
            box-shadow: 0 0 10px #ff4444;
        }

        .system-info {
            background: rgba(255, 255, 255, 0.05);
            border-radius: 15px;
            padding: 20px;
            margin-top: 20px;
        }

        .system-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }

        .system-item {
            text-align: center;
            padding: 10px;
        }

        .system-label {
            font-size: 0.9em;
            opacity: 0.7;
            margin-bottom: 5px;
        }

        .system-value {
            font-size: 1.2em;
            font-weight: bold;
        }

        .wind-compass {
            position: relative;
            width: 120px;
            height: 120px;
            margin: 20px auto;
            border: 2px solid rgba(255, 255, 255, 0.3);
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .wind-arrow {
            position: absolute;
            width: 4px;
            height: 40px;
            background: #00d4ff;
            transform-origin: bottom center;
            border-radius: 2px;
        }

        .loading {
            text-align: center;
            padding: 20px;
            opacity: 0.7;
        }

        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.5; }
            100% { opacity: 1; }
        }

        .pulse {
            animation: pulse 2s infinite;
        }

        @media (max-width: 768px) {
            .container {
                padding: 10px;
            }
            
            .grid {
                grid-template-columns: 1fr;
            }
            
            .header h1 {
                font-size: 2em;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🌤️ Weather Station Dashboard</h1>
            <p><span class="status-indicator" id="statusIndicator"></span><span id="statusText">Connecting...</span></p>
        </div>

        <div class="grid" id="weatherGrid">
            <div class="loading pulse">Loading weather data...</div>
        </div>

        <div class="system-info">
            <h3 style="margin-bottom: 20px; text-align: center;">📊 System Information</h3>
            <div class="system-grid" id="systemGrid">
                <div class="loading pulse">Loading system info...</div>
            </div>
        </div>
    </div>

    <script>
        let isOnline = false;

        function updateStatus(online) {
            const indicator = document.getElementById('statusIndicator');
            const text = document.getElementById('statusText');
            
            if (online) {
                indicator.className = 'status-indicator status-online';
                text.textContent = 'Online - Data updating';
                isOnline = true;
            } else {
                indicator.className = 'status-indicator status-offline';
                text.textContent = 'Offline - Connection lost';
                isOnline = false;
            }
        }

        function getWindDirection(degrees) {
            const directions = ["N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"];
            const index = Math.round(degrees / 22.5) % 16;
            return directions[index];
        }

        function createWeatherCard(icon, title, value, unit, extraInfo = '') {
            return `
                <div class="card">
                    <div class="card-header">
                        <div class="card-icon">${icon}</div>
                        <div class="card-title">${title}</div>
                    </div>
                    <div class="card-value">${value}</div>
                    <div class="card-unit">${unit}</div>
                    ${extraInfo}
                </div>
            `;
        }

        function updateWeatherData() {
            fetch('/api/weather')
                .then(response => response.json())
                .then(data => {
                    updateStatus(true);
                    
                    const windCompass = `
                        <div class="wind-compass">
                            <div class="wind-arrow" style="transform: rotate(${data.windDirection}deg);"></div>
                            <div style="position: absolute; font-size: 0.8em;">${getWindDirection(data.windDirection)}</div>
                        </div>
                    `;

                    const weatherHTML = `
                        ${createWeatherCard('🌡️', 'Temperature', data.temperature.toFixed(1), '°C')}
                        ${createWeatherCard('💧', 'Humidity', data.humidity.toFixed(0), '%')}
                        ${createWeatherCard('🌪️', 'Wind Speed', data.windSpeed.toFixed(1), 'm/s')}
                        ${createWeatherCard('💨', 'Gust Speed', data.gustSpeed.toFixed(1), 'm/s')}
                        ${createWeatherCard('🧭', 'Wind Direction', data.windDirection, '°', windCompass)}
                        ${createWeatherCard('🌧️', 'Rainfall', data.rainfall.toFixed(1), 'mm')}
                        ${createWeatherCard('📊', 'Pressure', data.pressure.toFixed(1), 'hPa')}
                        ${createWeatherCard('☀️', 'Light', data.light.toFixed(0), 'Lux')}
                        ${createWeatherCard('🔆', 'UV Index', data.uvi.toFixed(1), 'UVI')}
                    `;
                    
                    document.getElementById('weatherGrid').innerHTML = weatherHTML;
                })
                .catch(error => {
                    console.error('Error:', error);
                    updateStatus(false);
                });
        }

        function updateSystemInfo() {
            fetch('/api/system')
                .then(response => response.json())
                .then(data => {
                    const uptimeHours = (data.uptime / (1000 * 60 * 60)).toFixed(1);
                    const freeHeapKB = (data.freeHeap / 1024).toFixed(1);
                    
                    const systemHTML = `
                        <div class="system-item">
                            <div class="system-label">Uptime</div>
                            <div class="system-value">${uptimeHours}h</div>
                        </div>
                        <div class="system-item">
                            <div class="system-label">Free Memory</div>
                            <div class="system-value">${freeHeapKB} KB</div>
                        </div>
                        <div class="system-item">
                            <div class="system-label">CPU Frequency</div>
                            <div class="system-value">${data.cpuFreq} MHz</div>
                        </div>
                        <div class="system-item">
                            <div class="system-label">Connected Clients</div>
                            <div class="system-value">${data.connectedClients}</div>
                        </div>
                        <div class="system-item">
                            <div class="system-label">Chip Model</div>
                            <div class="system-value">${data.chipModel}</div>
                        </div>
                        <div class="system-item">
                            <div class="system-label">Chip Revision</div>
                            <div class="system-value">${data.chipRevision}</div>
                        </div>
                    `;
                    
                    document.getElementById('systemGrid').innerHTML = systemHTML;
                })
                .catch(error => {
                    console.error('System info error:', error);
                });
        }

        // Initialize
        updateWeatherData();
        updateSystemInfo();

        // Update data every 3 seconds
        setInterval(updateWeatherData, 3000);
        setInterval(updateSystemInfo, 10000);
    </script>
</body>
</html>
)rawliteral";
}