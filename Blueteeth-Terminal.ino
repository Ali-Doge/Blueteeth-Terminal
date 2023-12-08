#include "Blueteeth-Terminal.h"

using namespace std;

// BLE Terminal constants
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

#define SERVICE_UUID        "33bf5a58-a402-425c-9133-75bbb8ec7de5" // UART service UUID
#define CHARACTERISTIC_UUID "33bf5a58-a402-425c-9133-75bbb8ec7de5"

/*
* Scan for BLE servers and find the first one that advertises the service we are looking for.
*/

void scan(){
  BLEScan* pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  BLEScanResults foundDevices = pBLEScan->start(5); //5 second scan duration
  int count = foundDevices.getCount();
  Serial.print("Found ");
  Serial.print(count);
  Serial.println(" devices");
  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice d = foundDevices.getDevice(i);
    Serial.print(i);
    Serial.print(": ");
    Serial.print(d.getAddress().toString().c_str());
    Serial.print(" ");
    Serial.println(d.getName().c_str());
  }

  // Reconnect to the last connected device
  if (!deviceConnected) {
    Serial.println("DEBUG: Restarting advertising...");
    pServer->startAdvertising(); // restart advertising
  }

}

string receiveFromMaster() {
  delay(100);
  int bufferPos = 0;
  char inputBuffer[MAX_INPUT_BUFFER];
  string masterResponse;
  while (Serial.available() && bufferPos < MAX_INPUT_BUFFER) {
    inputBuffer[bufferPos] = Serial.read();
    if (inputBuffer[bufferPos] == '\r') {
      inputBuffer[bufferPos] = '\0';
      masterResponse = inputBuffer;
      bufferPos = 0;
      //clear the input buffer
      inputBuffer[0] = '\0';
      return masterResponse;
    }
    else {
      Serial.printf("%c", inputBuffer[bufferPos]);
      bufferPos++;
    }
  }
  return masterResponse;
}

/*
*  Handle the input that arrived from BLESerial
*/
void terminalInput(string input) {
  string masterResponse;
  if (input.compare("help") == 0) {
      pTxCharacteristic->setValue("help command");
      pTxCharacteristic->notify();
    /*
      Serial.println("BlueteethTerminal");
      Serial.println("help - Show this help");

      // Terminal/Scanner Functionality
      Serial.println("ping - Ping the slaves to get the BlueteethSlaveAddresses");
      Serial.println("scan - Scan for speakers");
      Serial.println("select a,b,c - Select speakers from the scan");
      Serial.println("list - Print the list of the selected speakers as an enumerated map");

      // Master connection functionality
      Serial.println("connect - Send the the list to BlueteethMaster and connect to the selected speakers");
      Serial.println("disconnect - Disconnect a speaker from the list");
      */
  }
  else if (input.compare("ping") == 0) {
      pTxCharacteristic->setValue("ping command");
      pTxCharacteristic->notify();
      Serial.println("DEBUG: Pinging slaves..."); 
      // UART ping request sent to Master
      Serial.println("ping");

      // Wait for response from Master
      Serial.println("DEBUG: Enter some data: ");
      masterResponse = receiveFromMaster();
      
      Serial.println("DEBUG: master: ");

      Serial.println(masterResponse.c_str());


      pTxCharacteristic->setValue(masterResponse.c_str());
      pTxCharacteristic->notify();


  }
  else if (input.compare("scan") == 0) {
      Serial.println("DEBUG: Scanning for speakers...");


      pTxCharacteristic->setValue("scan command");
      pTxCharacteristic->notify();
      scan();
  }
  else if (input.compare("select") == 0) {
      Serial.println("DEBUG: Selecting speakers...");
      pTxCharacteristic->setValue("select command");
      pTxCharacteristic->notify();
      //select();
  }
  else if (input.compare("list") == 0) {
      Serial.println("DEBUG: Listing speakers...");
      pTxCharacteristic->setValue("list command");
      pTxCharacteristic->notify();
      //list();
  }
  else if (input.compare("connect") == 0) {
      Serial.println("Connecting to speakers...");
      pTxCharacteristic->setValue("connect command");
      pTxCharacteristic->notify();

      //connect(); //Send connect requests via UART 1-by-1 with the directive "connect BlueteethAddress,SpeakerName""
  }
  else if (input.compare("disconnect") == 0) {
      Serial.println("Disconnecting from speakers...");
      //disconnect(); //However this is done
  }
}


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("DEBUG: Device connected!");  // Debugging data for the UART connection
      pTxCharacteristic->setValue("TerminalConnected");
      pTxCharacteristic->notify();
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("DEBUG: Device disconnected");  // Debugging data for the UART connection
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      string rxValue = pCharacteristic->getValue();
      for (int i = 0; i < rxValue.length(); i++) {
        rxValue[i] = tolower(rxValue[i]); // Turn all characters lower case to avoid issues with case sensitivity
      }
      terminalInput(rxValue);
    }
};


void setup() {

  //Start Serial comms
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  Serial.println("Advertising as BlueteethTerminal to connect to a phone...");

  BLEDevice::init("BlueteethTerminal");
  
  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
			CHARACTERISTIC_UUID,
			BLECharacteristic::PROPERTY_READ    | 
      BLECharacteristic::PROPERTY_WRITE   |
      BLECharacteristic::PROPERTY_NOTIFY  |
      BLECharacteristic::PROPERTY_INDICATE
			);

  BLEDescriptor *pTxNameDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
  pTxNameDescriptor->setValue("BlueteethTerminal");

  BLE2902* p2902Descriptor = new BLE2902();
  p2902Descriptor->setNotifications(true); 
  pTxCharacteristic->addDescriptor(p2902Descriptor);
  pTxCharacteristic->addDescriptor(pTxNameDescriptor);
  pTxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

}

void loop() {
}










