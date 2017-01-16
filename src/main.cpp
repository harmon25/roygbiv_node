#include "main.h"


void setup() {
  //Serial.begin(115200);
  //Serial.println("Booting");

  configHTTPServer();


  wifi.begin( ssid, password )
    .onChange( true, [] ( int idx, int v, int up  ) {
      //Serial.print( "Connected to Wifi, browse to http://");
      //Serial.println( wifi.ip() );
      server.start(); // Time to start the web server
      Udp.begin(listen_port);
    })
    .onChange( false, [] ( int idx, int v, int up  ) {
      //Serial.println( "Lost Wifi connection");
      ESP.restart();
    })
    .led( LED_BUILTIN, false ) // Esp8266 built in led shows wifi status
    .start();

  // do the OTA stuff so we are ready when ota mode initalized via http
  configOTA();
  //Serial.println("Ready");
  //Serial.print("IP address: ");
  //Serial.println(wifi.ip());
}

void loop() {
  // set global current millis referenced in some functions
  currentMillis = millis();

  // keep the rainbow going
  if(current_strip_state == 3){
    rainbow();
  }

  // automaton run loop for http server
  automaton.run();

  // handle udp discovery packets
  handleDiscovery();

}
