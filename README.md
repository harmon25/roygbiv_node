# roygbiv_node
platformio project for WeMos d1_mini or other esp8266 based MCUs to controll an RGB LED strip over an HTTP API
roygbiv_controller is an Elixr project used to control the state of multiple roygbiv_nodes

# OTA UPDATES!
1. Configure a custom password used when uploading OTA in `platformio.ini`
 - `upload_flags = --auth led_strip`
3. Place node in in OTA mode by making an HTTP GET to `http://IP_ADDRESS/ota`
2. Run the following command replacing `IP_ADDRESS` with the IP of your node to be updated
 - `pio run -e d1_mini -t upload --upload-port IP_ADDRESS`
