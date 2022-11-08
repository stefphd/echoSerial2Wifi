
//include
#include <ESP8266WiFi.h>

//user config
#define MODE_AP // PC connects directly to ESP
//#define MODE_STA // ESP and PC connect to a common WiFi router
#define MODE_TCP //TCP/IP mode
//#define MODE_UDP //UDP mode
//#define BIDIRECTIONAL //enable bidirectional transfer. If not defined the link is ESP->PC, otherwise ESP<->PC.
//#define DEBUG

//advanced config
#define UART_BAUD 115200 // UART baudrate. Must be consisted with that used in the Teensy
#define IP 192, 168, 0, 1 //IP address of ESP. ONLY WITH MODE_AP. With MODE_STA the router assigns the IP
#define MASK 255, 255, 255, 0 //Netmask of ESP. ONLY WITH MODE_AP. Not used with MODE_STA.
#define PORT 9876 //Port of the service. Must be consistent with that used in the PC
#define CHANNEL 2 //Channel of ESP. ONLY WITH MODE_AP. Not used with MODE_STA.
#define MAX_CONN 1 //Max number of simulatous connection. ONLY WITH MODE_AP. Not used with MODE_STA.
#define PWR_LEVEL 20.5 // Wifi power. Max is 20.5. ONLY WITH MODE_AP. Not used with MODE_STA.
#define BUF_SIZE  256 // Buffer size (maximum bytes read at a time).
#define TIMEOUT 1000 // Timeout after which the buffer read is sent (us).

//include
#ifdef MODE_TCP
#include <WiFiClient.h>
#else
#include <WiFiUdp.h>
#endif

//led on and off defines
#define LED_ON LOW //LED on state (inverse logic).
#define LED_OFF HIGH //LED off state (inverse logic).

//wifi
#ifdef MODE_TCP
WiFiClient client;
WiFiServer server(PORT); //service at this port
#else
WiFiUDP udp;
IPAddress broadcast;
#endif
#ifdef MODE_AP
IPAddress ip(IP); // connect to this IP if MODE_AP
IPAddress netmask(MASK); // with this netmash if MODE_AP
#endif

//buffer
byte buf[BUF_SIZE]; //buffer of size BUF_SIZE
#ifdef BIDIRECTIONAL
uint32_t timer; //timer for Client->Serial timeout
#endif
size_t len; //number of read bytes


void setup() {
  //set builtin led pin
  //pinMode(LED_BUILTIN, OUTPUT);
  //digitalWrite(LED_BUILTIN, LED_ON); //BUILT IN LED is ON
  
  //start serial
  Serial.begin(UART_BAUD);
  #ifdef DEBUG
  Serial.println();
  listNetworks();
  #endif
  while (!Serial) {} //wait for serial start
  while (!Serial.available()) {} //wait for wifi name and password
  String name_wifi = Serial.readStringUntil(':');
  String pwd_wifi = Serial.readStringUntil('\n');
  #ifdef DEBUG
  Serial.println();
  Serial.print("name: --");
  Serial.print(name_wifi);
  Serial.print("--\npwd:  --");
  Serial.print(pwd_wifi);
  Serial.println("--");
  #endif

  #ifdef MODE_AP //AP mode (PC connects directly to ESP) (no router)
  WiFi.mode(WIFI_AP); //set AP mode
  if (!WiFi.softAPConfig(ip, ip, netmask)) {
    #ifdef DEBUG
    Serial.println("Unable to configure IP address for AP mode");
    #endif
    while(1); // configure ip address for softAP 
  }
  if (!WiFi.softAP(name_wifi.c_str(), pwd_wifi.c_str(), CHANNEL, false, MAX_CONN)) {
    #ifdef DEBUG
    Serial.println("Unable to configure name and password for AP mode");
    #endif
    while(1); // configure ssid and password for softAP
  }
  WiFi.setOutputPower(PWR_LEVEL); //set power
  #endif

  #ifdef MODE_STA //STA mode (PC and ESP connected to same Wifi) (using router)
  WiFi.mode(WIFI_STA);
  WiFi.begin(name_wifi.c_str(), pwd_wifi.c_str());
  unsigned int c = 0;
  while ((WiFi.status() != WL_CONNECTED)) //check connection every 100ms
    delay(100);
  #ifdef DEBUG
  if (WiFi.status() == WL_CONNECTED) Serial.println("Connected to wifi");
  #endif
  #endif

  #ifdef MODE_TCP
  server.begin(); // start TCP server 
  IPAddress localIP = WiFi.localIP();
  #ifdef DEBUG
  #ifdef MODE_AP
  Serial.println(ip);
  #else
  Serial.println(localIP);
  #endif
  #endif
  #else
  #ifdef DEBUG
  if (udp.begin(PORT)) //start UDP server 
    Serial.println("UDP started");
  else 
    Serial.println("UDP not started");
  #endif
  uint32_t local_ip = WiFi.localIP();
  uint32_t netmask = WiFi.subnetMask();
  uint32_t broadcast_ip = local_ip | (~netmask);
  broadcast = IPAddress(broadcast_ip);
  #ifdef DEBUG
  Serial.println(broadcast);
  #endif
  #endif
  
  Serial.setTimeout(TIMEOUT/1e3); //set timeout for Serial->client
  Serial.flush(); //clear serial
  
  //digitalWrite(LED_BUILTIN, LED_OFF); //BUILT IN LED is OFF
}

void loop() {
#ifdef MODE_TCP
  #ifdef MODE_AP //with MODE_AP check the number of connected stations
  if (WiFi.softAPgetStationNum()>0) {
  #else //with MODE_STA check the connection status
  if ((WiFi.status() == WL_CONNECTED)) {
  #endif

    //this is run both in AP and STA mode
    if(!client.connected()) { // if client not connected check for new client
      //digitalWrite(LED_BUILTIN, LED_OFF); //BUILT IN LED is OFF
      client = server.available(); // wait for it to connect
      Serial.flush(); //clean serial
      return; //restart the cycle for a check for new client
    }
    //digitalWrite(LED_BUILTIN, LED_ON); //BUILT IN LED is ON
    
    if (Serial.available()) { //echo to client a maximum of BUF_SIZE bytes at a time, with a read timeout TIMEOUT
      len = Serial.readBytes(buf, BUF_SIZE);
      if (len) client.write(buf, len); //send len bytes of buffers (i.e. only read bytes)
      len = 0; //just to be sure
    }
    
    #ifdef BIDIRECTIONAL //if bidirectional echo to serial
    if (client.available()) { //echo to Serial a maximum of BUF_SIZE bytes
      timer = millis();
      len = client.available();
      while ((len < BUF_SIZE) && (micros()-timer) <= TIMEOUT) 
        len = client.available(); //wait BUF_SIZE bytes or timeout
      len = client.read(buf, len); //read len bytes (possibly equal to BUF_SIZE)
      if (len) Serial.write(buf, len); //send to serial
      len = 0; //just to be sure
    }
    #endif
    //end of AP and STA common part
    
  #ifdef MODE_AP //else for 'if (WiFi.softAPgetStationNum()>0)'
  } else { //nothing connected to the AP
    //digitalWrite(LED_BUILTIN, LOW); 
    client.stop();
    Serial.flush();
  }
  #else
  } else { //wifi not connected
    //digitalWrite(LED_BUILTIN, LOW); 
    Serial.flush();
  }
  #endif
#else
  #ifdef MODE_STA //with MODE_STA check the connection status
  if ((WiFi.status() == WL_CONNECTED)) {
  #endif
  if (Serial.available()) { //echo to client a maximum of BUF_SIZE bytes at a time, with a read timeout TIMEOUT
    len = Serial.readBytes(buf, BUF_SIZE);
    if (len) {
      udp.beginPacket(broadcast, PORT);
      udp.write(buf, len); //send len bytes of buffers (i.e. only read bytes)
      udp.endPacket();  
    }
    len = 0; //just to be sure
  }
  #ifdef MODE_STA
  } else { //wifi not connected
    //digitalWrite(LED_BUILTIN, LOW); 
    Serial.flush();
  }
  #endif

#endif  
}

void listNetworks() {
  int numssid = WiFi.scanNetworks();
  if (numssid == -1) {
    Serial.println("No network found");
    return;
  }
  Serial.print("Found "), Serial.print(numssid), Serial.println(" network(s)");
  Serial.println("No\tName\tSignal\tEncryption type");
  for (int i = 0; i < numssid; ++i) {
    Serial.print("("), Serial.print(i+1), Serial.print(")\t");
    Serial.print(WiFi.SSID(i)), Serial.print('\t');
    Serial.print(WiFi.RSSI(i)), Serial.print(" dBm\t");
    printEncryptionType(WiFi.encryptionType(i)), Serial.print('\n');
  }
}

void printEncryptionType(int i) {
  switch (i) {
    case ENC_TYPE_WEP:
      Serial.print("WEP");
      break;
    case ENC_TYPE_TKIP:
      Serial.print("WPA");
      break;
    case ENC_TYPE_CCMP:
      Serial.print("WPA2");
      break;
    case ENC_TYPE_NONE:
      Serial.print("None");
      break;
    case ENC_TYPE_AUTO:
      Serial.print("Auto");
      break;
    default:
      Serial.print("Unknown");
      break;
  }
}
