// #include "BLE101.h"
// #define SERVICE_UUID           "57d48840-5c28-45a3-ab45-d1b0b046870a"  // UART service UUID
// #define CHARACTERISTIC_UUID_RX "57d48840-5c28-45a3-ab45-d1b0b046870b"
// #define CHARACTERISTIC_UUID_TX "57d48840-5c28-45a3-ab45-d1b0b046870c"

// BLEServer *pServer;
// BLECharacteristic *pCharacteristic;

// void init_service() {
//   BLEAdvertising *pAdvertising;
//   pAdvertising = pServer->getAdvertising();
//   pAdvertising->stop();

//   // Create the BLE Service
//   BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

//   // Create a BLE Characteristic
//   pCharacteristic = pService->createCharacteristic(
//     CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
//   );
//   pCharacteristic->setCallbacks(new MyCallbacks());
//   pCharacteristic->addDescriptor(new BLE2902());

//   pAdvertising->addServiceUUID(BLEUUID(SERVICE_UUID));

//   // Start the service
//   pService->start();

//   pAdvertising->start();
// }

// void init_beacon() {
//   BLEAdvertising *pAdvertising;
//   pAdvertising = pServer->getAdvertising();
//   pAdvertising->stop();
//   // iBeacon
//   BLEBeacon myBeacon;
//   myBeacon.setManufacturerId(0x4c00);
//   myBeacon.setMajor(5);
//   myBeacon.setMinor(88);
//   myBeacon.setSignalPower(0xc5);
//   myBeacon.setProximityUUID(BLEUUID(BEACON_UUID_REV));

//   BLEAdvertisementData advertisementData;
//   advertisementData.setFlags(0x1A);
//   advertisementData.setManufacturerData(myBeacon.getData());
//   pAdvertising->setAdvertisementData(advertisementData);

//   pAdvertising->start();
// }



