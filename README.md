# Blueteeth Terminal.
An Arduino sketch of a BLE Terminal that connects to the phone and communicates control commands from the Phone to the Master and Scanner.

The Master is connected to the Terminal via Serial.
The Scanner is connected to the Terminal via Serial2.

## Current issues.
1. The BLE setup allows for changing broadcasted values, which is misaligned with the Serial Terminal approach. Bluetooth Serial Port Profile needs to be configured for correct recognition of the BLEServer as a Terminal. If this issue persists, switch to the BluetoothSerialLibrary and rework to do terminal via BluetoothClassic

2.  Scanner and Master not yet integrated together. Need debugging and testing.


### Dependencies
Original ESP32 board libraries should contain everything.
