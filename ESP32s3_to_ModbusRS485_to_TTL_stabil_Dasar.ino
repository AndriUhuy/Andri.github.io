#include <HardwareSerial.h>

// Pin definitions sesuai dengan setup Anda
#define RO_PIN         16  // Receiver Output dari RS485 module
#define DI_PIN         17  // Driver Input ke RS485 module  
#define DE_RE_PIN      18  // Driver Enable & Receiver Enable (dijumper)

// Weather station settings
#define SLAVE_ADDRESS  0x90
#define BAUD_RATE      9600

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

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32-S3 WN90LP Weather Station Reader ===");
  
  // Setup RS485 control pin
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW); // Start in receive mode
  
  // Initialize RS485 serial communication
  rs485Serial.begin(BAUD_RATE, SERIAL_8N1, RO_PIN, DI_PIN);
  
  Serial.println("Modbus RTU initialized");
  Serial.println("Pinout:");
  Serial.printf("  RO (RX): GPIO %d\n", RO_PIN);
  Serial.printf("  DI (TX): GPIO %d\n", DI_PIN);
  Serial.printf("  DE/RE:   GPIO %d\n", DE_RE_PIN);
  Serial.println("Starting weather data collection...\n");
  
  delay(2000);
}

void loop() {
  Serial.println("==========================================");
  Serial.println("Reading Weather Station Data...");
  Serial.println("==========================================");
  
  // Read all weather parameters
  readLight();
  delay(100);
  
  readUVI();
  delay(100);
  
  readTemperature();
  delay(100);
  
  readHumidity();
  delay(100);
  
  readWindSpeed();
  delay(100);
  
  readGustSpeed();
  delay(100);
  
  readWindDirection();
  delay(100);
  
  readRainfall();
  delay(100);
  
  readPressure();
  delay(100);
  
  Serial.println("==========================================\n");
  delay(5000); // Wait 5 seconds before next reading
}

void readLight() {
  uint16_t value = readModbusRegister(REG_LIGHT);
  if (value != 0xFFFF) {
    float light = value * 10.0;
    Serial.printf("Light:        %.0f Lux\n", light);
  } else {
    Serial.println("Light:        Invalid/No data");
  }
}

void readUVI() {
  uint16_t value = readModbusRegister(REG_UVI);
  if (value != 0xFFFF) {
    float uvi = value / 10.0;
    Serial.printf("UV Index:     %.1f\n", uvi);
  } else {
    Serial.println("UV Index:     Invalid/No data");
  }
}

void readTemperature() {
  uint16_t value = readModbusRegister(REG_TEMPERATURE);
  if (value != 0xFFFF) {
    // Temperature calculation from documentation
    // 10.5°C = 0x1F9, -10.5°C = 0x127 with 400 offset
    float temp = (value - 400) / 10.0;
    Serial.printf("Temperature:  %.1f °C\n", temp);
  } else {
    Serial.println("Temperature:  Invalid/No data");
  }
}

void readHumidity() {
  uint16_t value = readModbusRegister(REG_HUMIDITY);
  if (value != 0xFFFF) {
    Serial.printf("Humidity:     %d %%\n", value);
  } else {
    Serial.println("Humidity:     Invalid/No data");
  }
}

void readWindSpeed() {
  uint16_t value = readModbusRegister(REG_WIND_SPEED);
  if (value != 0xFFFF) {
    float windSpeed = value * 0.1;
    Serial.printf("Wind Speed:   %.1f m/s\n", windSpeed);
  } else {
    Serial.println("Wind Speed:   Invalid/No data");
  }
}

void readGustSpeed() {
  uint16_t value = readModbusRegister(REG_GUST_SPEED);
  if (value != 0xFFFF) {
    float gustSpeed = value * 0.1;
    Serial.printf("Gust Speed:   %.1f m/s\n", gustSpeed);
  } else {
    Serial.println("Gust Speed:   Invalid/No data");
  }
}

void readWindDirection() {
  uint16_t value = readModbusRegister(REG_WIND_DIR);
  if (value != 0xFFFF) {
    Serial.printf("Wind Dir:     %d°\n", value);
  } else {
    Serial.println("Wind Dir:     Invalid/No data");
  }
}

void readRainfall() {
  uint16_t value = readModbusRegister(REG_RAINFALL);
  if (value != 0xFFFF) {
    float rainfall = value * 0.1;
    Serial.printf("Rainfall:     %.1f mm\n", rainfall);
  } else {
    Serial.println("Rainfall:     Invalid/No data");
  }
}

void readPressure() {
  uint16_t value = readModbusRegister(REG_ABS_PRESSURE);
  if (value != 0xFFFF) {
    float pressure = value * 0.1;
    Serial.printf("Pressure:     %.1f hPa\n", pressure);
  } else {
    Serial.println("Pressure:     Invalid/No data");
  }
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
      } else {
        Serial.println("CRC Error");
      }
    } else if (response[1] & 0x80) {
      Serial.printf("Modbus Error: 0x%02X\n", response[2]);
    }
  } else {
    Serial.println("Timeout - No response");
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