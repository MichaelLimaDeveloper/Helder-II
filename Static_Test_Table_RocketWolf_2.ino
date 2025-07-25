#include <HX711_ADC.h>
#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif
#include <SPI.h>
#include <SD.h>
#include "BluetoothSerial.h"
#include <LoRa.h>  // Adicionando a biblioteca LoRa

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run make menuconfig to and enable it
#endif

BluetoothSerial SerialBT;

//pins:
const int HX711_dout = 4;  //mcu > HX711 dout pin
const int HX711_sck = 5;   //mcu > HX711 sck pin

//SD card pins
const int CS = 21;

// LoRa pins
const int LoRa_CS = 18;  // Pino CS do LoRa
const int LoRa_RST = 23; // Pino RST do LoRa
const int LoRa_IRQ = 26; // Pino IRQ do LoRa

File myFile;

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_eepromAdress = 0;
unsigned long t = 0;

void setup() {
  Serial.begin(57600);
  SerialBT.begin("RocketWolf Static Test ESP32"); // Iniciando Bluetooth
  delay(10);
  Serial.println();
  Serial.println("Starting...");

  // Inicializando LoRa
  LoRa.begin(915E6); // Frequência LoRa (ajuste conforme necessário)
  LoRa.setPins(LoRa_CS, LoRa_RST, LoRa_IRQ); // Configurando pinos do LoRa

  LoadCell.begin();
  unsigned long stabilizingtime = 2000;  // precision right after power-up can be improved by adding a few seconds of stabilizing time
  boolean _tare = true;                  // set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    SerialBT.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  } else {
    LoadCell.setCalFactor(1.0);  // user set calibration value (float), initial value 1.0 may be used for this sketch
    Serial.println("Startup is complete");
    SerialBT.println("Startup is complete");
  }
  while (!LoadCell.update());

  calibrate();  // start calibration procedure

  // Initialize SD card
  Serial.println("Initializing SD card...");
  SerialBT.println("Initializing SD card...");
  if (!SD.begin(CS)) {
    Serial.println("initialization failed!");
    SerialBT.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  SerialBT.println("initialization done.");
  myFile = SD.open("/resultado.txt", FILE_WRITE);
  if (myFile) {
    myFile.println("Teste Iniciado");
  }
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0;  // increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      Serial.print("Load_cell output val: ");
      Serial.println(i);
      SerialBT.print("Load_cell output val: ");
      SerialBT.println(i);
      sendLoRaData(i); // Enviando dados via LoRa
      logToSD(i);  // Grava os dados no arquivo log2.txt
      newDataReady = 0;
      t = millis();
    }
  }

  // receive command from serial terminal
  if (Serial.available() > 0 || SerialBT.available() > 0) {
    char inByte;
    if (Serial.available() > 0) {
      inByte = Serial.read();
    } else if (SerialBT.available() > 0) {
      inByte = SerialBT.read();
    }
    if (inByte == 't') LoadCell.tareNoDelay();       // tare
    else if (inByte == 'r') calibrate();             // calibrate
    else if (inByte == 'c') changeSavedCalFactor();  // edit calibration value manually
  }

  // check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
    SerialBT.println("Tare complete");
  }
}

void sendLoRaData(float value) {
  LoRa.beginPacket();
  LoRa.print("Load_cell output val: ");
  LoRa.println(value);
  LoRa.endPacket();
}

void calibrate() {
  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("Place the load cell on a level stable surface.");
  Serial.println("Remove any load applied to the load cell.");
  Serial.println("Send 't' from serial monitor to set the tare offset.");

  SerialBT.println("***");
  SerialBT.println("Start calibration:");
  SerialBT.println("Place the load cell on a level stable surface.");
  SerialBT.println("Remove any load applied to the load cell.");
  SerialBT.println("Send 't' from serial monitor to set the tare offset.");

  boolean _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0 || SerialBT.available() > 0) {
      char inByte;
      if (Serial.available() > 0) {
        inByte = Serial.read();
      } else if (SerialBT.available() > 0) {
        inByte = SerialBT.read();
      }
      if (inByte == 't') LoadCell.tareNoDelay();
    }
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
      SerialBT.println("Tare complete");
      _resume = true;
    }
  }

  Serial.println("Now, place your known mass on the load cell.");
  Serial.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");
  SerialBT.println("Now, place your known mass on the load cell.");
  SerialBT.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");

  float known_mass = 0;
  _resume = false;
  while (_resume == false) {
    LoadCell.update();
    if (Serial.available() > 0 || SerialBT.available() > 0) {
      if (Serial.available() > 0) {
        known_mass = Serial.parseFloat();
      } else if (SerialBT.available() > 0) {
        known_mass = SerialBT.parseFloat();
      }
      if (known_mass != 0) {
        Serial.print("Known mass is: ");
        Serial.println(known_mass);
        SerialBT.print("Known mass is: ");
        SerialBT.println(known_mass);
        _resume = true;
      }
    }
  }

  LoadCell.refreshDataSet();                                           //refresh the dataset to be sure that the known mass is measured correct
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass);  //get the new calibration value

  Serial.print("New calibration value has been set to: ");
  Serial.print(newCalibrationValue);
  Serial.println(", use this as calibration value (calFactor) in your project sketch.");
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");

  SerialBT.print("New calibration value has been set to: ");
  SerialBT.print(newCalibrationValue);
  SerialBT.println(", use this as calibration value (calFactor) in your project sketch.");
  SerialBT.print("Save this value to EEPROM address ");
  SerialBT.print(calVal_eepromAdress);
  SerialBT.println("? y/n");

  _resume = false;
  while (_resume == false) {
    if (Serial.available() > 0 || SerialBT.available() > 0) {
      char inByte;
      if (Serial.available() > 0) {
        inByte = Serial.read();
      } else if (SerialBT.available() > 0) {
        inByte = SerialBT.read();
      }
      if (inByte == 'y') {
#if defined(ESP8266) || defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266) || defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        Serial.print("Value ");
        Serial.print(newCalibrationValue);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress);
        SerialBT.print("Value ");
        SerialBT.print(newCalibrationValue);
        SerialBT.print(" saved to EEPROM address: ");
        SerialBT.println(calVal_eepromAdress);
        _resume = true;

      } else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        SerialBT.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }

  Serial.println("End calibration");
  Serial.println("***");
  Serial.println("To re-calibrate, send 'r' from serial monitor.");
  Serial.println("For manual edit of the calibration value, send 'c' from serial monitor.");
  Serial.println("***");

  SerialBT.println("End calibration");
  SerialBT.println("***");
  SerialBT.println("To re-calibrate, send 'r' from serial monitor.");
  SerialBT.println("For manual edit of the calibration value, send 'c' from serial monitor.");
  SerialBT.println("***");
}

void changeSavedCalFactor() {
  float oldCalibrationValue = LoadCell.getCalFactor();
  boolean _resume = false;
  Serial.println("***");
  Serial.print("Current value is: ");
  Serial.println(oldCalibrationValue);
  Serial.println("Now, send the new value from serial monitor, i.e. 696.0");

  SerialBT.println("***");
  SerialBT.print("Current value is: ");
  SerialBT.println(oldCalibrationValue);
  SerialBT.println("Now, send the new value from serial monitor, i.e. 696.0");

  float newCalibrationValue;
  while (_resume == false) {
    if (Serial.available() > 0 || SerialBT.available() > 0) {
      if (Serial.available() > 0) {
        newCalibrationValue = Serial.parseFloat();
      } else if (SerialBT.available() > 0) {
        newCalibrationValue = SerialBT.parseFloat();
      }
      if (newCalibrationValue != 0) {
        Serial.print("New calibration value is: ");
        Serial.println(newCalibrationValue);
        SerialBT.print("New calibration value is: ");
        SerialBT.println(newCalibrationValue);
        LoadCell.setCalFactor(newCalibrationValue);
        _resume = true;
      }
    }
  }
  _resume = false;
  Serial.print("Save this value to EEPROM address ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? y/n");

  SerialBT.print("Save this value to EEPROM address ");
  SerialBT.print(calVal_eepromAdress);
  SerialBT.println("? y/n");

  while (_resume == false) {
    if (Serial.available() > 0 || SerialBT.available() > 0) {
      char inByte;
      if (Serial.available() > 0) {
        inByte = Serial.read();
      } else if (SerialBT.available() > 0) {
        inByte = SerialBT.read();
      }
      if (inByte == 'y') {
#if defined(ESP8266) || defined(ESP32)
        EEPROM.begin(512);
#endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
#if defined(ESP8266) || defined(ESP32)
        EEPROM.commit();
#endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        Serial.print("Value ");
        Serial.print(newCalibrationValue);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress);
        SerialBT.print("Value ");
        SerialBT.print(newCalibrationValue);
        SerialBT.print(" saved to EEPROM address: ");
        SerialBT.println(calVal_eepromAdress);
        _resume = true;
      } else if (inByte == 'n') {
        Serial.println("Value not saved to EEPROM");
        SerialBT.println("Value not saved to EEPROM");
        _resume = true;
      }
    }
  }

  Serial.println("End change calibration value");
  Serial.println("***");

  SerialBT.println("End change calibration value");
  SerialBT.println("***");
}

void logToSD(float value) {
  if (myFile) {
    myFile.print("Load_cell output val: ");
    myFile.println(value);
    myFile.flush();  // Garante que os dados sejam gravados no cartão SD
  }
}

void iniciaSD(void) {
  myFile = SD.open("/resultado.txt", FILE_WRITE);
}
