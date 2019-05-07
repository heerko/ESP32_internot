
//
// a modified version of "ESP_AsyncFSBrowser.ino" example
//     in "ESPAsyncWebServer" library of Hristo Gochkov (me-no-dev @ github)
//
// licensed under "LPGL-3.0"
//

//
// at first, set of modifications suggested by the gist, https://gist.github.com/dsteinman/f792f0af25ce6d7d1db4b62d29dd4d9e, applied.
//
// --> Hristo Gochkov's original code only supports "esp8266" at the time of this writing.
//     Suggested patched version makes the same ex. code runnable on "esp32"
//
// --> refer to the issue @ https://github.com/me-no-dev/ESPAsyncWebServer/issues/327#issuecomment-382811459
//

//
// afterwards, more changes applied for
//    (2) use a captive portal
//

/***************************************/
/*  CONFIGURATIONS                     */
/***************************************/
const bool is_AP = true; // Set to false to just run as webserver, convenient for testing.

const char* ssid = "YOUR_SSID"; // Wifi credentials
const char* password = "YOUR_PASSWORD";

// AP identifications & credentials
String AP_ssid = "Internot";

/***************************************/
/*  ALL HEADERS and GLOBAL VARIABLES   */
/***************************************/

//file system
#include <FS.h>
#include <SPIFFS.h>

//wifi / tcp
#include <WiFi.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>

//dns server
#include <DNSServer.h>
DNSServer dnsServer;

// json
#include <ArduinoJson.h>

// captive portal -> a self-assigned ip
IPAddress apIP(192, 168, 4, 1);

//web server
#include <ESPAsyncWebServer.h> // <-- this includes, also, "AsyncWebSocket.h" and "AsyncEventSource.h"
AsyncWebServer server(80);

DynamicJsonDocument doc(2048);

String IP = "";

void setup() {
  //serial monitoring
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(10);
  Serial.println("START");

  SPIFFS.begin();   //file system
  JSONFromFile();
  AP_ssid = getSSIDFromFile();

  //wifi
  if ( is_AP ) {
    WiFi.mode(WIFI_AP);
    
    //start ap
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_ssid.c_str(), NULL, 6); //channel selection : 6 (choose one among 6-13 for world-wide accessibility)

    //my ip
    delay(500);
    Serial.print("Captive Network. IP address:"); Serial.println(WiFi.softAPIP());
    IP = WiFi.softAPIP().toString();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    IP = WiFi.localIP().toString();
  }

  //dns service (captive portal)
  dnsServer.start(53, "*", apIP); // reply with provided IP(apIP) to all("*") DNS request

  server.on("/post", HTTP_ANY, [](AsyncWebServerRequest * request) {
    //List all parameters
    Serial.println( "Request for /post" );
    JsonObject message = doc.createNestedObject(); // create object for storing the message
    int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isFile()) { //p->isPost() is also true
        Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if (p->isPost()) {
        String key = p->name().c_str();
        String value = p->value().c_str();
        Serial.printf("POST[%s]: %s\n", key, value);
        message[key] = value;
      } else {
        Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    JSONToFile(); // save the message to file
    request->redirect(String("http://") + IP + "/"); // redirect back to the form
  });

  server.on("/messages", HTTP_ANY, [](AsyncWebServerRequest * request) {
    //List all parameters
    Serial.println( "Request for /messages" );

    AsyncResponseStream *response = request->beginResponseStream("text/plain");
    response->addHeader("Server", "ESP Async Web Server");
    response->addHeader("Access-Control-Allow-Origin", "*"); // allows for testing from a static file:// url
    String json = "";
    serializeJson(doc, json);
    response->print( json );
    //send the response last
    request->send(response);
  });

  //serve-root
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

  server.onNotFound(onRequest);

  //start the web service
  server.begin();
}

void onRequest(AsyncWebServerRequest * request) {
  //Handle Unknown Request
  Serial.println( "Unknown request. Redirect to default" );
  request->redirect(String("http://") + apIP.toString() + "/");
}

void JSONFromFile() {
  File file = SPIFFS.open("/messages.json", FILE_READ);
  String s = file.readString();
  Serial.println( "Read: " + s );
  deserializeJson(doc, s);
  //  JsonArray array = doc.to<JsonArray>();
  file.close();
}

void JSONToFile() {
  File file = SPIFFS.open("/messages.json", FILE_WRITE);
  String json = "";
  serializeJson(doc, json);
  if (!file.print(json)) {
    Serial.println("File write failed");
  }
  file.close();
}

String getSSIDFromFile() {
  String ssid = "Internot";
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String n = file.name();
    String ext = n.substring(n.lastIndexOf("."));
    if( ext == ".ssid" ){
      ssid = n.substring(1,n.lastIndexOf(".")); // first index is 1 to skip the /
    }
    file = root.openNextFile();
  }
  return ssid;
}

void loop() {
  dnsServer.processNextRequest(); // handle DNS
}
