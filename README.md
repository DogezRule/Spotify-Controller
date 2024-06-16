# ESP32 Spotify Controller

## Project Overview
I created a PCB that controls Spotify playback using an ESP32 and a few Cherry MX switches.

## Demo
*Include a link to your demo video here*

## Instructions

### PCB Manufacturing
1. **Download Files**: Download the `esp32gerber.zip` file from this repository.
2. **Order PCB**: Go to [JLCPCB](https://jlcpcb.com) or any PCB manufacturer and use the following settings:
   - **Gerber file**: `Gerber_ESP32-Spotify-Controller_PCB_ESP32-Spotify-_Y1`
   - **Base Material**: FR-4
   - **Layers**: 2
   - **Dimension**: 109 mm x 73 mm
   - **PCB Qty**: 5
   - **PCB Color**: Green
   - **Silkscreen**: White
   - **Surface Finish**: HASL (with lead)

> **Note**: The minimum quantity might be 3 or 5 PCBs.

### Required Parts
1. [ESP32](https://www.amazon.com/gp/product/B08246MCL5/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
2. [PCB Standoffs](https://www.amazon.com/dp/B00VVI1L1W?psc=1&ref=ppx_yo2ov_dt_b_product_details)
3. [Cherry MX Switches](https://www.amazon.com/gp/product/B0C38JFJL2/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)
4. [Soldering Iron and Solder](https://www.amazon.com/WEP-927-IV-Soldering-High-Power-Magnifier/dp/B09TXP1KDV/ref=sr_1_12?crid=31IB3HXMDTA0U&keywords=soldering+iron&qid=1718400324&sprefix=soldering+iron%2Cindustrial%2C127&sr=1-12)
5. [Arduino IDE](https://www.arduino.cc/en/software)
6. [ESP32 Board Manager](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads)

### Setting Up the ESP32
1. **Install Arduino IDE**: Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
2. **Configure Arduino IDE**: Open the Boards Manager in Arduino IDE, search for `ESP32`, and install `esp32 by Espressif Systems`.
3. **Install Driver**: Download and install the [USB to UART Bridge VCP drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads).

### Arduino Code
1. **Open Arduino IDE**: Open the file in the repository named Code.ino

   

2. **Save the file** and we will come back to it later.

### Spotify API Setup
1. **Spotify Developer Dashboard**: Go to the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard).
2. **Login or Sign Up**: Log in or create an account.
3. **Create an App**: Create a new app and set the redirect URI to `http://localhost:8888` (we will change it later, so this doesn't matter for now).
4. **Configure the Code**: Put the client ID from the Spotify dashboard into the code near the top, named `client_id`. Do the same with the client secret for `client_secret`.

### Upload to ESP32
1. **Upload Code**: Upload the code to the ESP32 (select `ESP32 Dev Module` as the board).
2. **Connect and Test**: Connect your ESP32 and test the functionality.

### Wi-Fi Setup
1. Connect to the Wi-Fi network named `ESP32_Spotify_Controller` on a device.
2. The password is `12345678`.
3. Go to `192.168.1.2` in your browser while being connected.
4. Enter your Wi-Fi credentials and submit.
5. Switch back to your regular Wi-Fi on your device, then check the serial monitor in Arduino IDE. It should show the new IP address of the ESP32 (make sure to open the serial monitor beforehand).
6. Go to that IP address and copy it.
7. Go back to the Spotify dashboard, edit the app, and change the callback URL to `http://your_esp32_ip/callback`.
8. Return to the ESP32 webpage on the IP address and press "Log in with Spotify". After you log in, it should be set up.

### Resetting the Network
The only way to reset the network it's connected to is to reset the EEPROM. Do that at your own risk:

```cpp
#include <EEPROM.h>

#define EEPROM_SIZE 512 // Adjust this value to the size of EEPROM you are using

void clearEEPROM() {
  // Begin EEPROM with the size specified
  EEPROM.begin(EEPROM_SIZE);
  // Write 0 to each byte of the EEPROM
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  // Commit the changes to the EEPROM
  EEPROM.commit();
  // End the EEPROM to free resources
  EEPROM.end();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Clearing EEPROM...");
  clearEEPROM();
  Serial.println("EEPROM Cleared.");
}

void loop() {
  // Your main code
}
