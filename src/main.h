#ifndef __MAIN_H
#define __MAIN_H
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <Automaton.h>
#include <Atm_esp8266.h>
#include <NeoPixelBus.h>
#include <WiFiUdp.h>

char magicPacket[] = "ESP8266 DISCOVERY";

// Set to the number of LEDs in your LED strip
#define NUM_LEDS 116
// Maximum number of packets to hold in the buffer. Don't change this.
// need atleast 4*NUM_LEDS bytes LEDs
#define BUFFER_LEN 1024

char packetBuffer[BUFFER_LEN];

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS);

// some name, can be changed latter via /settings?name=NEW_NAME
String device_name = "test_leds";

// wifi credentials
const char* ssid = "**********";
const char* password = "**********";

unsigned int localPort = 7777;

unsigned long currentMillis;
unsigned long previousMillis = 0;        // will store last time tick was updated
long interval = 30;           // interval at which to tick
uint16_t current_rainbow_state = 0; // up to 255
// udp server for visualization
WiFiUDP port;
// automaton httpd server on port 80
Atm_esp8266_httpd_simple server( 80 );

RgbColor red(255, 0, 0);
RgbColor green(0, 255, 0);
RgbColor blue(0, 0, 255);
RgbColor white(255);
RgbColor black(0);

uint8_t N = 0; // designates the pixel to alter with a visualizer packet
RgbColor prev_colour;
RgbColor colour;
RgbColor rainbow_colour;
RgbColor v_colour;
// 0 = OFF
// 1 = SOLID ON
// 3 = RAINBOW mode
// 4 = MUSIC mode
// 10 = OTA
unsigned int current_strip_state = 0; // start off
unsigned int previous_strip_state = 0; // previous must me off right?

// Sends a UDP reply packet to the sender
void sendReply(String message) {
    port.beginPacket(port.remoteIP(), port.remotePort());
    port.write(message.c_str());
    port.endPacket();

}

void setColour() {
  for(int i = 0; i < NUM_LEDS; i+=1) {
  strip.SetPixelColor(i, colour);
  }
  strip.Show();
}

void stripOff(){
  previous_strip_state = current_strip_state;
  current_strip_state = 0;
  colour = black;
  for(int i = 0; i < NUM_LEDS; i+=1) {
  strip.SetPixelColor(i, colour);
  }
  strip.Show();
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
    rainbow_colour.R = 255 - WheelPos * 3;
    rainbow_colour.G = 0;
    rainbow_colour.B = WheelPos * 3;
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    // {0, WheelPos * 3, 255 - WheelPos * 3 }
    rainbow_colour.R = 0;
    rainbow_colour.G = WheelPos * 3;
    rainbow_colour.B = 255 - WheelPos * 3;
  } else {
    WheelPos -= 170;
    // {WheelPos * 3,255 - WheelPos * 3, 0 }
    rainbow_colour.R = WheelPos * 3;
    rainbow_colour.G = 255 - WheelPos * 3;
    rainbow_colour.B = 0;
  }
}

void handleRainbow() {
  if(currentMillis - previousMillis > interval){
    for(int i=0; i<NUM_LEDS; i++) {
      Wheel((i+current_rainbow_state) & 255);
      strip.SetPixelColor(i,rainbow_colour);
    }
    current_rainbow_state ++;
    previousMillis = currentMillis;
    strip.Show();
  }
}

// returns a json string of the state of the strip
String printState(){
  StaticJsonBuffer<250> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["state"] = current_strip_state;
  root["device"] = device_name;
  JsonArray& crgb = root.createNestedArray("c_rgb");
  crgb.add(colour.R);
  crgb.add(colour.G);
  crgb.add(colour.B);
  JsonArray& prgb = root.createNestedArray("p_rgb");
  prgb.add(prev_colour.R);
  prgb.add(prev_colour.G);
  prgb.add(prev_colour.B);

  String output;
  root.printTo(output);
  return output;
}

void handleVisualization() {
  uint16_t packetSize = port.parsePacket();

   // Prevent a crash and buffer overflow
   if (packetSize > BUFFER_LEN) {
       sendReply("ERR BUFFER OVF");
       return;
   }

   if (packetSize) {
       uint16_t len = port.read(packetBuffer, BUFFER_LEN);

       // Check for a magic packet discovery broadcast
       if (!strcmp(packetBuffer, magicPacket)) {
           char reply[50];
           sprintf(reply, "ESP8266 ACK LEDS %i", NUM_LEDS);
           sendReply(reply);
           return;
       }

       // Decode byte sequence and display on LED strip
       N = 0;
       packetBuffer[len] = 0;
       for(uint16_t i = 0; i < len; i+=4) {
           N = (uint8_t)packetBuffer[i];

           if (N >= NUM_LEDS) {
               return;
           }
           v_colour.R = (uint8_t)packetBuffer[i+1];
           v_colour.G = (uint8_t)packetBuffer[i+2];
           v_colour.B = (uint8_t)packetBuffer[i+3];
           strip.SetPixelColor(N, v_colour);
       }
   }
   strip.Show();
}

void configHTTPServer(){
  server.begin()
    .onRequest( "/custom", [] ( int idx, int v, int up ) {
     prev_colour = colour;
     colour.R = server.arg( "r" ).toInt();
     colour.G = server.arg( "g" ).toInt();
     colour.B = server.arg( "b" ).toInt();
     current_strip_state = 1;
     setColour();
     server.send( printState() );
    })
    .onRequest( "/settings", [] ( int idx, int v, int up ) {
      device_name = server.arg( "name" );
      server.send( printState() );
     })
     .onRequest( "/white", [] ( int idx, int v, int up ) {
       current_strip_state = 1;
       colour = white;
       setColour();
       server.send( printState() );
     })
     .onRequest( "/blue", [] ( int idx, int v, int up ) {
       current_strip_state = 1;
       colour = blue;
       setColour();
       server.send( printState() );
     })
    .onRequest( "/red", [] ( int idx, int v, int up ) {
      current_strip_state = 1;
      colour = red;
      setColour();
      server.send( printState() );
    })
    .onRequest( "/green", [] ( int idx, int v, int up ) {
      current_strip_state = 1;
      colour = green;
      setColour();
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
        "<a href='green'>Green</a><br>"
        "<a href='blue'>Blue</a><br>"
        "<a href='rainbow'>Rainbow</a><br>"
        "<a href='music'>Music</a><br>"
        "<a href='status'>Status</a><br>"
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
