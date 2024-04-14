// Code development was assisted by ChatGPT

#include <SPI.h>
#include <SD.h>
#include <digitalWriteFast.h>

// -- User-defined Settings -- //
bool sampling_on = true; // Set true to start sampling, false to stop
bool variable_sampling_on = false; // Set true for variable sampling rate, false for fixed rate
const unsigned long sampling_duration = 10000; // Duration of sampling (milliseconds)
const unsigned long pre_sampling_delay = 5000; // Delay before sampling starts


// -- Pin Definitions -- //
const int CS_MCP3008 = 10;
const int schmitt_trigger_output = 2;
const int sdChipSelect = 4;


// -- Constants -- //
#define BUFFER_SIZE 10000  // Buffer size for data storage
#define DEFAULT_SAMPLING_RATE 44100  // Default sampling rate in Hz
#define LOW_SAMPLING_RATE 100  // Low sampling rate in Hz
#define HIGH_SAMPLING_RATE 8000  // High sampling rate in Hz


// -- Global Variables -- //
bool samplingEnabled = false; // Set to false initially
unsigned long lastSampleTime = 0;
char buffer[BUFFER_SIZE];  // Data buffer
int bufferIndex = 0;  // Current position in buffer
unsigned long current_sampling_rate = DEFAULT_SAMPLING_RATE;  // Current sampling rate
File dataFile;  // File object for data storage

// Pre-calculated intervals for different sampling rates
const unsigned long intervalDefault = 1000000 / DEFAULT_SAMPLING_RATE;
const unsigned long intervalHigh = 1000000 / HIGH_SAMPLING_RATE;
const unsigned long intervalLow = 1000000 / LOW_SAMPLING_RATE;


// -- MCP3008 Settings -- //
SPISettings MCP3008(3000000, MSBFIRST, SPI_MODE0);
const byte adc_single_ch0 = (0x08);  // ADC Channel 0 configuration

SPISettings sdSPISettings(8000000, MSBFIRST, SPI_MODE0); // 8 MHz


// -- Function Definitions -- //

// Code taken from https://rheingoldheavy.com/mcp3008-tutorial-05-sampling-audio-frequency-signals-02
int adc_single_channel_read(byte readAddress) {
  byte dataMSB = 0;
  byte dataLSB = 0;
  byte JUNK = 0x00;

  SPI.beginTransaction(MCP3008);
  digitalWriteFast(CS_MCP3008, LOW);

  SPI.transfer(0x01);
  dataMSB = SPI.transfer(readAddress << 4) & 0x03;
  dataLSB = SPI.transfer(JUNK);

  digitalWriteFast(CS_MCP3008, HIGH);
  SPI.endTransaction();
  
  return (dataMSB << 8) | dataLSB;
}


bool fileExists(const char* fileName) {
  // Attempt to open the file in read mode
  File file = SD.open(fileName, FILE_READ);
  if (file) {
    file.close();  // Close the file if it was successfully opened
    return true;
  }
  return false;
}


// -- Setup Function -- //
void setup() {
  Serial.begin(2000000);
  delay(10000); // Gives time to pull up the serial monitor
  Serial.println("Setup Executing");
  pinMode(schmitt_trigger_output, INPUT);
  pinMode(sdChipSelect, OUTPUT);
  pinMode(CS_MCP3008, OUTPUT);
  digitalWriteFast(CS_MCP3008, HIGH); // Set CS high initially

  SPI.begin();
  delay(300); // Increase delay for stability

  // Checks to see if the SD card is working
  SPI.beginTransaction(sdSPISettings);
  if (!SD.begin(sdChipSelect)) {
    Serial.println("SD card initialization failed!");
    while (1); // Halt if the SD card fails to initialize
  }
  SPI.endTransaction();
  Serial.println("SD card initialized.");

  
  // First, check if the data file exists and then remove it to clear it
  if (SD.exists("data.txt")) {
    SD.remove("data.txt");
  }
  
  // Now, recreate the file by opening it for writing
  File data_file = SD.open("data.txt", FILE_WRITE);
  if (data_file) {
    Serial.println("Cleared data.txt by recreating it.");
    data_file.close(); // Close the file to finalize its creation
  } else {
    Serial.println("Failed to create data.txt.");
  }

  
  // Check if the ADC works by reading some values from it
  int adc_reading[10];
  SPI.beginTransaction(MCP3008);
  for (int i = 0; i < 10; i++) {
    adc_reading[i] = adc_single_channel_read(adc_single_ch0);
  }
  SPI.endTransaction();

  // Output the reading to the serial terminal
  for (int i = 0; i < 10; i++) {
    Serial.print("ADC Reading ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(adc_reading[i]);
  }
  Serial.println("Initialization done.");
}


// -- Main Function -- //
void loop() {
  // Initialize some variables
  static unsigned long startMillis = millis();
  static bool samplingPeriodOver = false;
  static String lastRateIndicator = "";

  // Waits the length of the predefined array to start reading and writing values from the ADC
  if (!samplingEnabled && millis() - startMillis >= pre_sampling_delay) {
    samplingEnabled = sampling_on;
    if (samplingEnabled) {
      dataFile = SD.open("data.txt", FILE_WRITE);
      startMillis = millis(); // Reset timer for sampling duration
    }
  }

  // Dump the buffer into data.txt
  if (samplingEnabled && millis() - startMillis >= sampling_duration && !samplingPeriodOver) {
    samplingPeriodOver = true;
    samplingEnabled = false;
    if (dataFile) {
      if (bufferIndex > 0) {
        dataFile.print(buffer);
        bufferIndex = 0;
      }
      dataFile.close();
    }
  }

  // Determine what sampling rate must be used
  if (samplingEnabled && !samplingPeriodOver) {
    unsigned long interval = intervalDefault;
    String rateIndicator = String(DEFAULT_SAMPLING_RATE) + "Hz: ";
    if (variable_sampling_on) {
      bool triggerState = digitalReadFast(schmitt_trigger_output);
      current_sampling_rate = triggerState ? HIGH_SAMPLING_RATE : LOW_SAMPLING_RATE;
      interval = triggerState ? intervalHigh : intervalLow;
      rateIndicator = String(current_sampling_rate) + "Hz: ";
    }

    // Read from the ADC and input its value into the buffer
    unsigned long currentMicros = micros();
    if (currentMicros - lastSampleTime >= interval) {
      lastSampleTime = currentMicros;
      int adcValue = adc_single_channel_read(adc_single_ch0);

      if (rateIndicator != lastRateIndicator) {
        if (bufferIndex > 0) { // Ensure we finish the last line before starting a new one
          bufferIndex += snprintf(buffer + bufferIndex, BUFFER_SIZE - bufferIndex, "\n"); //Also writes sampling rate into the buffer
          dataFile.print(buffer);
          bufferIndex = 0;
        }
        bufferIndex += snprintf(buffer + bufferIndex, BUFFER_SIZE - bufferIndex, "%s", rateIndicator.c_str());
        lastRateIndicator = rateIndicator;
      }

      bufferIndex += snprintf(buffer + bufferIndex, BUFFER_SIZE - bufferIndex, "%d ", adcValue);

      if (bufferIndex >= BUFFER_SIZE - 50) {
        dataFile.print(buffer);
        bufferIndex = 0;
      }
    }
  }

  // After the predefined sampling period is over, stop sampling
  if (samplingPeriodOver && !dataFile) {
    while (true) {} // Stop further execution
  }
}
 
