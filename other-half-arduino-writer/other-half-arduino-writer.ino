/*
 * --------------------------------------------------------------------------------------------------------------------
 * The outpost station sketch for the RFID set match game
 * --------------------------------------------------------------------------------------------------------------------
 * This sketch uses the MFRC522 library; for further details see: https://github.com/miguelbalboa/rfid

 * @license Released into the public domain.
 * 
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS      SDA(SS)      10            53        D10        10               10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 *
 */

#include <SPI.h>
#include <MFRC522.h>
#include <FastLED.h>

#define NUM_LEDS 64
#define DATA_PIN 4

// Define the array of leds
CRGB leds[NUM_LEDS];

#define WIN_STATE          0x80
#define VALID_STATE        0x40
#define COLOR_FIELD_MASK   0x03
#define PATTERN_FIELD_MASK 0x0C
#define MOTION_FIELD_MASK  0x30

#define RST_PIN         9           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

MFRC522::MIFARE_Key key;
//MFRC522::StatusCode status;
byte sector         = 1;
byte blockAddr      = 4;
byte dataBlock[]    = {
        0x01, 0x02, 0x03, 0x04, //  1,  2,   3,  4,
        0x05, 0x06, 0x07, 0x08, //  5,  6,   7,  8,
        0x09, 0x0a, 0xff, 0x0b, //  9, 10, 255, 11,
        0x0c, 0x0d, 0x0e, 0x0f  // 12, 13, 14, 15
    };
byte trailerBlock   = 7;
byte buffer[18];
byte size = sizeof(buffer);
bool read_success, write_success, auth_success;

#define INITIAL_COLOR 0x0
#define INITIAL_PATTERN 0x0
#define INITIAL_MOTION 0x0

byte state = INITIAL_COLOR << 0 | INITIAL_PATTERN << 2 | INITIAL_MOTION << 4;
byte new_state = 0;

unsigned long winTime = 0;
const int winLengthMs = 5000;
 byte power_mask = 0xFC; // for encoding color power 0x00, 0x01, 0x02, 0x03
// byte power_mask = 0xF3; // for encoding pattern power 0x00, 0x04, 0x08, 0x0C
//byte power_mask = 0xCF; // for encoding motion power 0x00, 0x10, 0x20, 0x30
// byte power = 0x00;
// byte power = 0x01;
// byte power = 0x02;
 byte power = 0x03;
// byte power = 0x04;
// byte power = 0x08;
// byte power = 0x0C;
// byte power = 0x10;
// byte power = 0x20;
//byte power = 0x30;
byte mission = 0x00;
byte PICC_version;

unsigned int readCard[4];

#include "touch-me-arduino.h"

void setup() {
    Serial.begin(9600); // Initialize serial communications with the PC
    while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
    SPI.begin();        // Init SPI bus
    mfrc522.PCD_Init(); // Init MFRC522 card
    mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
    Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
    // Prepare the key (used both as key A and as key B)
    // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

    // Serial.println(F("Scan a MIFARE Classic PICC to demonstrate read and write."));
    // Serial.print(F("Using key (for A and B):"));
    //dump_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);
    Serial.println();
    Serial.println("Touch me game tag writer");

    Serial.println(F("BEWARE: Data will be written to the PICC, in sector #1"));

    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(32);

    state = state | VALID_STATE;
}

/**
 * Main loop.
 */
void loop() {
    // advance leds first
    set_leds(state);
    checkWinStatus();
    FastLED.show();
    FastLED.delay(20);

    PICC_version = 0;
    PICC_version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    // Look for new cards
    if ( ! mfrc522.PICC_IsNewCardPresent())
        return;

    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial())
        return;
    for (int i = 0; i < mfrc522.uid.size; i++) {  // for size of uid.size write uid.uidByte to readCard
      readCard[i] = mfrc522.uid.uidByte[i];
      Serial.print(readCard[i], HEX);
    }

    // Show some details of the PICC (that is: the tag/card)
    Serial.print(F("Card UID:"));
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
    Serial.print(F("PICC type: "));
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    Serial.println(mfrc522.PICC_GetTypeName(piccType));

    // Check for compatibility
    if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        Serial.println(F("This sample only works with MIFARE Classic cards."));
        return;
    }

    // perform authentication to open communication
    auth_success = authenticate(trailerBlock, key);
    if (!auth_success) {
      Serial.println(F("Authentication failed"));
      return;
    }
    
    // read the tag to get coded information
    read_success = read_block(blockAddr, buffer, size);
    if (!read_success) {
      Serial.println(F("Initial read failed, closing connection"));
      // Halt PICC
      mfrc522.PICC_HaltA();
      // Stop encryption on PCD
      mfrc522.PCD_StopCrypto1();
      return;
    }
    else {
      // Show the whole sector as it currently is
      Serial.println(F("Current data in sector:"));
      mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
      Serial.println();
      
      //power_mask = buffer[0];
      //power = buffer[1];
      //mission = buffer[2];
    Serial.print(F("Read power mask ")); Serial.print(power_mask < 0x10 ? " 0" : " "); Serial.println(power_mask, HEX);
    Serial.print(F("Read power ")); Serial.print(power < 0x10 ? " 0" : " "); Serial.println(power, HEX);
    Serial.print(F("Read mission ")); Serial.print(mission < 0x10 ? " 0" : " "); Serial.println(mission, HEX);
    }
    
    
    
    FastLED.clear();
    FastLED.show();

    // Authenticate using key B
    //Serial.println(F("Authenticating again using key B..."));
    //status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, &key, &(mfrc522.uid));
    //if (status != MFRC522::STATUS_OK) {
    //    Serial.print(F("PCD_Authenticate() failed: "));
    //    Serial.println(mfrc522.GetStatusCodeName(status));
    //    return;
    //}

    new_state = 0; // zeroing new_state so we start fresh and not have residuals from last runs
    dataBlock[0] = power_mask;
    dataBlock[1] = power;
    dataBlock[2] = mission;

    new_state = state & power_mask;
    //Serial.println(new_state, HEX);
    new_state = new_state | power | VALID_STATE;
    //Serial.println(new_state, HEX);
    
    //if (new_state == mission) {
      write_success = write_and_verify(blockAddr, dataBlock, buffer, size);
      if (!write_success) {
        Serial.println(F("write failed, keeping previous state"));
      }
      else {
        Serial.println(F("write worked, applying new state"));
        state = new_state;
        winTime = millis();
      }
    //}
    
    delay(100);
    set_leds(state);
    Serial.print(F("current state is ")); Serial.print(state < 0x10 ? " 0" : " "); Serial.println(state, HEX);

    // Dump the sector data
    //Serial.println(F("Current data in sector:"));
    //mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
    //Serial.println();

    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
}
