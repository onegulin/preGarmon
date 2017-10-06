# preGarmon

Hardware
  WeMos D1 mini
  Aurel.RTX-MID-3V RF 433 receiver-transmitter
  WeMos RFLink shield (https://github.com/onegulin/WeMos-RFLink)

Libraries
  Manchester
     upstream https://github.com/mchr3k/arduino-libs-manchester (have added ESP8266 support)
     origin   https://github.com/onegulin/arduino-libs-manchester (use branch ESP8266 with adjusted counters)
  PubSubClient
     Origin   https://github.com/onegulin/pubsubclient
     forked from https://github.com/Imroy/pubsubclient

Algorithms
   fletcher16 (https://en.wikipedia.org/wiki/Fletcher's_checksum)
   Manchester coding (http://www.atmel.com/images/atmel-9164-manchester-coding-basics_application-note.pdf)

Passwords
   WiFi       ssid/password
   thingspeak server/post.apikey
   MQTT       server/port/user/password

MQTT
   subsribe   /GarageDoor/GetStatus
   publish    /GarageDoor/Test

   { "id":"<hex>", "loctime":"<hex>", "door":<int>", "vcc":"<int>", "t":"<int>", "event":"<int>", "cnt":"<str> cnt/drop/err" }
   
