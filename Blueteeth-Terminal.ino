#include "Blueteeth-Terminal.h"

using namespace std;

// Master UART variables
char inputBuffer[MAX_INPUT_BUFFER];
SemaphoreHandle_t masterUartMutex;
TaskHandle_t scanTaskHandle;

// BLE Terminal constants
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool terminalDataIncoming = false;
bool masterDataIncoming = false;
uint8_t txValue = 0;

// BLE Scanning variables
BLEScan *pBLEScan = NULL;
bool scanFlag = false;
BLEScanResults foundDevices;

#define SERVICE_UUID "33bf5a58-a402-425c-9133-75bbb8ec7de5" // UART service UUID
#define CHARACTERISTIC_UUID "33bf5a58-a402-425c-9133-75bbb8ec7de5"

#define MASTER_RESPONSE_TIMEOUT 5000

void clear_buffer(char *buffer, int length)
{
  for (int i = 0; i < length; i++)
  {
    buffer[i] = '\0';
  }
}

/*
* Receive data from the Master UART - Master needs to send data fairly fast - within 5 seconds,
* otherwise the Watchdog timer will reset the ESP32
*/
void receiveFromMaster()
{
  clear_buffer(inputBuffer, MAX_INPUT_BUFFER);
  int bufferPos = 0;
  while (masterDataIncoming)
  {
    vTaskDelay(1);
    if (Serial.available() && bufferPos < MAX_INPUT_BUFFER)
    {
      inputBuffer[bufferPos] = Serial.read();
      if (inputBuffer[bufferPos] == '\r')
      {
        inputBuffer[bufferPos] = '\0';
        Serial.print("\n\r");
        bufferPos = -1;
        masterDataIncoming = false;
      }

      else if (inputBuffer[bufferPos] == 127)
      {                                // handle a backspace character
        Serial.printf("%c", 127);      // print out backspace
        inputBuffer[bufferPos] = '\0'; // clear the backspace
        if (bufferPos > 0)
          inputBuffer[--bufferPos] = '\0'; // clear the previous buffer pos if there was another character in the buffer that wasn't a backspace
        bufferPos--;
      }

      else
        Serial.printf("%c", inputBuffer[bufferPos]);
      bufferPos++;
    }
  }
}

/*
 *  Handle the input that arrived from BLESerial
 */
void terminalInput(string input)
{
  String masterResponse;
  Serial.println(input.c_str());

  // Wait for response from Master
  masterDataIncoming = true;
  // TODO: change wdt timeout
  receiveFromMaster();
  masterResponse = inputBuffer;

  Serial.printf("DEBUG: Entered data: %s \n\r", masterResponse);
  pTxCharacteristic->setValue(masterResponse.c_str());
}

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("DEBUG: Device connected!"); // Debugging data for the UART connection
    pTxCharacteristic->setValue("TerminalConnected");
  };

  void onDisconnect(BLEServer *pServer)
  {
    Serial.println("DEBUG: Device disconnected"); // Debugging data for the UART connection
    deviceConnected = false;
    pServer->startAdvertising(); // restart advertising
  }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    string rxValue = pCharacteristic->getValue();
    for (int i = 0; i < rxValue.length(); i++)
    {
      rxValue[i] = tolower(rxValue[i]); // Turn all characters lower case to avoid issues with case sensitivity
    }
    terminalInput(rxValue);
  }
};

void bleServerSetup()
{
  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_INDICATE
      );

  BLEDescriptor *pTxNameDescriptor = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
  pTxNameDescriptor->setValue("BlueteethTerminal");

  BLE2902 *p2902Descriptor = new BLE2902();
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

void setup()
{

  // Start Serial comms
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  Serial.println("Advertising as BlueteethTerminal to connect to a phone...");


  // Create the BLE Device
  BLEDevice::init("BlueteethTerminal");
  bleServerSetup();
}

void loop()
{
}

