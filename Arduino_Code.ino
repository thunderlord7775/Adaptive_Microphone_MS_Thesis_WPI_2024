#include <SPI.h>
#include <SD.h>

const int chipSelectPin = 10; // Chip select pin for MCP3008
const int switchPin = 7;      // External switch pin
const int ledPinADCConnected = 8; // LED pin indicating ADC connection
const int ledPinADCReading = 9;   // LED pin indicating ADC is being read
const int sdChipSelect = 4;    // SD card module CS pin
bool samplingEnabled = false;  // Flag to control ADC reading
int lastSwitchState = LOW;     // Initial state of the switch

void setup() {
  pinMode(switchPin, INPUT);
  pinMode(ledPinADCConnected, OUTPUT);
  pinMode(ledPinADCReading, OUTPUT);
  SPI.begin();
  Serial.begin(115200);

  if (!SD.begin(sdChipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("Initialization done.");

  int adcValue = readADC(0);
  digitalWrite(ledPinADCConnected, (adcValue >= 0) ? HIGH : LOW);
}

void loop() {
  int currentSwitchState = digitalRead(switchPin);
  
  if (currentSwitchState != lastSwitchState) {
    delay(50); // Debounce delay
    lastSwitchState = currentSwitchState; // Update the last switch state after the debounce delay
    samplingEnabled = currentSwitchState; // Update samplingEnabled based on the switch's state
    digitalWrite(ledPinADCReading, samplingEnabled ? HIGH : LOW);
  }

  if (samplingEnabled) {
    int samplingRate = determineSamplingRate(currentSwitchState);
    int adcValue = readADC(0);

    File dataFile = SD.open("data.txt", FILE_WRITE);
    if (dataFile) {
      dataFile.print("ADC Value: ");
      dataFile.println(adcValue);
      dataFile.close();
    } else {
      Serial.println("Error opening data.txt");
    }

    delayMicroseconds(1000000 / samplingRate);
  }
}

int readADC(int channel) {
  digitalWrite(chipSelectPin, LOW);
  byte command = 0b00011000 | (channel << 3);
  SPI.transfer(command);
  int highByte = SPI.transfer(0x00);
  int lowByte = SPI.transfer(0x00);
  digitalWrite(chipSelectPin, HIGH);
  return ((highByte & 0x03) << 8) | lowByte;
}

int determineSamplingRate(int switchState) {
  if (switchState == HIGH) {
    return 8000;  // Return 8 kHz when switch is HIGH
  } else {
    return 100;   // Return 100 Hz when switch is LOW
  }
  // Adjust these values as necessary for your application
}
