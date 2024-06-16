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
1. **Open Arduino IDE**: Create a new file and paste the following code:

    ```cpp
    #include <WiFi.h>
    #include <HTTPClient.h>
    #include <WebServer.h>
    #include <ArduinoJson.h>
    #include <WiFiManager.h>
    #include <ArduinoOTA.h>
    #include <DNSServer.h>
    #include <WiFiUdp.h>

    // Spotify API credentials
    const char* client_id = "6bc180be928646868d6ba8fa858c0d2f";
    const char* client_secret = "50eeb8eb996c42b0bbcaba6e23a2caf6";
    String refresh_token;

    // GPIO pins for switches
    const int nextSongPin = 18;
    const int prevSongPin = 13;
    const int playPausePin = 12;

    // Spotify API endpoints
    const char* token_url = "https://accounts.spotify.com/api/token";
    const char* player_url = "https://api.spotify.com/v1/me/player";

    // Access token
    String access_token;

    // Interrupt flags
    volatile bool nextSongFlag = false;
    volatile bool prevSongFlag = false;
    volatile bool playPauseFlag = false;

    // Debounce variables
    unsigned long lastDebounceTimeNext = 0;
    unsigned long lastDebounceTimePrev = 0;
    unsigned long lastDebounceTimePlayPause = 0;
    const unsigned long debounceDelay = 500; // milliseconds

    WebServer server(80);

    void IRAM_ATTR nextSongISR() {
      unsigned long currentTime = millis();
      if (currentTime - lastDebounceTimeNext > debounceDelay) {
        nextSongFlag = true;
        lastDebounceTimeNext = currentTime;
      }
    }

    void IRAM_ATTR prevSongISR() {
      unsigned long currentTime = millis();
      if (currentTime - lastDebounceTimePrev > debounceDelay) {
        prevSongFlag = true;
        lastDebounceTimePrev = currentTime;
      }
    }

    void IRAM_ATTR playPauseISR() {
      unsigned long currentTime = millis();
      if (currentTime - lastDebounceTimePlayPause > debounceDelay) {
        playPauseFlag = true;
        lastDebounceTimePlayPause = currentTime;
      }
    }

    void setup() {
      Serial.begin(115200);
      Serial.println("Starting setup...");

      // Wi-Fi Manager for configuration
      WiFiManager wifiManager;
      wifiManager.autoConnect("SpotifyControllerAP");

      // OTA setup
      ArduinoOTA.setHostname("spotify-controller");
      ArduinoOTA.onStart([]() { Serial.println("OTA Start"); });
      ArduinoOTA.onEnd([]() { Serial.println("\nOTA End"); });
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
      });
      ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        if (error == OTA_END_ERROR) Serial.println("End Failed");
      });
      ArduinoOTA.begin();

      // Set up GPIO pins
      pinMode(nextSongPin, INPUT_PULLUP);
      pinMode(prevSongPin, INPUT_PULLUP);
      pinMode(playPausePin, INPUT_PULLUP);
      attachInterrupt(nextSongPin, nextSongISR, FALLING);
      attachInterrupt(prevSongPin, prevSongISR, FALLING);
      attachInterrupt(playPausePin, playPauseISR, FALLING);
      Serial.println("GPIO pins configured");

      // Set up web server routes
      server.on("/", handleRoot);
      server.on("/callback", handleCallback);
      server.begin();
      Serial.println("HTTP server started");

      // Placeholder for access token
      getAccessToken();
    }

    void loop() {
      server.handleClient();
      ArduinoOTA.handle();

      if (nextSongFlag) {
        nextSongFlag = false;
        Serial.println("Next song button pressed");
        controlSpotify("next");
      }
      if (prevSongFlag) {
        prevSongFlag = false;
        Serial.println("Previous song button pressed");
        controlSpotify("previous");
      }
      if (playPauseFlag) {
        playPauseFlag = false;
        Serial.println("Play/Pause button pressed");
        controlSpotify("playpause");
      }

      delay(500);
    }

    void handleRoot() {
      IPAddress ip = WiFi.localIP();
      String html = "<html><body>";
      html += "<h1>Spotify Controller</h1>";
      html += "<p>ESP32 is connected to Wi-Fi. Click the button below to log in with Spotify:</p>";
      html += "<a href='https://accounts.spotify.com/authorize?client_id=" + String(client_id) + "&response_type=code&redirect_uri=http://" + ip.toString() + "/callback&scope=user-modify-playback-state user-read-playback-state'>Log in with Spotify</a>";
      html += "</body></html>";
      server.send(200, "text/html", html);
    }

    void handleCallback() {
      if (server.hasArg("code")) {
        String code = server.arg("code");
        Serial.println("Authorization code: " + code);

        // Exchange authorization code for refresh token
        HTTPClient http;
        http.begin(token_url);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        String postData = "grant_type=authorization_code&code=" 
                          + code 
                          + "&redirect_uri=http://" + WiFi.localIP().toString() + "/callback"
                          + "&client_id=" + String(client_id) 
                          + "&client_secret=" + String(client_secret);

        int httpResponseCode = http.POST(postData);

        if (httpResponseCode == 200) {
          String response = http.getString();
          Serial.println("Token response: " + response);

          DynamicJsonDocument doc(1024);
          deserializeJson(doc, response);

          refresh_token = doc["refresh_token"].as<String>();
          access_token = doc["access_token"].as<String>();
          Serial.println("Refresh Token: " + refresh_token);
          Serial.println("Access Token: " + access_token);

          // Save the refresh token to persistent storage (SPIFFS, EEPROM, etc.)
          // For simplicity, we'll just keep it in memory in this example

          server.send(200, "text/plain", "Logged in successfully! You can close this window.");
        } else {
          String errorResponse = http.getString();
          Serial.println("Error on token exchange: " + String(httpResponseCode) + " - " + errorResponse);
          server.send(500, "text/plain", "Failed to log in.");
        }

        http.end();
      } else {
        server.send(400, "text/plain", "Authorization code not found.");
      }
    }

    void getAccessToken() {
      if (refresh_token == "") {
        Serial.println("Refresh token not available. Please log in first.");
        return;
      }

      Serial.println("Getting access token...");
      HTTPClient http;
      http.begin(token_url);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      String postData = "grant_type=refresh_token&refresh_token=" 
                        + refresh_token 
                        + "&client_id=" + String(client_id) 
                        + "&client_secret=" + String(client_secret);

      int httpResponseCode = http.POST(postData);

      if (httpResponseCode == 200) {
        String response = http.getString();
        Serial.println("Access token response: " + response);

        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);

        access_token = doc["access_token"].as<String>();
        Serial.println("Access Token: " + access_token);
      } else {
        String errorResponse = http.getString();
        Serial.println("Error on getting access token: " + String(httpResponseCode) + " - " + errorResponse);
      }

      http.end();
    }

    void controlSpotify(String action) {
      Serial.println("Controlling Spotify: " + action);
      HTTPClient http;
      String url;
      String method;

      if (action == "next") {
        url = String(player_url) + "/next";
        method = "POST";
      } else if (action == "previous") {
        url = String(player_url) + "/previous";
        method = "POST";
      } else if (action == "playpause") {
        // Get the current playback state
        String playPauseAction = getPlaybackState() ? "pause" : "play";
        url = String(player_url) + "/" + playPauseAction;
        method = "PUT";
      }

      http.begin(url);
      http.addHeader("Authorization", "Bearer " + access_token);
      http.addHeader("Content-Length", "0");  // Ensuring the Content-Length header is present

      int httpResponseCode = -1;
      if (method == "POST") {
        httpResponseCode = http.POST("");
      } else if (method == "PUT") {
        httpResponseCode = http.PUT("");
      }

      if (httpResponseCode == 204) {
        Serial.println("Spotify control successful: " + action);
      } else {
        String errorResponse = http.getString();
        Serial.println("Error on Spotify control: " + String(httpResponseCode) + " - " + errorResponse);
        // If the error is due to invalid token, refresh the token and retry
        if (httpResponseCode == 401) {
          getAccessToken();
          controlSpotify(action);  // Retry the action
        }
      }

      http.end();
    }

    bool getPlaybackState() {
      HTTPClient http;
      http.begin(player_url + String("/currently-playing"));
      http.addHeader("Authorization", "Bearer " + access_token);

      int httpResponseCode = http.GET();

      if (httpResponseCode == 200) {
        String response = http.getString();
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, response);

        bool isPlaying = doc["is_playing"].as<bool>();
        http.end();
        return isPlaying;
      } else {
        http.end();
        return false;
      }
    }
    ```

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

## Conclusion
Your ESP32 Spotify Controller should now be set up and working. Enjoy controlling your music with the press of a button!
