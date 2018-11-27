void checkWinStatus() {
  if(millis() - winTime > winLengthMs)
  {
    state = state & ~(WIN_STATE);
  }
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}

void set_leds(byte state) {
  CHSV ledsCHSV[NUM_LEDS];
  // state over 127 means WIN_STATE was reached, play victory sequence
  if (state > 127) {
    {
      fill_rainbow(leds, NUM_LEDS, beat8(60), 256 / NUM_LEDS);
      if(random8() < 64) {
        leds[random(NUM_LEDS)] = CRGB::White;
      }
    }
    return;
  }
  // after special cases state can be checked for COLOR, PATTERN and MOTION and set leds accordingly
  switch (state & COLOR_FIELD_MASK) {
    case 0x00 :
      fill_solid(ledsCHSV, NUM_LEDS, CHSV(0 * 256 / 4, 255, 255));
      break;
    case 0x01:
      fill_solid(ledsCHSV, NUM_LEDS, CHSV(1 * 256 / 4, 255, 255));
      break;
    case 0x02:
      fill_solid(ledsCHSV, NUM_LEDS, CHSV(2 * 256 / 4, 255, 255));
      break;
    case 0x03:
      fill_solid(ledsCHSV, NUM_LEDS, CHSV(3 * 256 / 4, 255, 255));
      break;
  }
  switch ( (state & MOTION_FIELD_MASK) >> 4) {
    case 0x00:
      {
        // blink
        uint8_t brightness = beatsin8(30, 64, 255);
        for(int i=0; i<NUM_LEDS; i++) {
          ledsCHSV[i].val = brightness;
        }
      }
      break;
    case 0x01:
      {
        uint8_t snakeHeadLoc = beat8(30) / NUM_LEDS;
        for (byte i=0; i < NUM_LEDS; i++) {
          uint8_t distanceFromHead = (i - snakeHeadLoc + NUM_LEDS) % NUM_LEDS;
          const int snakeLength = 8;
          uint8_t brightness = distanceFromHead > snakeLength ? 255 : 255 - ((int)distanceFromHead * (256 / snakeLength));
          ledsCHSV[i].val = brightness;
        }
      }
      break;
    case 0x02:
      // static
      break;
    case 0x03:
      //flicker
      {
        static bool isOn = true;
        if(random8() < (isOn ? 40 : 24))
          isOn = !isOn;
        uint8_t brightness = isOn ? 255 : 0;
        for(int i=0; i<NUM_LEDS; i++) {
          ledsCHSV[i].val = brightness;
        }
      }
      break;
  }
  switch ((state & PATTERN_FIELD_MASK) >> 2) {
    case 0x00 :
      {
        // Turn off even leds for dotted pattern
        for (byte i=0; i < NUM_LEDS/2; i++) {
          ledsCHSV[i*2].val = 0;
        }
      }
      break;
    case 0x01:
      {
        // Turn off first half
        for (byte i=0; i < NUM_LEDS/2; i++) {
          ledsCHSV[i].val = 0;
        }
      }
      break;
    case 0x02:
      {
         // full
      }
      break;
    case 0x03:
      {
        // Turn off quarters
        for (byte i=0; i < NUM_LEDS/4; i++) {
          ledsCHSV[i].val = 0;
          ledsCHSV[NUM_LEDS/2 + i].val = 0;
        }
      }
      break;
  }
  
  for(int i=0; i<NUM_LEDS; i++) {
    leds[i] = (CRGB)(ledsCHSV[i]);
  }
}

bool authenticate(byte trailerBlock, MFRC522::MIFARE_Key key) {
    MFRC522::StatusCode status;

    // Authenticate using key A
    Serial.println(F("Authenticating using key A..."));
    status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("PCD_Authenticate() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        return false;
    }
    return true;
}

bool read_block(byte blockAddr, byte buffer[], byte size) {
    MFRC522::StatusCode status;
    
    // Read data from the block
    Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Read() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        return false;
    }
    Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
    dump_byte_array(buffer, 16); Serial.println();
    Serial.println();
    return true;
}

bool write_and_verify(byte blockAddr, byte dataBlock[], byte buffer[], byte size) {
    MFRC522::StatusCode status;

    // Authenticate using key A
    //Serial.println(F("Authenticating using key A..."));
    //status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
    //if (status != MFRC522::STATUS_OK) {
    //    Serial.print(F("PCD_Authenticate() failed: "));
    //    Serial.println(mfrc522.GetStatusCodeName(status));
    //    return false;
    //}
    
    // Write data to the block
    Serial.print(F("Writing data into block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    dump_byte_array(dataBlock, 16); Serial.println();
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockAddr, dataBlock, 16);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Write() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        return false;
    }
    Serial.println();

    // Read data from the block (again, should now be what we have written)
    Serial.print(F("Reading data from block ")); Serial.print(blockAddr);
    Serial.println(F(" ..."));
    status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Read() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        return false;
    }
    Serial.print(F("Data in block ")); Serial.print(blockAddr); Serial.println(F(":"));
    dump_byte_array(buffer, 16); Serial.println();

    // Check that data in block is what we have written
    // by counting the number of bytes that are equal
    Serial.println(F("Checking result..."));
    byte count = 0;
    for (byte i = 0; i < 16; i++) {
        // Compare buffer (= what we've read) with dataBlock (= what we've written)
        if (buffer[i] == dataBlock[i])
            count++;
    }
    Serial.print(F("Number of bytes that match = ")); Serial.println(count);
    if (count == 16) {
        Serial.println(F("Success :-)"));
        return true;
    } else {
        Serial.println(F("Failure, no match :-("));
        Serial.println(F("  perhaps the write didn't work properly..."));
        Serial.println();
        return false;
    }
}
