/*
   Must allocate more memory for the ulp in
   Arduino15\packages\esp32\hardware\esp32\1.0.0\tools\sdk\sdkconfig.h
   -> #define CONFIG_ULP_COPROC_RESERVE_MEM
   for this sketch to compile. 2048b seems
   good.
*/

/*
   DESCRIPTION OF THE PAGE:
   OBJECTIVES OF THE MODULE:

   THIS IS A PROJECT THAT IS AIMED AT CARRYING OUT THE FOLLOWING REQUIREMENTS:
   1. FOR THE FIRST TIME, THE USER WILL BE REQUIRED TO LOGIN TO THE LOCAL CONFIGURATION WEBSERVER WHERE ALL THE AVAILABLE WIFI
      WILL BE DISPLAYED.
   2. THE SYSTEM GETS THE CREDENTIAL OF THE USER VIA THE CREATED LOCAL WEBSERVER AND SAVES IT IN THE EEPROM TO BE USED SUBSQUENTLY.
   3. THE SYSTEM THEN CHECKS THE LIQUID LEVEL OF THE DISPENSER
      IF THE DISPENSER IS FULL, THE SYSTEM GOES TO SLEEP FOR 3 HOURS
      ELSE IF THE DISPENSER IS EMPTY, THE SYSTEM SENDS A NOTIFICATION VIA EMAIL SHOWING WHICH DISPENSER IS EMPTY BY THE NUMBER AND THE FLOOR IT IS LOCATED.
   4. AFTER SENDING THE EMAIL, THE DISPENSER GOES TO SLEEP AGAIN. THIS ROUTINE CHECK WILL BE IN A LOOP.

   5. BELOW IS THE GENERAL REVIEW OF THE ENTIRE CODE GIVING THEIR FUNCTIONALITY.

*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////// CODE REVIEW FOR THE PROJECT ////////////////////////////////////////////////////////////////////////
#include <esp32fota.h>
#include <HTTPClient.h> // THIS IS THE LIBRARY USED FOR THE FONT AND FUCTIONALITY OF THE WEBSERVER. 
#include <WebServer.h>  // THIS LIBRARY CREATES THE LOCAL WEBSERVER 
#include <EEPROM.h>     // THIS LIBRARY IS USED TO STORE THE CREDENTIALS ON THE MEMORY ON THE MICROCONTROLLER.
#include "ArduinoJson.h"  //THIS IS A LIBRARY USED IN FORMATTING THE DATA GOTTEN FROM THE WEBSERVER SO AS TO BE USED BY THE MICROCONTROLLER CODE

#include <Wire.h>  // THIS LIBRARY IS USED  BY I2C DEVICES.
#include <Arduino.h>
#include <WiFiClient.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
// THE IF STATEMENT BELOW IS USED TO DEFINE THE MICROCONTROLLER BOARD BEING USED FOR THE PROJECT
#ifdef ESP8266
//#include <ESP8266WiFiSERVER.h>
#elif defined(ESP32)
#include <WiFi.h>
#else
#error "Board not found"
#endif

//#include "soc/rtc_cntl_reg.h"
//#include "soc/rtc_io_reg.h"
//#include "soc/soc_ulp.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "esp32/ulp.h"
#include "ulp_main.h"
#define CONFIG_ULP_COPROC_RESERVE_MEM
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10
extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

gpio_num_t ulp_27 = GPIO_NUM_27;
gpio_num_t ulp_13 = GPIO_NUM_13;
RTC_DATA_ATTR int bootCount = 0;

/////////////////////////////////////////
///////drawn from main_firmware//////////

//Establishing Local server at port 80
WebServer servercred(80);                            /// THIS DECLARES THE PORT BEING USED BY THE WEBSERVER
///
//WebServer serverstat(80); //Server for station mode on port 80

// esp32fota esp32fota("<Type of Firme for this device>", <this version>);
esp32FOTA esp32FOTA("esp32-fota-http", 14);


//Variables

int i = 0;                //// THIS VARIABLE IS USED MAJORLY IN LOOP CONTROL FUNCTION LIKE WHILE LOOP, FOR LOOP , ETC.
int statusCode;
///  THIS VARIABLE IS USED WHEN CREATING THE WEBSERVER.
//// TWO VALUES WERE ASSIGNED TO THIS VARIABLE IN THE FUNCTION THAT IS USED TO CREATE THE WEBSERVER.

const char* ssid = "Default SSID";
const char* passphrase = "Default passord";

String st;                 /// THIS VARIABLE IS USED TO STORE THE WIFI NAME AND WIFI PASSWORD.
String content;            /// THIS IS A STRING VARIABLE THAT CONTAINS THE INFORMATION THAT WILL BE PUBLISHED ON THE WEBSERVER
String esid;               /// DEFAULT WIFI NAME WHICH WILL BE CHANGED WHEN USER LOGIN TO THE WEBSERVER
String epass = "";         /// DEFAULT WIFI PASSWORD WHICH WILL BE CHANGED WHEN USER LOGIN TO THE WEBSERVER

String Email = "";
String macAdd = "";
String serverName = "http://unitecprotection.com/test/api.php?update=1&id=10&qty=0";
boolean Empty = false;
bool compare = false;
HTTPClient http;
//int final_eeprom_data = 0;
//int rangeval = 0;
//////////////////////////////////Function Decalration////////////////////////////////////////////////////
//------ Fuctions used for WiFi credentials saving and connecting to it which you do not need to change

/*
   THE "TESTWIFI" FUNCTION IS CALLED TO CHECK THE WIFI STATUS CONNECTION OF THE DISPENSER
   THE FUNCTION CHECKS AND RETRIEVE THE CREDENTIALS STORED IN THE MEMORY OF THE MICROCONTROLLER

*/
bool testWifi(void);
bool testWifi(void)
{
  int c = 0;   //THIS VARIABLE WAS USED IN CONTROLLING THE LOOP FUNCTIONS USED IN THIS TESTWIFI FUNCTION
  int checking = 0;  //THIS VARIABLE WAS USED IN CONTROLLING THE LOOP FUNCTIONS USED IN THIS TESTWIFI FUNCTION



  /*
     THIS FOR LOOP RETRIEVES THE WIFI NAME STORED IN THE MEMORY
  */
  for (int i = 0; i < 20; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");
  for (int i = 20; i < 40; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);


  //THIS WHILE LOOP RUNS 10 TIMES CONSTANTLY INITIALIZING THE WIFI CONNECTION
  //while (checking < 10) {
  //    checking = checking + 1;

  WiFi.begin(esid.c_str(), epass.c_str());
  // }
  // WiFi.begin(esid.c_str(), epass.c_str());

  /*
       THIS WHILE LOOP CHECKS IF THERE IS CONNECTION
  */
  while ( c < 50 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}

/*
   THIS FUNCTION WAS USED TO LAUNCH THE WEBSERVER WHICH IS USED TO COLLECT THE CREDENTIALS OF THE USER
*/
void launchWeb(void);
void launchWeb()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());


  createWebServer();  /// THIS IS THE FUNCTION THAT INITIALIZES THE CREATION OF THE WEBSERVER



  servercred.begin();
  Serial.println("Server started");
}


/*
   THIS FUNCTION WAS USED TO LAUNCH THE WEBSERVER WHICH IS USED TO COLLECT THE CREDENTIALS OF THE USER
*/
void launchWebemail(void);
void launchWebemail()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());


  createservermail();  /// THIS IS THE FUNCTION THAT INITIALIZES THE CREATION OF THE WEBSERVER



  servercred.begin();
  Serial.println("Server started webemail");
}


/*
   THIS "CREATEWEBSERVER" FUNCTION WAS CALLED IN THE ABOVE FUNCTION. ONCE THE "CREATEWEBSERVER" WAS CALLED, THE FUNCTION WAS IMMEDIATELY EXECUTED
   THIS FUNCTION IS WHERE THE MAIN WEBSERVER WAS DESIGNED.
*/

void createWebServer()
{
  /*
     THE "SERVERCRED" FUNCTION IS USED FOR DISPLAYING AND OBTAINING THE USER CREDENTIALS
  */


  servercred.on("/", []() {
    IPAddress ip = WiFi.softAPIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

    content += "<p>";
    content = "<!DOCTYPE HTML>\r\t\n<html><div style='text-align:center;font-weight:bolder;font-size:20px !important;margin-bottom:20px;'>Welcome to Wifi Credentials Update page ,Please fill every requirement. NB: restart the device if the required Wifi is not visbile below</div>";
    content += "<center><p>";
    content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"Available WiFi\"></form>";
    content += ipStr;
    content += "<p>";
    content += st;
    content += "</p><form method='get' action='setting'><label>SSID:</label><input name='ssid' id='ssid' length=20><label>PASSWORD:</label><input name='pass' length=20><label>EMAIL:</label><input name='email' length=100><input type='submit'></form>";
    content += "<script> function ssidlize(e){document.getElementById('ssid').value = e}</script></html>";
    servercred.send(200, "text/html", content);
  });
  servercred.on("/scan", []() {
    //setupAP();
    IPAddress ip = WiFi.softAPIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    content = "<!DOCTYPE HTML>\r\n<html>go back";
    servercred.send(200, "text/html", content);
  });

  /*
       THIS SESSION IS RESPONSIBLE FOR ASSIGNING THE USER CREDENNTIALS TO THREE VARIABLE SO WE CAN SAVE THEM IN THE MEMORY
  */
  servercred.on("/setting", []() {

    /*
       SAVING THE CREDENTIALS
    */
    String qsid = servercred.arg("ssid");
    String qpass = servercred.arg("pass");
    String mail = servercred.arg("email");

    Serial.print("SSID: ");
    Serial.println(qsid);
    Serial.print("PASSWORD: ");
    Serial.println(qpass);
    Serial.print("EMAIL: ");
    Serial.println(mail);

    /*
       CLEARING UP THE MEMORY
    */

    if (qsid.length() > 0 && qpass.length() > 0) {
      Serial.println("clearing eeprom");
      for (int i = 0; i < 1000; ++i) {
        EEPROM.write(i, 0);
      }
      Serial.println(qsid);
      Serial.println("");
      Serial.println(qpass);
      Serial.println("");

      /*
         THIS FOR LOOP IS RESPONSIBLE FOR STORING THE CREDENTIALS GOTTEN FROM THE USER
      */
      Serial.println("writing eeprom ssid:");
      for (int i = 0; i < qsid.length(); ++i)
      {
        EEPROM.write(i, qsid[i]);
        Serial.print("Wrote: ");
        Serial.println(qsid[i]);
      }
      Serial.println("writing eeprom pass:");
      for (int i = 0; i < qpass.length(); ++i)
      {
        EEPROM.write(20 + i, qpass[i]);
        Serial.print("Wrote: ");
        Serial.println(qpass[i]);
      }

      Serial.println("writing eeprom Email:");
      for (int i = 0; i < mail.length(); ++i)
      {
        EEPROM.write(40 + i, mail[i]);
        Serial.print("Wrote: ");
        Serial.println(mail[i]);
      }

      String configstate = "(";
      EEPROM.write(600, configstate[0]);
      EEPROM.commit();
      content = "{\"Successful\":\"Saved To Memory... Please Wait For Reconfiguration\"}";
      statusCode = 200;
      //  ESP.restart();
    } else {
      content = "{\"Error\":\"404 not found\"}";
      statusCode = 404;
      Serial.println("Sending 404");
    }
    servercred.sendHeader("Access-Control-Allow-Origin", "*");
    servercred.send(statusCode, "application/json", content);
    delay(2000);
    ESP.restart();
  });



}


/*
   THIS "CREATEWEBSERVER" FUNCTION WAS CALLED IN THE ABOVE FUNCTION. ONCE THE "CREATEWEBSERVER" WAS CALLED, THE FUNCTION WAS IMMEDIATELY EXECUTED
   THIS FUNCTION IS WHERE THE MAIN WEBSERVER WAS DESIGNED.
*/

void createservermail()
{
  /*
     THE "SERVERCRED" FUNCTION IS USED FOR DISPLAYING AND OBTAINING THE USER CREDENTIALS
  */


  servercred.on("/", []() {
    IPAddress ip = WiFi.softAPIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

    content += "<p>";
    content = "<!DOCTYPE HTML>\r\t\n<html><div style='text-align:center;font-weight:bolder;font-size:20px !important;margin-bottom:20px;'>Welcome to Email Configuration page ,Please Input Your Email Address For Notification</div>";
    content += "<center><p>";
    content += "<p>";
    content += st;
    content += "</p><form method='get' action='setting'><label>EMAIL:</label><input name='email' length=100><input type='submit'></form>";
    content += "<script> function ssidlize(e){document.getElementById('ssid').value = e}</script></html>";
    servercred.send(200, "text/html", content);
  });
  servercred.on("/scan", []() {
    //setupAP();
    IPAddress ip = WiFi.softAPIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    content = "<!DOCTYPE HTML>\r\n<html>go back";
    servercred.send(200, "text/html", content);
  });

  /*
       THIS SESSION IS RESPONSIBLE FOR ASSIGNING THE USER CREDENNTIALS TO THREE VARIABLE SO WE CAN SAVE THEM IN THE MEMORY
  */
  servercred.on("/setting", []() {

    /*
       SAVING THE CREDENTIALS
    */

    String mail = servercred.arg("email");

    Serial.print("EMAIL: ");
    Serial.println(mail);

    /*
       CLEARING UP THE MEMORY
    */

    if (mail.length() > 0) {
      Serial.println("clearing eeprom");
      for (int i = 40; i < 1000; ++i) {
        EEPROM.write(i, 0);
      }

      Serial.println(mail);
      Serial.println("");
      /*
         THIS FOR LOOP IS RESPONSIBLE FOR STORING THE CREDENTIALS GOTTEN FROM THE USER
      */


      Serial.println("writing eeprom Email:");
      for (int i = 0; i < mail.length(); ++i)
      {
        EEPROM.write(40 + i, mail[i]);
        Serial.print("Wrote: ");
        Serial.println(mail[i]);
      }
      String sign = "@";
      EEPROM.write(512, sign[0]);
      String configstate = "(";
      EEPROM.write(600, configstate[0]);
      EEPROM.commit();
      content = "{\"Successful\":\"Email Saved. Please Wait While System Reconfigures Itself\"}";
      statusCode = 200;
      //  ESP.restart();
    } else {
      content = "{\"Error\":\"404 not found\"}";
      statusCode = 404;
      Serial.println("Sending 404");
    }
    servercred.sendHeader("Access-Control-Allow-Origin", "*");
    servercred.send(statusCode, "application/json", content);
    delay(2000);
    ESP.restart();
  });

}


///////////////////////////////////////////////////////////////////////////////////////
////THIS FUNCTION IS AIMED AT SETTING UP THE SOFT AP MODE OF THE MICROCONTROLLER///////
//void setupAP(void);
void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int u = 0;
  int n = 0;
  while (u < 20) {
    //  Serial.println("testing setup");
    u = u + 1;
    n = WiFi.scanNetworks(); // THIS IS THE LINE THAT CALLS THE SCAN FUNCTION WHICH WILL SCAN THE VICINITY TO GET THE AVAILABLE WIFI
  }
  Serial.println("scan done");
  if (n == 0) {

    Serial.println("no networks found");
  }
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {

      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");

      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    /*
       THIS SESSION Print SSID and RSSI for each network found
    */
    st += "<li style='cursor:pointer;text-decoration:none' onclick=\"ssidlize('";
    st += WiFi.SSID(i);
    st += "')\">";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
    st += ")";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  /*
     THIS SESSION PUTS THE ESP IN THE SOFT AP MODE AND LAUNCHES THE WEBSERVER
  */
  String hotspot = "UNITEC-" + WiFi.macAddress();
  char hotspot1[1000];
  hotspot.toCharArray(hotspot1, 100);

  WiFi.softAP(hotspot1, "");
  Serial.println("Initializing_softap_for_wifi credentials_modification");
  launchWeb();
  Serial.println("over");
}


/////////////////////////////////////////////////////////////////////////
////////// pin declaration  //////////////////
const int liquid_sensor = 34;
const int Buzzer = 2;

////////////////  Variables ///////////////////

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */
float liquid_level = 0;


////////////////////////////////////////////////////////////////////
/////////////////// THE ERASE FUNCTION ////////////////////////////
///////////////////////////////////////////////////////////////////

String wifi = "";
void erase() {

  ///////////////////////////////////////////////
  //////////// SCAN FOR AVAILABLE WIFI/////////
  ////////////////////////////////////////////

  delay(100);
  int n = WiFi.scanNetworks(); // THIS IS THE LINE THAT CALLS THE SCAN FUNCTION WHICH WILL SCAN THE VICINITY TO GET THE AVAILABLE WIFI
  Serial.println("scan done");
  if (n == 0) {

    Serial.println("no networks found");
  }
  else
  {
    Serial.println("Reading EEPROM ssid");
    delay(1000);
    for (int i = 0; i < 20; ++i)
    {
      esid += char(EEPROM.read(i));
    }
    Serial.println();
    Serial.print("SSID: ");
    Serial.println(esid);

    Serial.println("Reading EEPROM pass");
    for (int i = 20; i < 40; ++i)
    {
      epass += char(EEPROM.read(i));
    }
    Serial.println();
    Serial.print("PASS: ");
    Serial.println(epass);

    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {

      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      wifi = WiFi.SSID(i);
      Serial.print("WiFi is: ");
      Serial.println(wifi);
      if (wifi == esid.c_str()) {
        WiFi.begin(esid.c_str(), epass.c_str());
        delay(2000);
        WiFi.begin(esid.c_str(), epass.c_str());

        if (WiFi.status() == WL_CONNECTED)
        {
          Serial.println("WIFI HAVING THE EEPROM SSID FOUND");
          compare = true;
          break;
        }
      }
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");

      delay(10);
    }
  }

  if (compare == true) {
    Serial.println("END OF ERASING FUNCTION");
    return ;
  }
  else {
    Serial.println("CLEARING EEPROM FOR SSID AND PASSWORD ");
    Serial.println("BECAUSE IT DOES NOT MATCH THE AVAILABLE WIFIS");
    for (int i = 0; i < 60; ++i) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    Serial.println("END OF ERASING FUNCTION");
  }


}

////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
/*
  THE FUNCTION BELOW WILL BE CALLED INTERMEDIATELY
  TO CHECK FOR LATEST UPDATES AND THEN UPDATE THE DEVICE
  IF LATEST FIRMWARE IS AVAILABLE
*/
void firmwareUpdate() {
  bool updatedNeeded = esp32FOTA.execHTTPcheck();
  if (updatedNeeded)
  {

    /*
       THIS READS THE EMAIL OF THE USER
    */
    Serial.println("Reading EEPROM EMAIL");
    delay(1000);
    for (int i = 40; i < 140; ++i)
    {
      Email += char(EEPROM.read(i));
    }
    Serial.println();
    Serial.print("Email: ");
    Serial.println(Email);


    //    HTTPClient http;  // THIS INITIALIZES THE PROTOCOL OF FOR THE HTTP
    //
    //
    //
    //    /*
    //      THIS LINE OF CODE BEGINS THE CONNECTION TO THE API FOR THE NOTIFICATION
    //
    //    */
    //
    //    http.begin("http://unitecprotection.com/test/upgrade.php");
    //    http.addHeader("Content-Type", "application/json");
    //
    //    StaticJsonDocument<1000> doc;
    //    // Add values in the document
    //    //
    //    macAdd = WiFi.macAddress();
    //    doc["id"] = macAdd;
    //    doc["email"] = Email;
    //
    //    String requestBody;
    //    serializeJson(doc, requestBody);
    //
    //    int httpResponseCode = http.POST(requestBody);
    //
    //    if (httpResponseCode > 0) {
    //
    //      String response = http.getString();
    //
    //      Serial.println(httpResponseCode);
    //      Serial.println(response);
    //
    //    }
    //    else {
    //
    //      Serial.printf("Error occurred while sending HTTP POST: %s\n", http.errorToString(httpResponseCode).c_str());
    //
    //    }


    HTTPClient client;  // THIS INITIALIZES THE PROTOCOL OF FOR THE HTTP

    /*
      THIS LINE OF CODE BEGINS THE CONNECTION TO THE API FOR THE NOTIFICATION

    */
    int q = 0;
    while (q <= 20) {
      q = q + 1;
      client.begin( "http://unitecprotection.com/test/upgrade.php");
      client.addHeader("Content-Type", "application/json");
    }
    StaticJsonDocument<1000> doc;
    // Add values in the document
    //
    macAdd = WiFi.macAddress();
    doc["id"] = macAdd;
    doc["email"] = Email;


    String requestBody;
    serializeJson(doc, requestBody);
    //     q=0;
    //     int httpCode = 0;
    //while (q<=20){
    //  q=q+1;
    Serial.println(requestBody);
    int httpCode = client.POST(String(requestBody));
    //}
    if (httpCode > 0) {
      String response = client.getString();

      Serial.println(httpCode);
      Serial.println(response);
      client.end();
    }
    else {
      //      Serial.println("Error occurred while sending HTTP POST: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    esp32FOTA.execOTA();
    Serial.println("UPDATE NEEDED");
  }

  delay(2000);

}

const int batterystate = 36;
float values = 0.0;

///drawn end //////////////////
///////////////////////////////
int waking = 0;
void setup() {
  //Serial.begin(115200);
  EEPROM.begin(1000); //Initialasing EEPROM
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause != ESP_SLEEP_WAKEUP_ULP) {
    ++bootCount;
    printf("Boot number: %d\n" , bootCount);
    printf("Not ULP wakeup\n");
    waking = 1;
    int rising = EEPROM.read(800);
    printf("rising = %d\n", rising);

    if (rising == 2) {
      printf("restarting the system\n");
      EEPROM.write(800, 1);
      EEPROM.commit();
      ESP.restart();
    }
    init_ulp_program();
  } else {

    // ***** HERE YOUR SKETCH *****
    waking = 2;
    EEPROM.write(800, 2);
    EEPROM.commit();
    printf("Deep sleep wakeup\n");
    printf("ULP did %d measurements since last reset\n", ulp_sample_counter & UINT16_MAX);
    printf("Thresholds:  low=%d  high=%d\n", ulp_low_threshold, ulp_high_threshold);
    ulp_ADC_reading &= UINT16_MAX;
    printf("Value=%d was %s threshold\n", ulp_ADC_reading, ulp_ADC_reading < ulp_low_threshold ? "below" : "above");

  }
  // printf("Entering deep sleep\n\n");
  if (waking == 2) {
    delay(100);
    start_ulp_program();
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                   " Seconds");
    esp_sleep_enable_ulp_wakeup() ;
    esp_deep_sleep_start();
  }

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  pinMode(2, OUTPUT);
  pinMode(batterystate, INPUT);
  Serial.begin(115200); //Initialising if(DEBUG)Serial Monitor
  EEPROM.begin(1000); //Initialasing EEPROM
  WiFi.disconnect();    // WE DISCOUNTED FROM EVERY INITIAL WIFI AS STATED IN THE REQUIREMENT
  Serial.println("Initializing version 14");
  pinMode(liquid_sensor, INPUT);
  esp32FOTA.checkURL = "http://unitecprotection.com/test/update.json";
  Serial.println();
  Serial.println("Disconnecting current wifi connection");
  EEPROM.write(800, 1);
  EEPROM.commit();

  delay(10);
  pinMode(15, INPUT);
  Serial.println();
  Serial.println();
  Serial.println("Startup");
  Serial.println();
  //erase();
  /*
    THIS SESSION Read eeprom for WIFI NAME and WIFI PASSWORD
  */

  Serial.println("Reading EEPROM ssid");
  delay(1000);
  for (int i = 0; i < 20; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println();
  Serial.println("Reading EEPROM pass");
  for (int i = 20; i < 40; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("PASS: ");
  Serial.println(epass);

  digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);
  digitalWrite(2, LOW);
  delay(1000);
  digitalWrite(2, HIGH);
  delay(1000);
  digitalWrite(2, LOW);
  delay(1000);
  digitalWrite(2, HIGH);
  delay(1000);
  digitalWrite(2, LOW);
  delay(1000);
  WiFi.disconnect();
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  WiFi.begin(esid.c_str(), epass.c_str());



  int checking = 0;
  //  while (checking < 100) {
  //    checking = checking + 1;
  //    /*
  //       THIS WIFI.BEGIN ENABLES THE WIFI FEATURE
  //    */
  //    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  //    WiFi.begin(esid.c_str(), epass.c_str());
  //  }
  //  WiFi.begin(esid.c_str(), epass.c_str());

  digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);
  digitalWrite(2, LOW);
  delay(1000);
  digitalWrite(2, HIGH);
  /*
     THIS WHILE LOOP CHECKS AGAIN TO VERIFY THE WIFI NAME AND PASSWORD
  */

  int c = 0;
  //////// BEGINNING OF IF STATEMENT ////////////////
  while (c < 50) {
    ++c;
    if (WiFi.status() == WL_CONNECTED)
    {
      /*
        THIS READS THE EMAIL OF THE USER
      */
      Serial.println("Reading EEPROM EMAIL after connection");
      for (int i = 40; i < 140; ++i)
      {
        Email += char(EEPROM.read(i));
      }
      Serial.println();
      Serial.print("Email: ");
      Serial.println(Email);
      if (Email == "") {
        Serial.println("Launch of mail retrieval");
        launchWebemail();
        setupAP();// Setup HotSpot
        /*
          THIS READS THE EMAIL OF THE USER
        */
        Serial.println("Reading EEPROM EMAIL");
        delay(1000);
        for (int i = 40; i < 140; ++i)
        {
          Email += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("Email: ");
        Serial.println(Email);

        Serial.println();
        Serial.println("Waiting.");
        /*
          THIS WHILE LOOP KEEPS THE WEBSERVER RUNNING AS FAR AS THE WEBSERVER IS OPEN AND THERE IS NO WIFI CONNECTION YET
          BUT ONCE THERE IS WIFI CONNECTION OR THE USER SUBMITS THE CREDENTIALS ON THE WEBSERVER PAGE, THE WHILE LOOP BREAKS
          AND THE SYSTEM RESTARTS HENCE OPERATING ON THE NEW CREDENTIALS
        */
        String sign = "64";
        String readme = "";
        while (sign != readme)
        {
          readme = String(EEPROM.read(512));
          Serial.println(readme);

          //stat = char(EEPROM.read(512));
          /*
            THIS READS THE EMAIL OF THE USER
          */
          Serial.println("Reading EEPROM EMAIL");

          for (int i = 40; i < 140; ++i)
          {
            Email += char(EEPROM.read(i));
          }
          Serial.println();
          Serial.print("Email: ");
          Serial.println(Email);
          Serial.print(".");
          delay(100);
          servercred.handleClient();
        }
      }
      break;
    }
  }
  //  c = 0;


  /*
    THIS READS THE EMAIL OF THE USER
  */
  Email = "";
  Serial.println("Reading EEPROM EMAIL");
  delay(1000);
  for (int i = 40; i < 140; ++i)
  {
    Email += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("Email: ");
  Serial.println(Email);

  macAdd = WiFi.macAddress();
  Serial.println();
  Serial.print("mac_Addess: ");
  Serial.println(macAdd);

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // THIS CHEECKS IF THE SYSTEM WAS CONFIGURED SO AS TO SEND THE REGULAR WELCOME MESSAGE AFTER EACH CONFIGURATION/////
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////


  String configState = "";
  String checkout = "40";
  configState  = String(EEPROM.read(600));
  Serial.print("Checkout is: ");
  Serial.println(configState);


  if (configState == checkout) {
    Serial.println("Configured state yes");
    String configstate = ")";
    EEPROM.write(600, configstate[0]);
    EEPROM.commit();
    /////////////////////////////////////////////////////
    /////////////////////////////////////////////////////
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
    //   WiFi.disconnect();
    WiFi.begin(esid.c_str(), epass.c_str());

    c = 0;
    //////// BEGINNING OF IF STATEMENT ////////////////
    while (c < 50) {
      Serial.println("Trying to connect to internet to send the welcome mail");

      ++c;
      if (WiFi.status() == WL_CONNECTED)
      {

        Serial.println("Connected sending email");
        WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
        //       WiFi.disconnect();
        //        WiFi.begin(esid.c_str(), epass.c_str());
        //  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //disable brownout detector
        delay(2000);
        WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
        //      WiFi.disconnect();
        //        WiFi.begin(esid.c_str(), epass.c_str());
        HTTPClient http;  // THIS INITIALIZES THE PROTOCOL OF FOR THE HTTP

        /*
          THIS LINE OF CODE BEGINS THE CONNECTION TO THE API FOR THE WELCOME NOTIFICATION

        */


        http.begin("http://unitecprotection.com/test/welcome.php");
        http.addHeader("Content-Type", "application/json");

        StaticJsonDocument<1000> doc;
        // Add values in the document
        //

        doc["email"] = Email;
        doc["id"] = macAdd;

        String requestBody;
        serializeJson(doc, requestBody);
        delay (1000);
        int httpResponseCode = http.POST(requestBody);

        if (httpResponseCode > 0) {

          String response = http.getString();

          Serial.println(httpResponseCode);
          Serial.println(response);
          break;

        }
        else {

          Serial.printf("Error occurred while sending message HTTP POST: %s\n", http.errorToString(httpResponseCode).c_str());

        }

      }
    }
  }

  if (esid == "" || epass == "" || macAdd == "" || Email == "" ) {
    Empty = true;
    //    Serial.println("INCOMPLETE USER'S DATA");
  }

}

void loop() {
  if (Empty == false) {
    Serial.println("COMPLETE USER'S DATA");

    //    Email = "";
    macAdd = "";

    /*
       THE FUNCTION BELOW IS CALLED TO CHECK FOR FIRMWARE UPDATES

    */
    firmwareUpdate();

    /*
       THE IF ELSE STATEMENT CHECKS IF THERE IS WIFI AVAILABLE
       IF YES DO THE FOLLOWING IF STATEMENT
    */
    //   WiFi.disconnect();
    WiFi.begin(esid.c_str(), epass.c_str());

    //////// BEGINNING OF IF STATEMENT ////////////////
    if ((WiFi.status() == WL_CONNECTED))
    {

      Serial.print("Connected to ");
      Serial.print(esid);
      Serial.println(" Successfully");
      int l = 0;
      int r = 10;
      liquid_level = 0;
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("connected to wifi");
        Serial.println("Reading");
        //  goToDeepSleep();
        values = analogRead(batterystate);
        gpio_wakeup_disable(GPIO_NUM_0);
        int timing = 36;
        pinMode(timing, OUTPUT);
        rtc_gpio_init(GPIO_NUM_0);
        gpio_reset_pin(GPIO_NUM_0);
        Serial.println(values);
        rtc_gpio_deinit(GPIO_NUM_0);
        values = (values * 3.3) / 2500;

        if (values <= 1.7) {
          delay(1000);
          HTTPClient client;  // THIS INITIALIZES THE PROTOCOL OF FOR THE HTTP

          /*
            THIS LINE OF CODE BEGINS THE CONNECTION TO THE API FOR THE NOTIFICATION

          */
          int q = 0;
          while (q <= 20) {
            q = q + 1;
            client.begin( "http://unitecprotection.com/test/battery.php");
            client.addHeader("Content-Type", "application/json");
          }
          delay(1000);
          StaticJsonDocument<1000> doc;
          // Add values in the document
          //
          macAdd = WiFi.macAddress();
          doc["id"] = macAdd;
          doc["email"] = Email;
          doc["battery level"] = values;


          String requestBody;
          serializeJson(doc, requestBody);
          //     q=0;
          //     int httpCode = 0;
          //while (q<=20){
          //  q=q+1;
          Serial.println(requestBody);
          delay(1000);
          int httpCode = client.POST(String(requestBody));
          //}
          if (httpCode > 0) {
            String response = client.getString();

            Serial.println(httpCode);
            Serial.println(response);
            client.end();
          }
          else {
            Serial.printf("Error occurred while sending HTTP POST: %s\n", client.errorToString(httpCode).c_str());
          }
        }
        else {
          Serial.println("battery sufficient");
        }
      }
      /*
          THIS WHILE LOOP CHECKS THE LIQUID SENSOR STATE. IF THERE IS WATER THE LIQUID_LEVEL READS "HIGH". IF NO LIQUID,
          THE LIQUID LEVEL READS "LOW"
      */
      /// THE "L" VARIABLE HELPS IN LOOPING THE FUNCTION
      while (l < 10) {
        liquid_level = 0;
        liquid_level = digitalRead(liquid_sensor);
        Serial.println(liquid_level);
        l = l + 1;
      }
      l = 0;


      /*
         IF THERE IS NO LIQUID, THE IF STATEMENT BELOW IS PERFORMED
      */

      if (liquid_level == LOW) {
        /*
           THIS READS THE EMAIL OF THE USER
        */
        Serial.println("Reading EEPROM EMAIL");
        delay(1000);
        for (int i = 40; i < 140; ++i)
        {
          Email += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("Email: ");
        Serial.println(Email);
        digitalWrite(Buzzer, HIGH); // THE BUZZER CONNECTED TO D2 GOES HIGH
        delay(1000);
        HTTPClient http;  // THIS INITIALIZES THE PROTOCOL OF FOR THE HTTP


        /*
          THIS LINE OF CODE BEGINS THE CONNECTION TO THE API FOR THE NOTIFICATION

        */

        http.begin("http://unitecprotection.com/test/json.php");
        http.addHeader("Content-Type", "application/json");
        delay(1000);
        StaticJsonDocument<1000> doc;
        // Add values in the document
        //
        macAdd = WiFi.macAddress();
        doc["id"] = macAdd;
        doc["email"] = Email;


        String requestBody;
        serializeJson(doc, requestBody);
        delay(1000);
        int httpResponseCode = http.POST(requestBody);

        if (httpResponseCode > 0) {

          String response = http.getString();

          Serial.println(httpResponseCode);
          Serial.println(response);

        }
        else {

          Serial.printf("Error occurred while sending HTTP POST: %s\n", http.errorToString(httpResponseCode).c_str());

        }

        delay(1000);

        for (int i = 0; i < 20; ++i)
        {
          esid += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("SSID: ");
        Serial.println(esid);
        Serial.println("Reading EEPROM pass");

        for (int i = 20; i < 40; ++i)
        {
          epass += char(EEPROM.read(i));
        }
        Serial.print("PASS: ");
        Serial.println(epass);

        Serial.println("Reading EEPROM EMAIL");
        delay(1000);
        for (int i = 40; i < 140; ++i)
        {
          Email += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("Email: ");
        Serial.println(Email);

        if (esid != "" && epass != "" && Email != "" ) {
          goToDeepSleep();
          //    Serial.println("INCOMPLETE USER'S DATA");
        }
      }



      /*
         IF THERE IS LIQUID, THE ELSEIF STATEMENT BELOW IS PERFORMED
      */
      else if (liquid_level == HIGH) {

        Serial.println(liquid_level);
        delay(1000);
        digitalWrite(Buzzer, LOW);

        /*
                 THE ESP GOES TO SLEEP AFTER READING NOTING THAT THERE IS LIQUID
        */
        for (int i = 0; i < 20; ++i)
        {
          esid += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("SSID: ");
        Serial.println(esid);
        Serial.println("Reading EEPROM pass");

        for (int i = 20; i < 40; ++i)
        {
          epass += char(EEPROM.read(i));
        }
        Serial.print("PASS: ");
        Serial.println(epass);

        Serial.println("Reading EEPROM EMAIL");
        delay(1000);
        for (int i = 40; i < 140; ++i)
        {
          Email += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("Email: ");
        Serial.println(Email);

        if (esid != "" && epass != "" && Email != "" ) {
          goToDeepSleep();
          //    Serial.println("INCOMPLETE USER'S DATA");
        }

        Serial.println("Done sleeping");
      }



      /*
         IF SENSOR IS GIVING INVALID READINGS, THE ELSE STATEMENT BELOW IS PERFORMED
      */
      else {
        Serial.println("Faulty sensor connection");
        digitalWrite(Buzzer, HIGH);
      }
    }
    ////////// END OF IF STATEMENT ////////////

    /*
       IF NO WIFI AVAILABLE PERFORM THE ELSE STATEMENT
    */

    //////////// BEGINNING OF ELSE STATEMENT WHEN THERE IS NO INTERNET CONNECTION, THIS ELSE STATEMENT IS PERFORMED///////////////
    else
    {
      delay(1000);
      Serial.println("DISCONNECTED");
      digitalWrite(Buzzer, HIGH);
      int l = 0;
      int r = 10;
      liquid_level = 0;
      /*
          THIS WHILE LOOP CHECKS THE LIQUID SENSOR STATE. IF THERE IS WATER THE LIQUID_LEVEL READS "HIGH". IF NO LIQUID,
          THE LIQUID LEVEL READS "LOW"
      */
      /// THE "L" VARIABLE HELPS IN LOOPING THE FUNCTION
      while (l < 20) {
        liquid_level = 0;
        liquid_level = digitalRead(liquid_sensor);
        Serial.println(liquid_level);
        l = l + 1;
      }
      l = 0;

      /*
         IF THERE IS NO LIQUID, THE IF STATEMENT BELOW IS PERFORMED
      */
      if (liquid_level == LOW) {

        Serial.println("EMPTY TANK");

        Serial.println(liquid_level);
        delay(1000);
        digitalWrite(Buzzer, HIGH);  // THE BUZZER CONNECTED TO D2 GOES HIGH


        /*
          SINCE THERE IS NO WIFI, THE MICROCONTROLLER CHECKS THE MEMORY AGAIN TO OBTAIN THE CREDENTIALS USING THE WHILE LOOP
        */
        int checking = 0;

        /*
           THIS FOR LOOP OBTAINS THE WIFI PASSWORD FROM THE MEMORY////
        */
        for (int i = 0; i < 20; ++i)
        {
          esid += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("SSID: ");
        Serial.println(esid);
        Serial.println("Reading EEPROM pass");


        for (int i = 20; i < 40; ++i)
        {
          epass += char(EEPROM.read(i));
        }
        Serial.print("PASS: ");
        Serial.println(epass);


        //        while (checking < 20) {
        //          checking = checking + 1;

        //  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
        //       WiFi.disconnect();
        WiFi.begin(esid.c_str(), epass.c_str());
        // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //disable brownout detector
        //       }

        /*
           THE WIFI.BEGIN TRIES TO INITIALIZE THE WIFI FEATURE
        */
        //     WiFi.begin(esid.c_str(), epass.c_str());


        /*
           IF THERE IS NO WIFI STILL, THE IF STATEMENT IS PERFORMED
        */
        if ((WiFi.status() != WL_CONNECTED))
        {

          /*
              THIS SESSION CHECKS THE WIFI STATUS BY CALLING THE "testWifi()" FUNCTION
              IF THE "testWifi()" FUNCTION RETURNS TRUE, THE IF FUNCTION IS PERFORMED
          */
          if (testWifi() && (digitalRead(15) != 1))
          {
            Serial.println(" connection status positive");
            return;
          }

          /*
              IF THE "testWifi()" FUNCTION RETURNS FALSE, THIS ELSE FUNCTION IS PERFORMED
              ONCE PERFORMED, THE "" FUNCTION AND "" FUNCTION ARE CALLED. THIS TWO FUNCTIONS WERE DECLARED ABOVE
              AND THEY ARE RESPONSIBLE FOR LAUNCHING THE WEBSERVER WHICH THE USER WILL LOG INTO AND INPUT THE CREDENTIALS
          */
          else
          {
            Serial.println("Connection Status Negative / D15 HIGH");
            Serial.println("Turning the HotSpot On");
            launchWeb();
            setupAP();// Setup HotSpot
          }
          Serial.println();
          Serial.println("Waiting.");
          /*
            THIS WHILE LOOP KEEPS THE WEBSERVER RUNNING AS FAR AS THE WEBSERVER IS OPEN AND THERE IS NO WIFI CONNECTION YET
            BUT ONCE THERE IS WIFI CONNECTION OR THE USER SUBMITS THE CREDENTIALS ON THE WEBSERVER PAGE, THE WHILE LOOP BREAKS
            AND THE SYSTEM RESTARTS HENCE OPERATING ON THE NEW CREDENTIALS
          */
          int j = 0;
          while ((WiFi.status() != WL_CONNECTED))
          {

            Serial.print("*");
            delay(100);
            servercred.handleClient();
            delay(500);
          }
          delay(1000);
        }

        for (int i = 0; i < 20; ++i)
        {
          esid += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("SSID: ");
        Serial.println(esid);
        Serial.println("Reading EEPROM pass");

        for (int i = 20; i < 40; ++i)
        {
          epass += char(EEPROM.read(i));
        }
        Serial.print("PASS: ");
        Serial.println(epass);

        Serial.println("Reading EEPROM EMAIL");
        delay(1000);
        for (int i = 40; i < 140; ++i)
        {
          Email += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("Email: ");
        Serial.println(Email);

        //        if (esid != "" && epass != "" && Email != "" ) {
        //          goToDeepSleep();
        //          //    Serial.println("INCOMPLETE USER'S DATA");
        //        }

      }


      /*
         IF THERE IS LIQUID, THE ELSE IF STATEMENT BELOW IS PERFORMED
      */
      else if (liquid_level == HIGH) {

        Serial.println("FULL TANK");
        Serial.println(liquid_level);
        delay(1000);
        digitalWrite(Buzzer, LOW);

        /*
                 THE ESP GOES TO SLEEP AFTER READING NOTING THAT THERE IS LIQUID
        */

        /*
          SINCE THERE IS NO WIFI, THE MICROCONTROLLER CHECKS THE MEMORY AGAIN TO OBTAIN THE CREDENTIALS USING THE WHILE LOOP
        */
        int checking = 0;

        /*
           THIS FOR LOOP OBTAINS THE WIFI PASSWORD FROM THE MEMORY////
        */
        for (int i = 0; i < 20; ++i)
        {
          esid += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("SSID: ");
        Serial.println(esid);
        Serial.println("Reading EEPROM pass");


        for (int i = 20; i < 40; ++i)
        {
          epass += char(EEPROM.read(i));
        }
        Serial.print("PASS: ");
        Serial.println(epass);


        //        while (checking < 20) {
        //          checking = checking + 1;

        //  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
        //       WiFi.disconnect();
        WiFi.begin(esid.c_str(), epass.c_str());
        //  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); //disable brownout detector
        //        }

        /*
           THE WIFI.BEGIN TRIES TO INITIALIZE THE WIFI FEATURE
        */
        //WiFi.begin(esid.c_str(), epass.c_str());


        /*
           IF THERE IS NO WIFI STILL, THE IF STATEMENT IS PERFORMED
        */
        if ((WiFi.status() != WL_CONNECTED))
        {

          /*
              THIS SESSION CHECKS THE WIFI STATUS BY CALLING THE "testWifi()" FUNCTION
              IF THE "testWifi()" FUNCTION RETURNS TRUE, THE IF FUNCTION IS PERFORMED
          */
          if (testWifi() && (digitalRead(15) != 1))
          {
            Serial.println(" connection status positive");
            return;
          }

          /*
              IF THE "testWifi()" FUNCTION RETURNS FALSE, THIS ELSE FUNCTION IS PERFORMED
              ONCE PERFORMED, THE "" FUNCTION AND "" FUNCTION ARE CALLED. THIS TWO FUNCTIONS WERE DECLARED ABOVE
              AND THEY ARE RESPONSIBLE FOR LAUNCHING THE WEBSERVER WHICH THE USER WILL LOG INTO AND INPUT THE CREDENTIALS
          */
          else
          {
            Serial.println("Connection Status Negative / D15 HIGH");
            Serial.println("Turning the HotSpot On");
            launchWeb();
            setupAP();// Setup HotSpot
          }
          Serial.println();
          Serial.println("Waiting.");
          /*
            THIS WHILE LOOP KEEPS THE WEBSERVER RUNNING AS FAR AS THE WEBSERVER IS OPEN AND THERE IS NO WIFI CONNECTION YET
            BUT ONCE THERE IS WIFI CONNECTION OR THE USER SUBMITS THE CREDENTIALS ON THE WEBSERVER PAGE, THE WHILE LOOP BREAKS
            AND THE SYSTEM RESTARTS HENCE OPERATING ON THE NEW CREDENTIALS
          */
          int j = 0;
          while ((WiFi.status() != WL_CONNECTED))
          {
            //++j;
            //            WiFi.begin(esid.c_str(), epass.c_str());
            Serial.print("*");
            delay(100);
            servercred.handleClient();
            delay(500);
          }
          delay(1000);
        }

        for (int i = 0; i < 20; ++i)
        {
          esid += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("SSID: ");
        Serial.println(esid);
        Serial.println("Reading EEPROM pass");

        for (int i = 20; i < 40; ++i)
        {
          epass += char(EEPROM.read(i));
        }
        Serial.print("PASS: ");
        Serial.println(epass);

        Serial.println("Reading EEPROM EMAIL");
        delay(1000);
        for (int i = 40; i < 140; ++i)
        {
          Email += char(EEPROM.read(i));
        }
        Serial.println();
        Serial.print("Email: ");
        Serial.println(Email);

        if (esid != "" && epass != "" && Email != "" ) {
          goToDeepSleep();
          //    Serial.println("INCOMPLETE USER'S DATA");
        }
      }


    }



  }
  else if (Empty == true) {

    Serial.println("Incomplete user's data");
    WiFi.disconnect();
    Serial.println("Connection Status Negative / D15 HIGH");
    Serial.println("Turning the HotSpot On");
    launchWeb();
    setupAP();// Setup HotSpot
    Serial.println();
    Serial.println("Waiting.");
    /*
      THIS WHILE LOOP KEEPS THE WEBSERVER RUNNING AS FAR AS THE WEBSERVER IS OPEN AND THERE IS NO WIFI CONNECTION YET
      BUT ONCE THERE IS WIFI CONNECTION OR THE USER SUBMITS THE CREDENTIALS ON THE WEBSERVER PAGE, THE WHILE LOOP BREAKS
      AND THE SYSTEM RESTARTS HENCE OPERATING ON THE NEW CREDENTIALS
    */
    while ((WiFi.status() != WL_CONNECTED))
    {
      Serial.print(".");
      delay(100);
      //      WiFi.begin(esid.c_str(), epass.c_str());
      servercred.handleClient();
    }
    delay(1000);
  }
}


/*
   THIS FUNCTION IS RESPONSIBLE FOR PUTTING THE ESP32 INTO SLEEP ONCE IT IS DONE WITH ITS ROUTINE CHECKS
*/

void goToDeepSleep() {
  /*
    First we configure the wake up source
    We set our ESP32 to wake up every 5 seconds
  */
  init_ulp_program();
  delay(100);
  start_ulp_program();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");
  esp_sleep_enable_ulp_wakeup();
  esp_deep_sleep_start();
}

static void init_ulp_program()
{
  esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
                                  (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
  ESP_ERROR_CHECK(err);

  rtc_gpio_init(ulp_27);
  rtc_gpio_set_direction(ulp_27, RTC_GPIO_MODE_OUTPUT_ONLY);

  rtc_gpio_init(ulp_13);
  rtc_gpio_set_direction(ulp_13, RTC_GPIO_MODE_OUTPUT_ONLY);


  /* Configure ADC channel */
  /* Note: when changing channel here, also change 'adc_channel' constant
     in adc.S */
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_ulp_enable();
  ulp_low_threshold = 2 * (4095 / 3.3);  //1 volt
  ulp_high_threshold = 2.5 * (4095 / 3.3);   // 2 volts

  /* Set ULP wake up period to 100ms */
  ulp_set_wakeup_period(0, 100 * 1000);

  /* Disable pullup on GPIO15, in case it is connected to ground to suppress
     boot messages.
  */
  //  rtc_gpio_pullup_dis(GPIO_NUM_15);
  //  rtc_gpio_hold_en(GPIO_NUM_15);
}

static void start_ulp_program()
{
  /* Start the program */
  esp_err_t err = ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t));
  ESP_ERROR_CHECK(err);
}
