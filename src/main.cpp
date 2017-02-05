#include "main.h"


void setup() {
//  Serial.begin(115200);

  configHTTPServer();

  wifi.begin( ssid, password )
    .onChange( true, [] ( int idx, int v, int up  ) {
      //Serial.print( "Connected to Wifi, browse to http://");
      //Serial.println( wifi.ip() );
      strip.Begin();
      strip.Show();

      port.begin(localPort);


    //  ledstrip.init(NUM_LEDS);
      server.start(); // Time to start the web server

      //startUdpForPong();
      //startUdpForVisualizer();

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
  // automaton run loop for http server
  automaton.run();

  // keep the rainbow going
  if(current_strip_state == 3){
    handleRainbow();
  } else if(current_strip_state == 4) {
    handleVisualization();
  }



}
