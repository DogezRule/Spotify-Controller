#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <WiFiUdp.h>

// Spotify API credentials
const char* client_id = "client_id";
const char* client_secret = "client_secret";
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
HTTPClient httpClient;

bool isPlaying = false;
unsigned long lastPlaybackStateFetch = 0;
const unsigned long playbackStateFetchInterval = 5000; // 5 seconds
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 1000; // 1 second

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
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
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
  prefetchAccessToken();
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  unsigned long currentTime = millis();
  if (currentTime - lastPlaybackStateFetch > playbackStateFetchInterval) {
    isPlaying = fetchPlaybackState();
    lastPlaybackStateFetch = currentTime;
  }

  if (currentTime - lastWiFiCheck > wifiCheckInterval) {
    checkWiFiConnection();
    lastWiFiCheck = currentTime;
  }

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
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected. Reconnecting...");
    WiFi.reconnect();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Reconnected to Wi-Fi");
  }
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
    httpClient.begin(token_url);
    httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "grant_type=authorization_code&code=" 
                      + code 
                      + "&redirect_uri=http://" + WiFi.localIP().toString() + "/callback"
                      + "&client_id=" + String(client_id) 
                      + "&client_secret=" + String(client_secret);

    int httpResponseCode = httpClient.POST(postData);

    if (httpResponseCode == 200) {
      String response = httpClient.getString();
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
      String errorResponse = httpClient.getString();
      Serial.println("Error on token exchange: " + String(httpResponseCode) + " - " + errorResponse);
      server.send(500, "text/plain", "Failed to log in.");
    }

    httpClient.end();
  } else {
    server.send(400, "text/plain", "Authorization code not found.");
  }
}

void prefetchAccessToken() {
  if (refresh_token == "") {
    Serial.println("Refresh token not available. Please log in first.");
    return;
  }

  Serial.println("Prefetching access token...");
  getAccessToken();
}

void getAccessToken() {
  if (refresh_token == "") {
    Serial.println("Refresh token not available. Please log in first.");
    return;
  }

  Serial.println("Getting access token...");
  httpClient.begin(token_url);
  httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postData = "grant_type=refresh_token&refresh_token=" 
                    + refresh_token 
                    + "&client_id=" + String(client_id) 
                    + "&client_secret=" + String(client_secret);

  unsigned long startTime = millis();  // Start timing
  int httpResponseCode = httpClient.POST(postData);
  unsigned long endTime = millis();    // End timing

  Serial.print("HTTP POST took: ");
  Serial.print(endTime - startTime);
  Serial.println("ms");

  if (httpResponseCode == 200) {
    String response = httpClient.getString();
    Serial.println("Access token response: " + response);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);

    access_token = doc["access_token"].as<String>();
    Serial.println("Access Token: " + access_token);
  } else {
    String errorResponse = httpClient.getString();
    Serial.println("Error on getting access token: " + String(httpResponseCode) + " - " + errorResponse);
  }

  httpClient.end();
}

void controlSpotify(String action) {
  Serial.println("Controlling Spotify: " + action);
  String url;
  String method;

  if (action == "next") {
    url = String(player_url) + "/next";
    method = "POST";
  } else if (action == "previous") {
    url = String(player_url) + "/previous";
    method = "POST";
  } else if (action == "playpause") {
    // Use the pre-fetched playback state
    String playPauseAction = isPlaying ? "pause" : "play";
    isPlaying = !isPlaying;  // Toggle the state
    url = String(player_url) + "/" + playPauseAction;
    method = "PUT";
  }

  httpClient.begin(url);
  httpClient.addHeader("Authorization", "Bearer " + access_token);
  httpClient.addHeader("Content-Length", "0");  // Ensuring the Content-Length header is present

  unsigned long startTime = millis();  // Start timing
  int httpResponseCode = -1;
  if (method == "POST") {
    httpResponseCode = httpClient.POST("");
  } else if (method == "PUT") {
    httpResponseCode = httpClient.PUT("");
  }
  unsigned long endTime = millis();    // End timing

  Serial.print("HTTP ");
  Serial.print(method);
  Serial.print(" took: ");
  Serial.print(endTime - startTime);
  Serial.println("ms");

  if (httpResponseCode == 204) {
    Serial.println("Spotify control successful: " + action);
  } else {
    String errorResponse = httpClient.getString();
    Serial.println("Error on Spotify control: " + String(httpResponseCode) + " - " + errorResponse);
    // If the error is due to invalid token, refresh the token and retry
    if (httpResponseCode == 401) {
      getAccessToken();
      controlSpotify(action);  // Retry the action
    }
  }

  httpClient.end();
}

bool fetchPlaybackState() {
  httpClient.begin(String(player_url) + "/currently-playing");
  httpClient.addHeader("Authorization", "Bearer " + access_token);

  unsigned long startTime = millis();  // Start timing
  int httpResponseCode = httpClient.GET();
  unsigned long endTime = millis();    // End timing



  if (httpResponseCode == 200) {
    String response = httpClient.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, response);

    bool isPlaying = doc["is_playing"].as<bool>();
    httpClient.end();
    return isPlaying;
  } else {
    httpClient.end();
    return false;
  }
}
