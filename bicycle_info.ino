#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>

BLEServer *server = nullptr;
unsigned long timerDelay = 500, lastTimer = 0;

#define BUFFER_SAMPLES 3
float speedBuffer[BUFFER_SAMPLES] = {0.f};
uint8_t bufferIndex = 0;

unsigned long currTime = 0, lastTime = 0;
bool last = false;

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

  if (last) lastTime = millis();
  else currTime = millis();

  last = !last;
}

void setup() {
  Serial.begin(115200);

  //Bluetooth Low Energy server init 
  initBLEServer();

  // Hall Effect sensor interrupt
  pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);
	attachInterrupt(HALL_SENSOR_PIN, onRevolution, FALLING);
}

void loop() {
  if (deviceConnected) {
    if ((millis() - lastTimer) > timerDelay) {
      // UnitSpeed
      unsigned long timeDiff = currTime > lastTime ? currTime - lastTime : lastTime - currTime;
      float unitSpeed = 0;

      if (timeDiff != 0 && lastRevolutions != revolutions) {
        unitSpeed = PI / (timeDiff / 1000.f);
      }

      speedBuffer[bufferIndex++] = unitSpeed;
      if (bufferIndex > BUFFER_SAMPLES) bufferIndex = 0; 

      for (int i = 0; i < BUFFER_SAMPLES; i++) {
        unitSpeed += speedBuffer[i];
      }
      unitSpeed /= BUFFER_SAMPLES;
      
      // uint32_t max value is 4_294_967_295.0000
      // so we have 10 + 1 (decimal) + 4 = 15 positions
      static char unitSpeedData[20];
      dtostrf(unitSpeed, -1, 4, unitSpeedData);
      
      unitSpeedCharacteristic.setValue(unitSpeedData);
      unitSpeedCharacteristic.notify();
      
      // Revolutions
      static char revolutionsData[20];
      ltoa(revolutions, revolutionsData, 10);

      revolutionsCharacteristic.setValue(revolutionsData);
      revolutionsCharacteristic.notify();

      Serial.print("unitSpeed (km/h): ");
      Serial.println(unitSpeed*3.6);

      Serial.print("revolutions: ");
      Serial.println(revolutions);

      lastRevolutions = revolutions;
      lastTimer = millis();
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
