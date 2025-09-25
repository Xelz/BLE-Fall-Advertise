

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEBeacon.h>
#include <string>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
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

#define LIGHT_GPIO D0
#define LED_PIN D1

SEEED_MR60FDA2 mmWave;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

static uint8_t fallFrame[8];

BLEServer *pServer;
BLECharacteristic *pCharacteristic;
BLEAdvertising *pAdvertising;
bool deviceConnected = false;

// timing
unsigned long previousMillis = 0;
unsigned long lastPrintMillis = 0;
const unsigned long interval = 2500;      // update BLE payload every 10s
const unsigned long printInterval = 10000; // log every 10s

// radar params
uint32_t sensitivity = 3;
float height = 2.58, threshold = 0.85;
float rect_XL, rect_XR, rect_ZF, rect_ZB;

typedef enum {
  EXIST_PEOPLE,
  NO_PEOPLE,
  PEOPLE_FALL,
} MMWAVE_STATUS;

MMWAVE_STATUS status = NO_PEOPLE, last_status = NO_PEOPLE;

void relay_init() { pinMode(LIGHT_GPIO, OUTPUT); }
void relay_on()   { digitalWrite(LIGHT_GPIO, HIGH); }
void relay_off()  { digitalWrite(LIGHT_GPIO, LOW); }

void Fall_Frame_Send(uint8_t *fallFrame, MMWAVE_STATUS status) {
    uint8_t checksum = 0x00;
    uint16_t payloadLength = 4;

    fallFrame[0] = 0x55; // Start Byte
    fallFrame[1] = (payloadLength >> 8) & 0xFF; // Length MSB
    fallFrame[2] = payloadLength & 0xFF;        // Length LSB
    fallFrame[3] = 0x01; // End Device ID
    fallFrame[4] = 0x99; // Command

    if (status == PEOPLE_FALL) {
        fallFrame[5] = 0x02;
        Serial.println("Fall Detected");
    } else if (status == EXIST_PEOPLE) {
        fallFrame[5] = 0x01;
        Serial.println("People Detected");
    } else if (status == NO_PEOPLE) {
        fallFrame[5] = 0x00;
        Serial.println("No People Detected");
    } else {
        fallFrame[5] = 0xFF;
        Serial.println("Status Unknown");
    }

    for (int i = 3; i < 6; i++) checksum ^= fallFrame[i];
    fallFrame[6] = checksum;
    fallFrame[7] = 0xCC;

    Serial.print("Fall Frame: ");
    for (int i = 0; i < 8; i++) {
        Serial.print(fallFrame[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void updateAdvertiseData(uint8_t *fallFrame) {
  // Serial.println("Updating BLE payload");
  uint8_t buf[10];
  buf[0] = 0xE5;  // Espressif company ID low byte
  buf[1] = 0x02;  // Espressif company ID high byte
  memcpy(buf + 2, fallFrame, 8);

  BLEAdvertisementData adv;
  adv.setManufacturerData(String((char*)buf, 10));

  pAdvertising->setAdvertisementData(adv); // just update payload
  Serial.println("BLE payload updated");
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
  };
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
    pAdvertising->start();
  }
};

void init_service() {
  BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pAdvertising->addServiceUUID(BLEUUID(SERVICE_UUID));
  pService->start();
}

void setup() {
  Serial.begin(115200);
  mmWave.begin(&mmwaveSerial);
  Serial.println("Initializing...");

  relay_init();
  pixels.begin();
  pixels.clear();
  pixels.setBrightness(80);
  pixels.show();

  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pAdvertising = pServer->getAdvertising();

  init_service();

  // set BLE interval once
  pAdvertising->setMinInterval(16000); // 16000 * 0.625ms ≈ 10s
  pAdvertising->setMaxInterval(16000);
  pAdvertising->start();

  mmWave.setUserLog(0);
  mmWave.setAlamArea(1.50, 1.50, 1.50, 1.50);
  mmWave.setInstallationHeight(height);
  mmWave.setThreshold(threshold);
  mmWave.setSensitivity(sensitivity);

  if (mmWave.getRadarParameters(height, threshold, sensitivity, rect_XL,
                                rect_XR, rect_ZF, rect_ZB)) {
    Serial.printf("Radar init OK: h=%.2f, th=%.2f, sens=%d, rect=(%.2f, %.2f, %.2f, %.2f)\n",
                  height, threshold, sensitivity,
                  rect_XL, rect_XR, rect_ZF, rect_ZB);
  } else {
    Serial.println("Radar init failed");
  }
}

void loop() {
  if (mmWave.update(100)) {
    bool is_human, is_fall;
    mmWave.getHuman(is_human);
    mmWave.getFall(is_fall);

    if (!is_human && !is_fall) status = NO_PEOPLE;
    else if (is_fall)          status = PEOPLE_FALL;
    else                       status = EXIST_PEOPLE;
  }

  // if (millis() - lastPrintMillis >= printInterval) {
  //   lastPrintMillis = millis();
  //   switch (status) {
  //     case NO_PEOPLE:   Serial.println("Waiting for people"); break;
  //     case EXIST_PEOPLE: Serial.println("PEOPLE !!!"); break;
  //     case PEOPLE_FALL: Serial.println("FALL !!!"); break;
  //   }
  // }

  if (millis() - previousMillis >= interval) {
    previousMillis = millis();
    Fall_Frame_Send(fallFrame, status);
    updateAdvertiseData(fallFrame);
    // Serial.println("Updated BLE payload");
  }

  if (status != last_status) {
    switch (status) {
      case NO_PEOPLE:    pixels.setPixelColor(0, pixels.Color(0, 0, 255)); break; // blue
      case EXIST_PEOPLE: pixels.setPixelColor(0, pixels.Color(0, 255, 0)); break; // green
      case PEOPLE_FALL:  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); break; // red
    }
    pixels.show();
    last_status = status;
  }
}
