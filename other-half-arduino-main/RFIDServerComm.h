#include <Ethernet.h>

class RFIDServerComm {

  public:

  byte handle_socket(IPAddress ipAddr) {

    bool isConnected = tryToConnect(ipAddr);
    if(!isConnected)
      return NO_MSG;
      
    return readFromSocket();
  }
  
  void handle_socket_heartbeat(IPAddress ipAddr) {

    bool isConnected = tryToConnect(ipAddr);
    if(!isConnected)
      return;
      
    readFromSocket();
    sendHeartBeat();
    checkHeartBeat();
  }

  void handle_socket_tag(IPAddress ipAddr) {

    bool isConnected = tryToConnect(ipAddr);
    if(!isConnected)
      return;

    sendTagInfo();
    byte msg = 0;
    while (msg != TAG_RESPONSE_MSG){
      msg = readFromSocket();
    }
  }

  void handle_socket_write_status(IPAddress ipAddr, byte writeStatus) {

    bool isConnected = tryToConnect(ipAddr);
    if(!isConnected)
      return;

    sendWriteStatus(writeStatus);
  }
  
  private:

  bool tryToConnect(IPAddress ipAddr) {
    if(client.connected()) {
      return true;
    }

    client.stop();
    if (client.connect(ipAddr, 5007)) {
      Serial.println("successfully connected to the RFID server");
      lastReadTime = millis();
      return true;
    } else {
      Serial.println("no connection to the RFID server");
      return false;
    }
  }

  void sendHeartBeat() {
    // Serial.println("TCP socket connected - sending heartbeat to server");
    byte buf[MSG_LENGTH];
    buf[0] = HEARTBEAT_MSG;
    buf[1] = PICC_version;
    for(int i=2; i<8; i++) buf[i] = 0;
    client.write(buf, MSG_LENGTH);    
  }

  void sendTagInfo() {
    Serial.println("sending tag info to server");
    byte buf[MSG_LENGTH];
    buf[0] = TAG_INFO_MSG;
    for(int i=1; i<5; i++)
      buf[i] = (byte)readCard[i];
    buf[5] = mission;
    buf[6] = power;
    buf[7] = power_mask;
    client.write(buf, MSG_LENGTH);    
  }

  void sendWriteStatus(byte writeStatus) {
    Serial.println("sending write status to server");
    byte buf[MSG_LENGTH];
    buf[0] = WRITE_STATUS_MSG;
    buf[1] = writeStatus;
    for(int i=2; i<8; i++) buf[i] = 0;
    client.write(buf, MSG_LENGTH);    
  }
  
  byte readFromSocket() {
    if (client.available()) {
      lastReadTime = millis();
      byte i = 0;
      byte read_buf[MSG_LENGTH];
      while(client.available() && i < MSG_LENGTH) {
        read_buf[i] = client.read();
        i++;
      }
      Serial.print("got msg from server: ");
      for (int i=0; i<MSG_LENGTH; i++) {
        Serial.print(read_buf[i] < 0x10 ? " 0" : " "); Serial.print(read_buf[i], HEX);
      }
      Serial.println();
      // Three msg types supported TAG_RESPONSE, SHOW_LEDS and HEARTBEAT
      if (read_buf[0] == TAG_RESPONSE_MSG) {
        mission_command = read_buf[1];
        mission = read_buf[2];
        return TAG_RESPONSE_MSG;
      }
      else if (read_buf[0] == SHOW_LEDS_MSG) {
        master_state = read_buf[1];
        return SHOW_LEDS_MSG;
      }
      else if (read_buf[0] == HEARTBEAT_MSG) {
        // we dont really do anything here because the lastReadTime is updated for all packets
        return HEARTBEAT_MSG;
      }
    }
    else {
      return NO_MSG;    
    }
  }
  
  void checkHeartBeat() {
    unsigned long timeSinceLastHB = millis() - lastReadTime;
    if(timeSinceLastHB > HB_TIMEOUT_MILLIS) {
      Serial.println("did not receive heartbeat. assuming socket is dead");
      client.stop();
    }
  }

  private:
    const int MSG_LENGTH = 8;
    EthernetClient client;
    unsigned long lastReadTime = 0;
    const unsigned long HB_TIMEOUT_MILLIS = 3000;
};

