#ifndef __MAIN_H
#define __MAIN_H
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Automaton.h>
#include <Atm_esp8266.h>
#include <Adafruit_NeoPixel.h>

const char* device_name = "test_leds";


const char* ssid = "TPAP";
const char* password = "zaC2JSLnnxi7";

unsigned int listen_port = 2025;      // local port to listen on - See more at: http://www.esp8266.com/viewtopic.php?f=29&t=4209#sthash.h4KrQKr5.dpuf
char packetBuffer[255]; //buffer to hold incoming packet


// udp listen on 2025 for indicate alive.
WiFiUDP Udp;

#define PIN D4

Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);
Atm_esp8266_httpd_simple server( 80 );

unsigned long currentMillis;
unsigned long previousMillis = 0;        // will store last time tick was updated
long interval = 30;           // interval at which to tick

uint16_t current_rainbow_state = 0;

uint32_t colour;

uint32_t white = strip.Color(127, 127, 127);
uint32_t red = strip.Color(255, 0, 0);
uint32_t green = strip.Color(0, 255, 0);
uint32_t blue = strip.Color(0, 0, 255);
uint32_t cyan = strip.Color(0, 255, 255);
uint32_t purple = strip.Color(128, 0, 128);
uint32_t yellow = strip.Color(255, 255, 0);
uint32_t off_led = strip.Color(0, 0, 0);

// 0 = OFF
// 1 = SOLID ON
// 3 = RAINBOW
unsigned int current_strip_state = 0;
unsigned int previous_strip_state = 0;




void setColour(uint32_t c) {
  previous_strip_state = current_strip_state;
  current_strip_state = 1;
  colour = c;
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
}

void stripOff() {
  previous_strip_state = current_strip_state;
  current_strip_state = 0;
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, off_led);
  }
  strip.show();
}

void stripOn() {
  // if we were rainbowing, go back to it
  if(current_strip_state == 0 && previous_strip_state == 3){
      current_strip_state = 3;
  } else {
      // we were just off, turn back on to last color.
      current_strip_state = 1;
      for(uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, colour);
      }
      strip.show();
  }
}

void toggleRainbow(){
  previous_strip_state = current_strip_state;
  // if strip is already rainbowing, turn it off
  if(current_strip_state == 3){
    current_strip_state = 0;
  // else we are on some other state, so switch to rainbowing
  } else {
    current_strip_state = 3;
  }
}

// used for rainbowing
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbow() {
  uint16_t i;
  // restart rainbow
  if(current_rainbow_state > 255){
    current_rainbow_state = 0;
  }

  if(currentMillis - previousMillis > interval){
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+current_rainbow_state) & 255));
    }
    current_rainbow_state ++;
    previousMillis = currentMillis;
    strip.show();
  }
}


// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

String printState(){
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  String c = String(colour, HEX);
  c.toUpperCase();

  root["hex"] = c;
  root["int"] = colour;
  root["state"] = current_strip_state;
  root["device"] = device_name;
  String output;
  root.printTo(output);
  return output;
}

void handleDiscovery(){
  int packetSize = Udp.parsePacket();
  if(packetSize){
    IPAddress remoteIp = Udp.remoteIP();
    int len = Udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(printState().c_str());
    Udp.endPacket();
  }
}


void configHTTPServer(){
  server.begin()
    .onRequest( "/custom", [] ( int idx, int v, int up ) {
     int r = server.arg( "r" ).toInt();
     int g = server.arg( "g" ).toInt();
     int b = server.arg( "b" ).toInt();
     String msg = String("Set colour: R="+String(r) + " G=" + String(g) + " B=" + String(b));

     colour = strip.Color(r, g, b);
     setColour(colour);
     server.send( printState() );
    })
    .onRequest( "/blue", [] ( int idx, int v, int up ) {
      setColour(blue);
      server.send( printState() );
     })
    .onRequest( "/green", [] ( int idx, int v, int up ) {
      setColour(green);
      server.send( printState() );
    })
    .onRequest( "/red", [] ( int idx, int v, int up ) {
      setColour(red);
      server.send( printState() );
    })
    .onRequest( "/cyan", [] ( int idx, int v, int up ) {
      setColour(cyan);
      server.send( printState() );
    })
    .onRequest( "/purple", [] ( int idx, int v, int up ) {
      setColour(purple);
      server.send( printState() );
    })
    .onRequest( "/yellow", [] ( int idx, int v, int up ) {
      setColour(yellow);
      server.send( printState() );
    })
    .onRequest( "/white", [] ( int idx, int v, int up ) {
       setColour(white);
       server.send( printState() );
    })
    .onRequest( "/rainbow", [] ( int idx, int v, int up ) {
       toggleRainbow();
       server.send( printState() );
    })
    .onRequest( "/off", [] ( int idx, int v, int up ) {
      stripOff();
      server.send( printState() );
    })
    .onRequest( "/on", [] ( int idx, int v, int up ) {
      stripOn();
      server.send( printState() );
    })
    .onRequest( "/ota", [] ( int idx, int v, int up ) {
      server.send( "Entering OTA Mode" );
      ArduinoOTA.handle();
      delay(10000);
    })
    .onRequest( "/status", [] ( int idx, int v, int up ) {
      server.send( printState() );
    })
    .onRequest( "/", [] ( int idx, int v, int up ) {
      server.send(
        "<!DOCTYPE html><html><body>"
        "<a href='white'>White</a><br>"
        "<a href='red'>Red</a><br>"
        "<a href='blue'>Blue</a><br>"
        "<a href='green'>Green</a><br>"
        "<a href='yellow'>Yellow</a><br>"
        "<a href='cyan'>Cyan</a><br>"
        "<a href='purple'>Purple</a><br>"
        "<a href='rainbow'>Rainbow</a><br>"
        "<a href='status'>Status</a><br>"
        "<a href='on'>On</a><br>"
        "<a href='off'>Off</a><br>"
        "<a href='ota'>OTA</a><br>"
        "</body></html>"
      );
    });

}


void configOTA(){
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(device_name);

  // No authentication by default
  ArduinoOTA.setPassword("led_strip");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
  //  Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

#endif /* __MAIN_H */
