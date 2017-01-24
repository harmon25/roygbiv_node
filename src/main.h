#ifndef __MAIN_H
#define __MAIN_H
#include <ESPAsyncUDP.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <Automaton.h>
#include <Atm_esp8266.h>
#include <ws2812_i2s.h>

// Set to the number of LEDs in your LED strip
#define NUM_LEDS 60
// Maximum number of packets to hold in the buffer. Don't change this.
// not actually ever holding more than one packet, shrinking this...
#define BUFFER_LEN 256

char packetBuffer[BUFFER_LEN];

// LED strip
static WS2812 ledstrip;
static Pixel_t pixels[NUM_LEDS];

// some name, can be changed latter via /settings?name=NEW_NAME
String device_name = "test_leds";

// wifi credentials
const char* ssid = "*******";
const char* password = "*********";

unsigned int listen_port = 2025;      // local port to listen on - See more at: http://www.esp8266.com/viewtopic.php?f=29&t=4209#sthash.h4KrQKr5.dpuf

unsigned long currentMillis;
unsigned long previousMillis = 0;        // will store last time tick was updated
long interval = 30;           // interval at which to tick
uint16_t current_rainbow_state = 0; // up to 255

// udp listen on 2025 for pings.
AsyncUDP udp;
// listen on port 7777 for visualizer packets
AsyncUDP udp_music;

// automaton httpd server on port 80
Atm_esp8266_httpd_simple server( 80 );

// colour = {r,g,b}
//the current solid led colour
uint8_t cur_colour[3] = {0,0,0};
// previous colour
uint8_t prev_colour[3] = {0,0,0};
// colour used for rainbowing
uint8_t rainbow_colour[3] = {0,0,0};

// some static colours
uint8_t red_c[3] = {255,0,0};
uint8_t white_c[3] = {255,255,255};
uint8_t blue_c[3] = {0, 0, 255};
uint8_t green_c[3] = {0, 255, 0};
uint8_t cyan_c[3] = {0, 255, 255};
uint8_t purple_c[3] = {128, 0, 128};
uint8_t off_c[3] = {0, 0, 0};

// 0 = OFF
// 1 = SOLID ON
// 3 = RAINBOW mode
// 4 = MUSIC mode
unsigned int current_strip_state = 0; // start off
unsigned int previous_strip_state = 0; // previous must me off right?

void setColour(uint8_t col[]) {
  memcpy(prev_colour,cur_colour, 3);
  for(int i = 0; i < NUM_LEDS; i+=1) {
     pixels[i].R = col[0];
     pixels[i].G = col[1];
     pixels[i].B = col[2];
  }
  ledstrip.show(pixels);
  memcpy(cur_colour, col, 3);
}

// used for rainbow effect
void setRainbow(int i){
   pixels[i].R = rainbow_colour[0];
   pixels[i].G = rainbow_colour[1];
   pixels[i].B = rainbow_colour[2];
}

void stripOff(){
  memcpy(prev_colour, cur_colour, 3);
  previous_strip_state = current_strip_state; // previous must me off right?
  current_strip_state = 0;
  setColour(off_c);
}

void stripOn(){
  // default on is white
  if(current_strip_state == 0 && previous_strip_state == 0){
    setColour(white_c);
  }

  current_strip_state = previous_strip_state; // previous must me off right?
  if(previous_strip_state != 3){
    current_strip_state = 1;
    setColour(prev_colour);
  }
}

// toggles state to rainbow
void toggleState(int s){
  previous_strip_state = current_strip_state;
  // if strip is already rainbowing, turn it off
  if(current_strip_state == s){
    current_strip_state = 0;
  // else we are on some other state, so switch to rainbowing
  } else {
    current_strip_state = s;
  }
}

// used for rainbowing, rotates around a cur_colour wheel based on 0-255 input
void Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    //{255 - WheelPos * 3, 0, WheelPos * 3 }
    rainbow_colour[0] = 255 - WheelPos * 3;
    rainbow_colour[1] = 0;
    rainbow_colour[2] = WheelPos * 3;
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    // {0, WheelPos * 3, 255 - WheelPos * 3 }
    rainbow_colour[0] = 0;
    rainbow_colour[1] = WheelPos * 3;
    rainbow_colour[2] = 255 - WheelPos * 3;
  } else {
    WheelPos -= 170;
    // {WheelPos * 3,255 - WheelPos * 3, 0 }
    rainbow_colour[0] = WheelPos * 3;
    rainbow_colour[1] = 255 - WheelPos * 3;
    rainbow_colour[2] = 0;
  }
}

void rainbow() {
  if(currentMillis - previousMillis > interval){
    for(int i=0; i<NUM_LEDS; i++) {
      Wheel((i+current_rainbow_state) & 255);
      setRainbow(i);
    }
    current_rainbow_state ++;
    previousMillis = currentMillis;
    ledstrip.show(pixels);
  }
}

// returns a json string of the state of the strip
String printState(){
  StaticJsonBuffer<250> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["state"] = current_strip_state;
  root["device"] = device_name;
  JsonArray& crgb = root.createNestedArray("c_rgb");
  crgb.add(cur_colour[0]);
  crgb.add(cur_colour[1]);
  crgb.add(cur_colour[2]);
  JsonArray& prgb = root.createNestedArray("p_rgb");
  prgb.add(prev_colour[0]);
  prgb.add(prev_colour[1]);
  prgb.add(prev_colour[2]);

  String output;
  root.printTo(output);
  return output;
}

// state asyncudp server listening for multicast discovery packetS
// sends back the state
void startUdpForPong(){
  if(udp.listen(listen_port)){
     udp.onPacket([](AsyncUDPPacket packet) {
       Serial.println("GOT UDP PACKET -> RESPONDING!");

       packet.printf("%s", printState().c_str());
     });
  }
}

uint8_t N = 0; // designates the pixel to alter with a visualizer packet

void startUdpForVisualizer(){
  if(udp_music.listen(7777)){
     udp_music.onPacket([](AsyncUDPPacket packet) {
       if(current_strip_state == 4){
          memcpy(packetBuffer,packet.data(), packet.length());
          for(int i = 0; i < packet.length(); i+=4) {
            packetBuffer[packet.length()] = 0;
            N = packetBuffer[i];
            pixels[N].R = (uint8_t)packetBuffer[i+1];
            pixels[N].G = (uint8_t)packetBuffer[i+2];
            pixels[N].B = (uint8_t)packetBuffer[i+3];
         }
          ledstrip.show(pixels);
      }
     });
  }
}

void configHTTPServer(){
  server.begin()
    .onRequest( "/custom", [] ( int idx, int v, int up ) {
     cur_colour[0] = server.arg( "r" ).toInt();
     cur_colour[1] = server.arg( "g" ).toInt();
     cur_colour[2] = server.arg( "b" ).toInt();
     // this mutates r, g, b
     //String msg = String("Set cur_colour: R="+String(r) + " G=" + String(g) + " B=" + String(b));
     current_strip_state = 1;
     setColour(cur_colour);
     server.send( printState() );
    })
    .onRequest( "/settings", [] ( int idx, int v, int up ) {
      device_name = server.arg( "name" );
      server.send( printState() );
     })
     .onRequest( "/blue", [] ( int idx, int v, int up ) {
       current_strip_state = 1;
       setColour(blue_c);
       server.send( printState() );
     })
    .onRequest( "/red", [] ( int idx, int v, int up ) {
      current_strip_state = 1;
      setColour(red_c);
      server.send( printState() );
    })
    .onRequest( "/white", [] ( int idx, int v, int up ) {
       current_strip_state = 1;
       setColour(white_c);
       server.send( printState() );
    })
    .onRequest( "/cyan", [] ( int idx, int v, int up ) {
       current_strip_state = 1;
       setColour(cyan_c);
       server.send( printState() );
    })
    .onRequest( "/purple", [] ( int idx, int v, int up ) {
       current_strip_state = 1;
       setColour(purple_c);
       server.send( printState() );
    })
    .onRequest( "/green", [] ( int idx, int v, int up ) {
       current_strip_state = 1;
       setColour(green_c);
       server.send( printState() );
    })
    .onRequest( "/rainbow", [] ( int idx, int v, int up ) {
       toggleState(3);
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
    .onRequest( "/music", [] ( int idx, int v, int up ) {
      toggleState(4);
      server.send( printState() );
    })
    .onRequest( "/ota", [] ( int idx, int v, int up ) {
      server.send( "Entering OTA Mode" );
      ArduinoOTA.handle();
      delay(5000);
    })
    .onRequest( "/status", [] ( int idx, int v, int up ) {
      server.send( printState() );
    })
    .onRequest( "/reset_device", [] ( int idx, int v, int up ) {
      server.send( "!!!! Restarting Device !!!!" );
      ESP.restart();
    })
    .onRequest( "/", [] ( int idx, int v, int up ) {
      server.send(
        "<!DOCTYPE html><html><body>"
        "<a href='white'>White</a><br>"
        "<a href='red'>Red</a><br>"
        "<a href='blue'>Blue</a><br>"
        "<a href='green'>Green</a><br>"
        "<a href='cyan'>Cyan</a><br>"
        "<a href='purple'>Purple</a><br>"
        "<a href='rainbow'>Rainbow</a><br>"
        "<a href='music'>Music</a><br>"
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
  ArduinoOTA.setHostname(device_name.c_str());

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
