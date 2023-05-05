#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>

BLEServer *server = nullptr;

unsigned long lastTime = 0;
unsigned long timerDelay = 1000;

#define HALL_SENSOR_PIN 13
uint32_t revolutions = 0, lastRevolutions = 0;

bool deviceConnected = false;
bool oldDeviceConnected = false;

#define bleServerName "BicycleInfo"
#define SERVICE_UUID "0000dead-0000-1000-8000-00805f9b34fb"

// UnitSpeed Characteristic and Descriptor
BLECharacteristic unitSpeedCharacteristic("0000f4f4-0000-1000-8000-00805f9b34fb", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor unitSpeedDescriptor(BLEUUID((uint16_t)0x2902));

// Revolutions Characteristic and Descriptor
BLECharacteristic revolutionsCharacteristic("0000e4e4-0000-1000-8000-00805f9b34fb", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor revolutionsDescriptor(BLEUUID((uint16_t)0x2903));

//Setup callbacks onConnect and onDisconnect
class CustomServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* server) {
    deviceConnected = true;
  };
  void onDisconnect(BLEServer* server) {
    deviceConnected = false;
  }
};

void initBLEServer() {
  // Create the BLE Device
  BLEDevice::init(bleServerName);

  // Create the BLE Server
  server = BLEDevice::createServer();
  server->setCallbacks(new CustomServerCallbacks());

  // Create the BLE Service
  BLEService *service = server->createService(SERVICE_UUID);

  // Create BLE Characteristics and Descriptors
  service->addCharacteristic(&unitSpeedCharacteristic);
  unitSpeedDescriptor.setValue("UnitSpeed");
  unitSpeedCharacteristic.addDescriptor(new BLE2902());
  
  service->addCharacteristic(&revolutionsCharacteristic);
  revolutionsDescriptor.setValue("Revolutions");
  revolutionsCharacteristic.addDescriptor(new BLE2902());

  // Start the service
  service->start();

  // Start advertising
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  server->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void IRAM_ATTR onRevolution() {
	revolutions++;
}

void setup() {
  Serial.begin(115200);

  initBLEServer();

  // Hall Effect sensor interrupt
  pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);
	attachInterrupt(HALL_SENSOR_PIN, onRevolution, FALLING);
}

void loop() {
  if (deviceConnected) {
    if ((millis() - lastTime) > timerDelay) {
      // UnitSpeed
      // uint32_t max value is 4_294_967_295.0000
      // so we have 10 + 1 (decimal) + 4 = 15 positions
      static char unitSpeedData[20];
      float unitSpeed = (revolutions - lastRevolutions) * PI / (timerDelay / 1000);
      dtostrf(unitSpeed, -1, 4, unitSpeedData);

      unitSpeedCharacteristic.setValue(unitSpeedData);
      unitSpeedCharacteristic.notify();
      
      // Revolutions
      static char revolutionsData[20];
      ltoa(revolutions, revolutionsData, 10);

      revolutionsCharacteristic.setValue(revolutionsData);
      revolutionsCharacteristic.notify();
      
      Serial.print("unitSpeed: ");
      Serial.println(unitSpeed);

      Serial.print("revolutions: ");
      Serial.println(revolutions);

      lastRevolutions = revolutions;
      lastTime = millis();
    }
  }
  
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); // give the bluetooth stack the chance to get things ready
      server->startAdvertising(); // restart advertising
      Serial.println("start advertising");
      oldDeviceConnected = deviceConnected;
  }
  
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}
