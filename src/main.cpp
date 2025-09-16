
/*/*
   Based on 31337Ghost's reference code from https://github.com/nkolban/esp32-snippets/issues/385#issuecomment-362535434
   which is based on pcbreflux's Arduino ESP32 port of Neil Kolban's example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
*/

/*
   Create a BLE server that will send periodic iBeacon frames.
   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create advertising data
   3. Start advertising.
   4. wait
   5. Stop advertising.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEBeacon.h>
#include <string>
#include <Arduino.h>
#include "Seeed_Arduino_mmWave.h"

#ifdef ESP32
#  include <HardwareSerial.h>
HardwareSerial mmwaveSerial(0);
#else
#  define mmwaveSerial Serial1
#endif

#define DEVICE_NAME         "ESP32-FTFD"
#define SERVICE_UUID        "7A0247E7-8E88-409B-A959-AB5092DDB03E"
#define BEACON_UUID         "2D7A9F0C-E0E8-4CC9-A71B-A21DB2D034A1"
#define CHARACTERISTIC_UUID "82258BAA-DF72-47E8-99BC-B73D7ECD08A5"

SEEED_MR60FDA2 mmWave;

static uint8_t fallFrame[8];
uint8_t FallStatus = 0;

BLEServer *pServer;
BLECharacteristic *pCharacteristic;
BLEAdvertising *pAdvertising;
BLEScan* pBLEScan;
bool deviceConnected = false;
uint8_t value = 0;
int scanTime = 5;


// Variables for non-blocking timing
unsigned long previousMillis = 0;
unsigned long burstStartMillis = 0;
const long interval = 10000; // 10 seconds
const long burstDuration = 100; // 100 milliseconds
unsigned long lastPrintMillis = 0;
const unsigned long printInterval = 10000; // print once per 1s


uint32_t sensitivity = 15;
float height = 2.8, threshold = 1.0;
float rect_XL, rect_XR, rect_ZF, rect_ZB;

typedef enum {
  EXIST_PEOPLE,
  NO_PEOPLE,
  PEOPLE_FALL,
} MMWAVE_STATUS;

MMWAVE_STATUS status = NO_PEOPLE, last_status = NO_PEOPLE;


bool isAdvertising = false;

void Fall_Frame_Send(uint8_t *fallFrame, MMWAVE_STATUS status);
void updateScanResponse(uint8_t *fallFrame);

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("deviceConnected = true");
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("deviceConnected = false");
    // Restart advertising to be visible and connectable again
    pAdvertising->start();
    Serial.println("iBeacon advertising restarted");
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.println("*********");
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);
      }
      Serial.println();
      Serial.println("*********");
    }
  }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());
    }
};

void init_service() {
  // Create the BLE Service
  BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  pAdvertising->addServiceUUID(BLEUUID(SERVICE_UUID));

  // Start the service
  pService->start();
}

void init_beacon(uint8_t *fallFrame) {
  // --- iBeacon advertisement ---
  BLEBeacon b;
  b.setManufacturerId(0x4C00);           // Apple Inc.
  b.setProximityUUID(BLEUUID(BEACON_UUID));
  b.setMajor(5);
  b.setMinor(88);
  b.setSignalPower((int8_t)-59);

  // --- Scan Response: put fallFrame here ---
  uint8_t buf[10];
  buf[0] = 0xE5;  // Espressif company ID low byte
  buf[1] = 0x02;  // Espressif company ID high byte
  memcpy(buf + 2, fallFrame, 8);

  BLEAdvertisementData adv;
  adv.setFlags(0x1A);
  adv.setName(DEVICE_NAME);
  adv.setManufacturerData(String((char*)buf, 10));  // correct iBeacon payload
  pAdvertising->setAdvertisementData(adv);

  pAdvertising->setMinInterval(0x1000); // Set min interval to 10 seconds
  pAdvertising->setMaxInterval(0x1000); // Set max interval to
  pAdvertising->start();

  // Serial.println("iBeacon advertising started (Apple 0x004C).");
}


void setup() {
  Serial.begin(115200);
  mmWave.begin(&mmwaveSerial);
  Serial.println();
  Serial.println("Initializing...");
  Serial.flush();

  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  pAdvertising = pServer->getAdvertising();

  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(16000);
  pBLEScan->setWindow(99);

  init_service();
  init_beacon(fallFrame);

  Serial.println("iBeacon + service defined and advertising!");

  mmWave.setUserLog(0);

  if (mmWave.setInstallationHeight(height)) {
    Serial.printf("setInstallationHeight success: %.2f\n", height);
  } else {
    Serial.println("setInstallationHeight failed");
  }

  /** Set threshold **/
  if (mmWave.setThreshold(threshold)) {
    Serial.printf("setThreshold success: %.2f\n", threshold);
  } else {
    Serial.println("setThreshold failed");
  }

  /** Set sensitivity **/
  if (mmWave.setSensitivity(sensitivity)) {
    Serial.printf("setSensitivity success %d\n", sensitivity);
  } else {
    Serial.println("setSensitivity failed");
  }

  /** get new parameters of mmwave **/
  if (mmWave.getRadarParameters(height, threshold, sensitivity, rect_XL,
                                rect_XR, rect_ZF, rect_ZB)) {
    Serial.printf("height: %.2f\tthreshold: %.2f\tsensitivity: %d\n", height,
                  threshold, sensitivity);
    Serial.printf(
        "rect_XL: %.2f\trect_XR: %.2f\trect_ZF: %.2f\trect_ZB: %.2f\n", rect_XL,
        rect_XR, rect_ZF, rect_ZB);
  } else {
    Serial.println("getRadarParameters failed");
  }

}


void loop() {
  if (mmWave.update(100)) {
    bool is_human, is_fall;
    // Get the human detection status
    if (mmWave.getHuman(is_human)) {
      // Get the fall detection status
      if (mmWave.getFall(is_fall)) {
        // Determine the status based on human and fall detection
        if (!is_human && !is_fall) {
          status = NO_PEOPLE;  // No human and no fall detected
        } else if (is_fall) {
          status = PEOPLE_FALL;  // Fall detected
        } else {
          status = EXIST_PEOPLE;  // Human detected without fall
        }
      }
    }
    // Get the human detection status
    if (!mmWave.getHuman(is_human) && !mmWave.getFall(is_fall)) {
      status = NO_PEOPLE;  // No human and no fall detected
    } else if (is_fall) {
      status = PEOPLE_FALL;  // Fall detected
    } else {
      status = EXIST_PEOPLE;  // Human detected without fall
    }
  }

 if (millis() - lastPrintMillis >= printInterval) {
    lastPrintMillis = millis();

    switch (status) {
      case NO_PEOPLE:
        Serial.println("Waiting for people");
        break;
      case EXIST_PEOPLE:
        Serial.println("PEOPLE !!!");
        break;
      case PEOPLE_FALL:
        Serial.println("FALL !!!");
        break;
      default:
        break;
    }
  }
  


  if (deviceConnected) {
    Serial.printf("*** NOTIFY: %d ***\n", value);
    pCharacteristic->setValue(&value, 1);
    pCharacteristic->notify();
    value++;
  }

  // State 1: Check if it's time to start a new advertising burst.
  if (!isAdvertising && millis() - previousMillis >= interval) {
    previousMillis = millis();

    Fall_Frame_Send(fallFrame, status);

    init_beacon(fallFrame);

    // Start advertising and switch to the advertising state.
    pAdvertising->start();
    burstStartMillis = millis();
    isAdvertising = true;

    Serial.println("Starting advertising burst.");
  }
  
  // State 2: Check if it's time to end the advertising burst.
  if (isAdvertising && millis() - burstStartMillis >= burstDuration) {
    pAdvertising->stop();
    isAdvertising = false;
    Serial.println("Advertising burst ended.");
  }
}


void Fall_Frame_Send(uint8_t *fallFrame, MMWAVE_STATUS status){
    uint8_t checksum = 0x00;
    uint16_t payloadLength = 4;

    fallFrame[0] = 0x055; // Start Byte
    fallFrame[1] = (payloadLength >> 8) & 0xFF; // Length MSB
    fallFrame[2] = payloadLength & 0xFF; // Length LSB
    fallFrame[3] = 0x01; // End Device ID
    fallFrame[4] = 0x99; // Command

    /*Data Processing*/
    if (status == PEOPLE_FALL){
        fallFrame[5] = 0x01;
        Serial.println("Fall Detected");
    }else if (status == EXIST_PEOPLE)  {
        fallFrame[5] = 0x69;
        Serial.println("People Detected");
    }else if (status == NO_PEOPLE)  {
        fallFrame[5] = 0x00;
        Serial.println("No Fall Detected");
    }else{
        fallFrame[5] = 0xFF;
        Serial.println("Status Unknown");
    }

    /*Checksum Calculation*/
    for (int i = 3; i < 6; i++) {
        checksum ^= fallFrame[i]; // Checksum
    }
    fallFrame[6] = checksum; // Checksum
    fallFrame[7] = 0xCC; // End Byte

    Serial.print("Actual Fall Frame: ");
    for (int i = 0; i < 8; i++) {
        Serial.print(fallFrame[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}


