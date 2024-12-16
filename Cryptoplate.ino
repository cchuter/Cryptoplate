#ifndef ARDUINO_INKPLATE2
#error "Wrong board selection for this example, please select Soldered Inkplate2 in the boards menu."
#endif

#include "Inkplate.h" // Include Inkplate library to the sketch
#include "Network.h" // Our networking functions, declared in Network.cpp

#include "EEPROM.h"   // Include ESP32 EEPROM library
#include <WebServer.h>  //Include ESP32 library for Web server
#include <WiFi.h>       //Include ESP32 WiFi library
#include <WiFiClient.h> //Include ESP32 WiFi library for AP
#include <uri/UriBraces.h>
#include "htmlsetup.h"
#include "driver/rtc_io.h" //ESP32 library used for deep sleep and RTC wake up pins
#include <rom/rtc.h>       // Include ESP32 library for RTC (needed for rtc_get_reset_reason() function)

#define ssid "OSMIXMAS2024"
#define pass "osmirules"
#define uS_TO_S_FACTOR 1000000 // Conversion factor for micro seconds to seconds
#define EEPROM_START_ADDR 0  // Start EEPROM address for user data.
#define EEPROM_SIZE       128

#define SSID_SIZE 32
#define SSIDPASS_SIZE 16
#define REFRESH_SIZE 3
#define TIMEZONE_SIZE 3
#define URL_SIZE 77

#define eepromSSID EEPROM.readString(EEPROM_START_ADDR)
#define eepromPass EEPROM.readString(EEPROM_START_ADDR+SSID_SIZE+1)
#define eepromRefresh EEPROM.readString(EEPROM_START_ADDR+SSID_SIZE+SSIDPASS_SIZE+2)
#define eepromTimezone EEPROM.readString(EEPROM_START_ADDR+SSID_SIZE+SSIDPASS_SIZE+REFRESH_SIZE+3)
#define eepromURL EEPROM.readString(EEPROM_START_ADDR+SSID_SIZE+SSIDPASS_SIZE+REFRESH_SIZE+TIMEZONE_SIZE+4)

// Include fonts used
#include "Fonts/Inter16pt7b.h"
#include "Fonts/Inter8pt7b.h"
#include "Fonts/Inter12pt7b.h"

// bitmaps made by https://javl.github.io/image2cpp/
#include "osmi.h"
#include "gala.h"

// Delay between API calls in miliseconds
//#define DELAY_MS 5 * 60 * 1000 // Every 1 minute, minute has 60 seconds and second has 1000 miliseconds

Network network; // Create object with all networking functions

Inkplate display; // Create object for Inkplate library

// Adjust your time zone, 2 means UTC+2
//int timeZone = -6;

// Used for storing raw price values
double prices[2];
char currentosmi[16];
char currentgala[16];

// Used to simplify UI design
struct textElement
{
  int x;
  int y;
  const GFXfont *font;
  char *text;
  char align;
  uint8_t text_color;
};

// Variables for storing all displayed data as char arrays
char date[64];

textElement elements[] = {
  {110, 18, &Inter8pt7b, date, 0, INKPLATE2_BLACK},
  {32, 50, &Inter12pt7b, "OSMI", 0, INKPLATE2_BLACK},
  {110, 50, &Inter12pt7b, currentosmi, 0, INKPLATE2_BLACK}, 
  {32, 85, &Inter12pt7b, "GALA", 0, INKPLATE2_BLACK}, 
  {110, 85, &Inter12pt7b, currentgala, 0, INKPLATE2_BLACK}};


// Our functions declared below setup and loop
void drawAll();
void getCurrencyData();

WebServer server(80);            // Create Web server on port 80 (HTTP port number)
IPAddress serverIP;

// Delay between API calls in miliseconds
#define DELAY_MS 3 * 60 * 1000 // Every 3 minutes, minute has 60 seconds and second has 1000 miliseconds
#define DELAY_WIFI_RETRY_SECONDS 10

void setup()
{
  // Begin serial communitcation, sed for debugging
  Serial.begin(115200);

  // Initial display settings
  display.begin();
  display.setTextWrap(false); // Disable text wrapping
  display.setTextColor(INKPLATE2_BLACK, INKPLATE2_WHITE);

  display.setCursor(0, 0); // Set cursor, custom font uses different method for setting cursor
  // You can find more about that here https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts
  display.setTextSize(1); // Set size of font in comparison to original 5x7 font

  if (rtc_get_reset_reason(0) == DEEPSLEEP_RESET) // Check if ESP32 is reseted by deep sleep or power up / user manual
                                                  // reset (or some other reason)
  {  
    EEPROM.begin(EEPROM_SIZE);
    delay(1000);

    renderCryptoPrices();

    Serial.println("Going to sleep");
    delay(1000);

    esp_sleep_enable_timer_wakeup(uS_TO_S_FACTOR * 60 * eepromRefresh.toInt());
    esp_deep_sleep_start();      
  }
  else
  {
    display.setCursor(0, 0);
    display.clearDisplay();             // Clear frame buffer of display
    display.display();                  // Put clear image on display

    display.setTextWrap(false); // Disable text wrapping
    display.setTextColor(INKPLATE2_BLACK, INKPLATE2_WHITE);

    // Start WiFi
    WiFi.begin();            // Init. WiFi library
    WiFi.mode(WIFI_AP);      // Set WiFi to Access point mode
    WiFi.softAP(ssid, pass); // Set SSID (WiFi name) and password for Access point

    serverIP = WiFi.softAPIP(); // Get the server IP address

    server.on("/", handleRoot); // If you open homepage, go to handle root function
    server.on(UriBraces("/string/{}"),
              handleString); // If you send some text to Inkplate, go to handleString function. Note that {} brackets at
                            // the end of address. That means that web address has some arguments (our text!).
    server.begin();          // Start the web server

    clearEEPROM();

    // Print Instructions    
    showInstructions();
  }
}

void renderCryptoPrices() 
{
  display.setCursor(0,0);
  display.clearDisplay(); 

  delay(1000);

  // Join wifi
  int tries = 0;
  while (!display.connectWiFi(eepromSSID.c_str(), eepromPass.c_str(), WIFI_TIMEOUT, true))
  {
    // Try several times to connect
    tries++;
    delay(500);

    if (tries == 10)
    {
      // Can't connect to netowrk
      // Clear display for the error message
      display.clearDisplay();
      // Set the font size;
      display.setTextSize(1);
      // Set the cursor positions and print the text.
      display.setCursor(0, 0);
      display.println("Unable to connect to: ");
      display.println(eepromSSID);
      display.println("Please check ssid and pass!");
      // Display the error message on the Inkplate and go to deep sleep
      display.display();
      
      esp_sleep_enable_timer_wakeup(1000L * DELAY_WIFI_RETRY_SECONDS);
      (void)esp_deep_sleep_start();
    }
  }
  
  //After connecting to WiFi we need to get internet time from NTP server
  time_t nowSec;
  struct tm timeInfo;
  // Fetch current time in epoch format and store it
  display.getNTPEpoch(&nowSec);
  gmtime_r(&nowSec, &timeInfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeInfo));
  
  while (!network.getData(prices)) // Get data and check if data is successfully fetched
  {
      Serial.println("Retrying retreiving data!");
      delay(1000);
  }

  // Our main drawing function
  drawAll();
  
  // Refresh
  display.display();

  // Go to sleep before checking again
  // rtc_gpio_isolate(GPIO_NUM_12);   // Isolate/disable GPIO12 on ESP32 (only to reduce power consumption in sleep)
  esp_sleep_enable_timer_wakeup(uS_TO_S_FACTOR * 60 * eepromRefresh.toInt()); // Activate wake-up timer
    
  esp_deep_sleep_start();       // Put ESP32 into deep sleep. Program stops here
}

void loop() {
  if (rtc_get_reset_reason(0) != DEEPSLEEP_RESET) // Still waiting on settings
  {
    server.handleClient(); // You have to constantly read if there is any new client connected to web server
  }
  delay(100);
}

void updateHTML()
{ 
  // This function will send response to client and send HTML code of our web page
  server.send(200, "text/html", s);
}

void handleRoot()
{ 
  // This function will send response to client if client open a root (homepage) of our web page
  updateHTML();
}

void handleString()
{ 
  // This function will send response to client, send HTML code of web page, get the text from argument sent in web page
  // address and refresh screen with new text

  String homeSSID, homePass, apiURL, refresh, timezone;
  homeSSID = server.arg(0);
  homePass = server.arg(1);
  refresh = server.arg(2);
  timezone = server.arg(3);
  //TODO: allow dynamic updates to API endpoint - too long right now
  //apiURL = "https://api.coingecko.com/api/v3/simple/price?ids=osmi,gala&vs_currencies=usd";
  apiURL = server.arg(4);
  updateHTML();

  if (refresh.toInt() <= 0)
  {
    refresh = "1";
  }
  if (timezone.toInt() > 24 || timezone.toInt() < -24)
  {
    timezone = "0";
  }

  if (homeSSID != "" && homePass != "" && refresh != "" && timezone != "") 
  {
    WiFi.softAPdisconnect();
    display.clearDisplay();    // Clear everything from epaper frame buffer
    display.setCursor(0, 10);
    display.setTextColor(INKPLATE2_RED);
    display.println("Writing to EEPROM...");
    display.println(homeSSID);
    display.println(homePass);
    display.println(refresh);
    display.println(timezone);

    // Write to EEPROM one variable after another with linefeed - never go below the start address
    EEPROM.begin(EEPROM_SIZE);
    int address = EEPROM_START_ADDR;
    EEPROM.writeString(address,homeSSID);
    address += SSID_SIZE+1;
    EEPROM.writeString(address,homePass);
    address += SSIDPASS_SIZE+1;
    EEPROM.writeString(address,refresh);
    address += REFRESH_SIZE+1;
    EEPROM.writeString(address,timezone);
    address += TIMEZONE_SIZE+1;
    EEPROM.writeString(address,apiURL);
    EEPROM.commit();
    delay(500);

    display.setTextColor(INKPLATE2_BLACK);
    display.println("Wrote EEPROM.");
    display.println(eepromSSID);
    display.println(eepromPass);
    display.println(eepromRefresh);
    display.println(eepromTimezone);
    display.display();
    delay(500);
    renderCryptoPrices();
  }
  else
  {
    display.setTextColor(INKPLATE2_RED);
    display.println("Error reading values submitted: ");
    display.setTextColor(INKPLATE2_BLACK);
    display.print(homeSSID);
    display.print(homePass);
    display.print(refresh);
    display.print(timezone);
    display.display();
  }
}

void getCurrencyData()
{
  strcat(currentosmi, "$");
  dtostrf(prices[0], 6, 4, currentosmi+1);
  Serial.println(prices[0]);
  strcat(currentgala, "$");
  dtostrf(prices[1], 6, 4, currentgala+1);
  Serial.println(prices[1]);
}

// Our main drawing function
void drawAll()
{
  // Save current date string, more about it in Network.cpp
  network.getTime(date, eepromTimezone.toInt());

  getCurrencyData();

  // Draw our UI elements
  for (int i = 0; i < sizeof(elements) / sizeof(elements[0]); ++i)
  {
      // Text settings
      display.setTextColor(elements[i].text_color, INKPLATE2_WHITE);
      display.setFont(elements[i].font);
      display.setTextSize(1);

      // 0 is aligned by left bottom corner, 1 by right
      if (elements[i].align == 0)
          display.setCursor((int)(elements[i].x * 0.96), (int)(elements[i].y));
      else if (elements[i].align == 1)
      {
          int16_t x, y;
          uint16_t w, h;

          // Get how much the textx offsets pointer and draw it that much more left
          display.getTextBounds(elements[i].text, 0, 0, &x, &y, &w, &h);

          display.setCursor((int)(elements[i].x * 0.96) - w, (int)(elements[i].y));
      }

      // Print out text to above set cursor location
      display.print(elements[i].text);
  }

  display.drawBitmap(5,32,osmi_bits,osmi_width,osmi_height,INKPLATE2_RED);
  display.drawBitmap(5,68,gala_bits,gala_width,gala_height,INKPLATE2_RED);
}

// Function for clearing EEPROM data (it will NOT clear waveform data)
void clearEEPROM()
{
  for (int i = 0; i < EEPROM_SIZE; i++)
  {
      EEPROM.write(i + EEPROM_START_ADDR,
                    0); // Start writing from address 76 (anything below that address number is waveform data!)
  }
  EEPROM.commit();
}

void showInstructions()
{
  display.clearDisplay();    // Clear everything from epaper frame buffer

  display.setCursor(0, 5); // Print out instruction on how to connect to Inkplate WiFi and how to open a web page
  display.setTextColor(INKPLATE2_RED);
  display.print("Connect to SSID: ");
  display.setTextColor(INKPLATE2_BLACK); 
  display.println(ssid);
  display.println();

  display.setTextColor(INKPLATE2_RED);
  display.print("With password: ");
  display.setTextColor(INKPLATE2_BLACK);
  display.println(pass);
  display.println();

  display.setTextColor(INKPLATE2_RED);
  display.print("Open your web browser to: ");
  display.println();
  display.println();
  display.setTextColor(INKPLATE2_BLACK);
  display.print("http://");
  display.print(serverIP);
  display.println('/');
  display.println();

  display.display(); // Send everything to screen (refresh the screen)
}