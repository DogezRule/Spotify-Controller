# Please do not do this currently, I have realized that the refresh token authentication does not work and it too much work

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
2. **Configure Arduino IDE**: Open the Boards Manager in Arduino IDE, search for `ESP32`, and install the `esp32 by Espressif Systems`.
3. **Install Driver**: Download and install the [USB to UART Bridge VCP drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads).

### Arduino Code
1. **Open Arduino IDE**: Create a new file and paste the following code:

    ```cpp
    #include <WiFi.h>
    #include <HTTPClient.h>
    #include <ArduinoJson.h>

    // Wi-Fi credentials
    const char* ssid = "YourSSID";
    const char* password = "YourPassword";

    // Spotify API credentials
    const char* client_id = "YourClientID";
    const char* client_secret = "YourClientSecret";
    const char* refresh_token = "YourRefreshToken";

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

    void IRAM_ATTR nextSongISR() {
      nextSongFlag = true;
    }

    void IRAM_ATTR prevSongISR() {
      prevSongFlag = true;
    }

    void IRAM_ATTR playPauseISR() {
      playPauseFlag = true;
    }

    void setup() {
      Serial.begin(115200);
      Serial.println("Starting setup...");

      // Initialize Wi-Fi
      Serial.print("Connecting to WiFi: ");
      Serial.println(ssid);
      WiFi.begin(ssid, password);
      
      int retries = 0;
      while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi");
      } else {
        Serial.println("\nFailed to connect to WiFi");
        return; // Halt further execution if Wi-Fi connection fails
      }

      // Set up GPIO pins
      pinMode(nextSongPin, INPUT_PULLUP);
      pinMode(prevSongPin, INPUT_PULLUP);
      pinMode(playPausePin, INPUT_PULLUP);
      attachInterrupt(nextSongPin, nextSongISR, FALLING);
      attachInterrupt(prevSongPin, prevSongISR, FALLING);
      attachInterrupt(playPausePin, playPauseISR, FALLING);
      Serial.println("GPIO pins configured");

      // Get initial access token
      getAccessToken();
    }

    void loop() {
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

    void getAccessToken() {
      Serial.println("Getting access token...");
      HTTPClient http;
      http.begin(token_url);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      String postData = "grant_type=refresh_token&refresh_token=" 
                        + String(refresh_token) 
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
        Serial.println("Error on getting access token: " 
                       + String(httpResponseCode) + " - " + errorResponse);
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
        method = playPauseAction == "pause" ? "PUT" : "PUT";
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
        Serial.println("Error on Spotify control: " 
                       + String(httpResponseCode) + " - " + errorResponse);
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

      http.end();
    }
    ```

2. **Save the file** and we will come back to it later.

### Spotify API Setup
1. **Spotify Developer Dashboard**: Go to [Spotify Developer Dashboard](https://developer.spotify.com/dashboard).
2. **Login or Sign Up**: Log in or create an account.
3. **Create an App**: Create a new app and set the redirect URI to `http://localhost:8888`.
4. **Authorize Application**: 
   - Go to the following URL: `https://accounts.spotify.com/authorize?client_id=YOUR_CLIENT_ID&response_type=code&redirect_uri=YOUR_REDIRECT_URI&scope=user-read-playback-state%20user-modify-playback-state`
   - Replace `YOUR_CLIENT_ID` and `YOUR_REDIRECT_URI` with your actual client ID and redirect URI from the Spotify dashboard.
   - Authorize the application and copy the code from the URL after `callback?code=`.

### Python Script for Tokens
1. **Download VSCode**: Download and set up [VSCode](https://code.visualstudio.com/).
2. **Create Python Script**: Create a new Python file and paste the following code:

    ```python
    import requests
    import base64

    # Spotify API credentials
    client_id = "client_id"
    client_secret = "client_secret"
    redirect_uri = "http://localhost:8888/callback"
    authorization_code = "auth_code"  # Replace with the actual authorization code

    # Encode client credentials
    client_creds = f"{client_id}:{client_secret}"
    client_creds_b64 = base64.b64encode(client_creds.encode())

    # Token request headers
    token_url = "https://accounts.spotify.com/api/token"
    headers = {
        "Authorization": f"Basic {client_creds_b64.decode()}",
        "Content-Type": "application/x-www-form-urlencoded"
    }

    # Token request payload
    data = {
        "grant_type": "authorization_code",
        "code": authorization_code,
        "redirect_uri": redirect_uri
    }

    # Make the request to get tokens
    response = requests.post(token_url, headers=headers, data=data)

    # Print the response (access token and refresh token)
    print(response.json())
    ```

3. **Install Requests Library**: Open the terminal in VSCode and run `pip install requests`.
4. **Run the Script**: Replace `client_id`, `client_secret`, and `auth_code` with your actual credentials and run the script to get the refresh token.
5. **Update Arduino Code**: Copy the refresh token and paste it into the Arduino code's `refresh_token` variable.

### Upload to ESP32
1. **Upload Code**: Upload the code to the ESP32 (select `ESP32 Dev Module` as the board).
2. **Connect and Test**: Connect your ESP32 and test the functionality.

## Conclusion
Your ESP32 Spotify Controller should now be set up and working. Enjoy controlling your music with the press of a button!
