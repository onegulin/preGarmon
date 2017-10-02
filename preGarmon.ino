#include <ESP8266WiFi.h>
#include <Manchester.h>
#include <PubSubClient.h>

#define DO_WIFI         1
#define POST_THINGSPEAK 1
#define POST_MQTT       1
#define DEBUG           0

#define AUREL  1 

#ifdef ARDUINO_ESP8266_NODEMCU
  #define RX_PIN     4   // D2
  #define LED_PIN    16  // buildin
  #define DEBUG_PIN  5   // D1
#endif

#ifdef ARDUINO_AVR_UNO
  #define RX_PIN     3
  #define LED_PIN    2
  #define DEBUG_PIN  4
#endif

#ifdef ARDUINO_ESP8266_WEMOS_D1MINI
  #define LED_PIN    2   // buildin
  #define DEBUG_PIN  4   // D2
  
#if AUREL
  #define RX_PIN     12  // D6
  #define RX_VCC_PIN 15  // D8
#else
  #define RX_PIN     5   // D1
#endif  
  
#endif

/* ------------------------------------------------------ */
const char* ssid = "XXX";
const char* password = "XXX";

//const char* server = "api.thingspeak.com";
const char* server = "184.106.153.149";
const String apiKey = "XXX";
 
const char *mqtt_server = "XXX.com";
const int  mqtt_port = 123;
const char *mqtt_user = "XXX";
const char *mqtt_pass = "XXX";

/* ------------------------------------------------------ */
#if DO_WIFI

bool WiFiconnected = false;

WiFiClient client;
PubSubClient mqttclient(client, mqtt_server, mqtt_port);

bool connectWiFi(WiFiClient& client, uint16_t timeout)
{
  bool rc = false;
  int n = 20;

  if (timeout)
    n = timeout * 2;

  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.print(ssid);
  for (int i = 0; i < n; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.print("IP address: ");
      Serial.print(WiFi.localIP());
      rc = true;
      break;
    }
    else {
      delay(500);
    }
  }
  Serial.println();
  return rc;
}
#endif 

#if POST_THINGSPEAK
void postThingSpeak(WiFiClient& client, String apiKie, String f1, String f2, String f3, String f4, String f5)
{
  String postStr = apiKey;
  postStr += "&field1="; postStr += f1;
  postStr += "&field2="; postStr += f2;
  postStr += "&field3="; postStr += f3;
  postStr += "&field4="; postStr += f4;
  postStr += "&field5="; postStr += f5;
  postStr += "\r\n\r\n";

  client.print("POST /update HTTP/1.1\n");
  client.print("Host: api.thingspeak.com\n");
  client.print("Connection: close\n");
  client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
  client.print("Content-Type: application/x-www-form-urlencoded\n");
  client.print("Content-Length: ");
  client.print(postStr.length());
  client.print("\n\n");
  client.print(postStr);
}

#endif

/* ----------------------------------------------------------------------------- */
typedef struct {
  uint8_t len;
  uint8_t id;
  uint8_t door_status;
  uint8_t event;
  uint16_t local_time;
  uint16_t vcc;
  uint16_t temperature;
  uint16_t checksum;

} Payload;

Payload rxdata;

uint16_t lastloctime [10] = {};
uint16_t rec_pack_cnt[10] = {};
uint16_t drp_pack_cnt[10] = {};
uint16_t cs_err_cnt[10]   = {};

uint8_t moo = 1;

char laststr [128];

/* ----------------------------------------------------------------------------- */
uint16_t fletcher16( uint8_t const *data, size_t bytes )
{
  uint16_t sum1 = 0xff, sum2 = 0xff;
  size_t tlen;

  while (bytes) {
    tlen = bytes >= 20 ? 20 : bytes;
    bytes -= tlen;
    do {
      sum2 += sum1 += *data++;
    } while (--tlen);
    sum1 = (sum1 & 0xff) + (sum1 >> 8);
    sum2 = (sum2 & 0xff) + (sum2 >> 8);
  }
  /* Second reduction step to reduce sums to 8 bits */
  sum1 = (sum1 & 0xff) + (sum1 >> 8);
  sum2 = (sum2 & 0xff) + (sum2 >> 8);
  return sum2 << 8 | sum1;
}

/* ----------------------------------------------------------------------------- */
void setup()
{
  Serial.begin(9600);
  Serial.println();
 
  pinMode(RX_PIN, INPUT);
  
#if AUREL
  pinMode(RX_VCC_PIN, OUTPUT);
  digitalWrite(RX_VCC_PIN, HIGH);
#endif

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, moo);

#if DEBUG
  // for use in Manchester code
  pinMode(DEBUG_PIN, OUTPUT);
  digitalWrite(DEBUG_PIN, 1);

  pinMode(0, OUTPUT);
  digitalWrite(0, 0);
  pinMode(12, OUTPUT);
  digitalWrite(12, 0);
  pinMode(14, OUTPUT);
  digitalWrite(14, 0);
#endif

#if DO_WIFI
  WiFiconnected = connectWiFi(client, 0);
#endif

  man.setupReceive(RX_PIN, MAN_1200);
  man.beginReceiveArray(sizeof(rxdata), (uint8_t*)&rxdata);
}

/* ----------------------------------------------------------------------------- */
void loop() {
  int16_t  t = 0;
  uint16_t v = 0;
  uint16_t e = 0;
  uint16_t d = 0;
  uint16_t lt    = 0;
  uint16_t cnt   = 0;
  uint16_t drop  = 0;
  uint16_t cserr = 0;
  char str [128];


  if (man.receiveComplete()) { //received something
    int      receivedSize = rxdata.len;
    uint16_t checksum1 = rxdata.checksum;
    uint16_t checksum2 = fletcher16((uint8_t *)&rxdata, sizeof(rxdata) - 2);
    int n;

    t = rxdata.temperature;
    v = rxdata.vcc;
    d = rxdata.door_status;
    e = rxdata.event;
    n = rxdata.id;
    lt = rxdata.local_time;

    man.beginReceiveArray(sizeof(rxdata), (uint8_t*)&rxdata);
    moo++;
    moo = moo % 2;
    digitalWrite(LED_PIN, moo);

    if (receivedSize > sizeof(rxdata)) {
      Serial.println("!!!!!!!!!!!! Corruption!!!!");
      goto SkipIt;
    }

    if (checksum1 != checksum2) {
      Serial.print(" cs1:"); Serial.print(checksum1);
      Serial.print(" cs2:"); Serial.print(checksum2);
      Serial.println();
      cs_err_cnt[n]++; 
      goto SkipIt;
    }

    //if (v > 3500 || t > 350 || d > 1 || e > 3) {
    if (v > 3500 ||  d > 1 || e > 3) {
      Serial.println("Looks like a garbage. Skipping...");
      Serial.print(" vcc:"); Serial.print(v);
      Serial.print(" t:");   Serial.print(t);
      Serial.print(" d:");   Serial.print(d);
      Serial.print(" e:");   Serial.print(e);
      Serial.println();
      goto SkipIt;
    }

    rec_pack_cnt[n]++;
    if (lastloctime[n] && lt - 1 > lastloctime[n]) {
      + lt - lastloctime[n] - 1;
    }
    lastloctime[n] = lt;
    
    cnt = rec_pack_cnt[n];
    drop = drp_pack_cnt[n];
    cserr= cs_err_cnt[n];
    /*
    Serial.print("id:");
    Serial.print(n, HEX);
    Serial.print(" loctime:"); Serial.print(lt, HEX);
    Serial.print(" door:");  Serial.print(d, HEX);
    Serial.print(" vcc:");   Serial.print(v);
    Serial.print(" t:");     Serial.print(t);
    Serial.print(" event:"); Serial.print(e);
    Serial.print(" cnt:");
    Serial.print(cnt);
    Serial.print("/");
    Serial.print(drop);
    Serial.print("/");
    Serial.println(cserr);
    */
    
    //sprintf(str, "id:%x loctime:%X door:%x vcc:%d t:%d event:%d cnt:%d/%d/%d",
    //              n, lt, d, v, t, e, cnt, drop, cserr);
    sprintf(str, "{\"id\":\"%x\",\"loctime\":\"%X\",\"door\":\"%x\",\"vcc\":\"%d\",\"t\":\"%d\",\"event\":\"%d\",\"cnt\":\"%d/%d/%d\" }",
                  n, lt, d, v, t, e, cnt, drop, cserr);
    Serial.println(str);

#if DO_WIFI
    if (!WiFiconnected) {
       Serial.print("... no WiFi ...");
       WiFiconnected = connectWiFi(client, 0);
    }
#endif
                    
#if POST_THINGSPEAK
    if (WiFiconnected) {
      Serial.println("connecting to Thingspeak");
      if (client.connect(server, 80)) { //   "184.106.153.149" or api.thingspeak.com
        postThingSpeak(client, apiKey, String(t), String(v), String(d), String(cnt), String(drop));
        //Serial.println("posted to Thingspeak");
        client.stop();
      }
      else {
        Serial.print("Can't connect to ");
        Serial.println(server);
      }
    }
#endif

#if POST_MQTT
    if (WiFiconnected) { 
       if (mqttclient.connected()) {
          mqttclient.publish("/GarageDoor/Test", str);                            // Change it !!!!
          strcpy(laststr, str);
       } else {   
          Serial.println("Connecting to MQTT server");
          if (mqttclient.connect(MQTT::Connect("ESPPclient")
                      .set_auth("vlbl", "vlblvlbl"))) {
              Serial.println("Connected to MQTT server");
              mqttclient.set_callback(callback);
              //mqttclient.subscribe("/GarageDoor/Button");
              mqttclient.subscribe("/GarageDoor/GetStatus");
              strcpy(laststr, str);
              mqttclient.publish("/GarageDoor/Test", str);                        // Change it !!!!
              strcpy(laststr, str);
          } else {
             Serial.println("Could not connect to MQTT server");   
          }
       }       
    }
#endif
    
  }
SkipIt:    ;

#if POST_MQTT
   if (WiFi.status() == WL_CONNECTED) {
     if (!mqttclient.connected()) {
        Serial.println("Connecting to MQTT server - callback");
        if (mqttclient.connect(MQTT::Connect("ESPPclient")
                       .set_auth("vlbl", "vlblvlbl"))) {
            Serial.println("Connected to MQTT server - callback");
            mqttclient.set_callback(callback);
            //mqttclient.subscribe("/GarageDoor/Button");
            mqttclient.subscribe("/GarageDoor/GetStatus");
         } else {
            Serial.println("Could not connect to MQTT server - callback");   
         }
     }
   }
      
   if (mqttclient.connected())
      mqttclient.loop();
#endif

}

void callback(const MQTT::Publish& pub) {
  
  Serial.print("callback......");  
  Serial.print(pub.topic());
  Serial.print("=>");
  Serial.println(pub.payload_string());
  
  if (strcmp(pub.topic().c_str(),"/GarageDoor/GetStatus")==0) {
      mqttclient.publish("/GarageDoor/Test", laststr);  
  }
  
}
