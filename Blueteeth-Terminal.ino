#include "Blueteeth-Terminal.h"

using namespace std;

#define SERVICE_UUID "33bf5a58-a402-425c-9133-75bbb8ec7de5" // UART service UUID
#define CHARACTERISTIC_UUID "33bf5a58-a402-425c-9133-75bbb8ec7de5"

#define SERIAL_RESPONSE_TIMEOUT 7000
#define MAX_INPUT_BUFFER 100

// UART variables
char scannerBuffer[MAX_INPUT_BUFFER];
char masterBuffer[MAX_INPUT_BUFFER];
bool scannerDataIncoming = false;
bool masterDataIncoming = false;

// BLE Terminal constants
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// Clear the buffer
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
void receiveFromSerial(HardwareSerial serial, bool flag, char inputBuffer[])
{
  clear_buffer(inputBuffer, MAX_INPUT_BUFFER);
  int bufferPos = 0;
  int now = millis();
  while (flag)
  {
    vTaskDelay(1);
    if (millis() - now > SERIAL_RESPONSE_TIMEOUT) 
    {
      flag = false;
      Serial.print("Timeout occured\n\r");
      break;
    }
    if (serial.available() && bufferPos < MAX_INPUT_BUFFER)
    {
      serial.println("DEBUG: Data received from UART");
      inputBuffer[bufferPos] = serial.read();
      if (inputBuffer[bufferPos] == '\n')
      {
        inputBuffer[bufferPos] = '\0';
        serial.print("Carriage return case caught\n\r");
        serial.print("\n\r");
        bufferPos = -1;
        flag = false;
      }
      else if (inputBuffer[bufferPos] == 127)
      {                                // handle a backspace character
        serial.printf("%c", 127);      // print out backspace
        inputBuffer[bufferPos] = '\0'; // clear the backspace
        if (bufferPos > 0)
          inputBuffer[--bufferPos] = '\0'; // clear the previous buffer pos if there was another character in the buffer that wasn't a backspace
        bufferPos--;
      }

      else serial.printf("%c", inputBuffer[bufferPos]);
      bufferPos++;
    }
  }
}

/*
 *  Handle the input that arrived from BLESerial
 */
void terminalInput(string input)
{
  if (input.compare("scan") == 0)
  {
    Serial.println("DEBUG: Scanning for speakers...");
    Serial2.print("s"); // Send scan request via UART
    pTxCharacteristic->setValue("scan command");
    scannerDataIncoming = true;
    receiveFromSerial(Serial2, scannerDataIncoming, scannerBuffer);
    Serial.printf("DEBUG: Entered data: %s \n\r", masterBuffer);
    pTxCharacteristic->setValue(masterBuffer);
  }
  else if (input.compare("clear") == 0)
  {
    Serial.println("DEBUG: Clearing the scanned list of speakers...");
    Serial2.print("c"); // Send scan request via UART
    pTxCharacteristic->setValue("clear command");
  }
  else 
  {
    // UART ping request sent to Master
    Serial.printf("%s\n\r",input.c_str());
    // Wait for response from Master
    masterDataIncoming = true;
    receiveFromSerial(Serial, masterDataIncoming, masterBuffer);

    Serial.printf("DEBUG: Entered data: %s \n\r", masterBuffer);
    pTxCharacteristic->setValue(masterBuffer);
  }
}

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("DEBUG: Device connected!"); // Debugging data for the UART connection
    delay(500);
    pTxCharacteristic->setValue(
      "Available commands: ping, scan, select, list, connect, disconnect\n\r \
      BlueteethTerminal\n\r \
      help - Show this help\n\r \
      ping - Ping the slaves to get the BlueteethSlaveAddresses\n\r \
      scan - Scan for speakers\n\r \
      select a,b,c - Select speakers from the scan\n\r \
      list - Print the list of the selected speakers as an enumerated map\n\r \
      connect - Send the the list to BlueteethMaster and connect to the selected speakers\n\r \
      disconnect - Disconnect a speaker from the list\n\r");
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
          BLECharacteristic::PROPERTY_INDICATE);

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
  // Start Master Serial comms
  Serial.begin(115200);

  // Start Scan Serial Comms
  Serial2.begin(115200);

  // Create the BLE Device
  BLEDevice::init("BlueteethTerminal");
  bleServerSetup();
}

void loop()
{
}

