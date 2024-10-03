#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
bool connected = false;
String data = "";
StaticJsonDocument<200> doc;
String teamId = "";

// Wi-Fi network parameters
String ssid = "";
String password = "";
bool connected_WiFi;
#define CONNECT_TIMEOUT 15000 // ms
long connectStart = 0;

void deviceConnected(
    esp_spp_cb_event_t event,
    esp_spp_cb_param_t *param)
{
  if (event == ESP_SPP_SRV_OPEN_EVT)
  {
    Serial.println("Device Connected");
    connected = true;
  }
  if (event == ESP_SPP_CLOSE_EVT)
  {
    Serial.println("Device disconnected");
    connected = false;
  }
}

void getNetworks()
{
  String message_teamId = doc["teamId"];
  teamId = message_teamId;
  int n = WiFi.scanNetworks();

  if (n == 0)
  {
    Serial.println("No network");
  }
  else
  {
    for (int i = 0; i < n; i++)
    {
      DynamicJsonDocument network(1024);
      network["ssid"] = WiFi.SSID(i);
      network["strength"] = WiFi.RSSI(i);
      network["encryption"] = WiFi.encryptionType(i);
      network["teamId"] = teamId;
      
      String jsonString;
      serializeJson(network, jsonString);
      SerialBT.println(jsonString);
      delay(100);
    }
  }
}

void wifiConnect()
{

  String message_ssid = doc["ssid"];
  ssid = message_ssid;
  String message_password = doc["password"];
  password = message_password;

  WiFi.begin(ssid.c_str(), password.c_str());

  int time = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(".");
    delay(500);
    time += 500;
    if (time > 10000)
      break;
  }
  bool wifi_connected;

  if (WiFi.status() != WL_CONNECTED)
    wifi_connected = false;
  else
    wifi_connected = true;

  DynamicJsonDocument doc(200);
  doc["ssid"] = ssid;
  doc["connected"] = wifi_connected;
  doc["teamId"] = teamId;
  String output;
  serializeJson(doc, output);
  SerialBT.println(output);
}

void getData()
{

  String URL = "http://proiectia.bogdanflorea.ro/api/marvel-api/characters";
  HTTPClient client;

  client.begin(URL);
  int statusCode = client.GET();

  if (statusCode != 200)
    Serial.println("Connection failed");
  else
  {
    String data = client.getString();

    DynamicJsonDocument doc(16000);

    DeserializationError err = deserializeJson(doc, data);

    JsonArray recordsArray = doc.as<JsonArray>();

    DynamicJsonDocument finalData(16000);

    for (JsonObject record : recordsArray)
    {
      int id = record["_id"];
      finalData["id"] = id;
      String name = record["name"];
      finalData["name"] = name;
      String image = record["imageUrl"];
      finalData["image"] = image;
      finalData["teamId"] = teamId.c_str();

      String response;
      serializeJson(finalData, response);

      SerialBT.println(response);
    }
  }

  client.end();
}

void getDetails(String id)
{

  String URL = "http://proiectia.bogdanflorea.ro/api/marvel-api/character?id=" + id;
  HTTPClient client;

  client.begin(URL);
  int statusCode = client.GET();

  if (statusCode != 200)
    Serial.println("Error on sending GET request");
  else
  {
    String data = client.getString();

    DynamicJsonDocument doc(16000);

    DeserializationError err = deserializeJson(doc, data);

    DynamicJsonDocument finalData(16000);

    int id = doc["_id"];
    finalData["id"] = id;
    String name = doc["name"];
    finalData["name"] = name;
    String image = doc["imageUrl"];
    finalData["image"] = image;
    String description=doc["overview"];
    finalData["description"]=description;
    finalData["teamId"] = teamId.c_str();

    String response;
    serializeJson(finalData, response);

    SerialBT.println(response);
  }
  client.end();
}

void receivedData()
{
  // Read the data using the appropriate SerialBT functions
  // according to the app specifications
  // The data is terminated by the new line character (\n)
  while (SerialBT.available())
  {
    data = SerialBT.readStringUntil('\n');
    DeserializationError err = deserializeJson(doc, data);
    String action = doc["action"];
    Serial.println(data);

    if (action == "getNetworks")
    {
      getNetworks();
    }
    else if (action == "connect")
    {
      wifiConnect();
    }
    else if (action == "getData")
    {
      getData();
    }
    else if (action == "getDetails")
    {
      String id = doc["id"];

      getDetails(id);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  SerialBT.begin("ESP32");
  SerialBT.register_callback(deviceConnected);
  Serial.println("Device ready for pairing!");
}

void loop()
{
  // Check available Bluetooth data and perform read from the app
  if (SerialBT.available())
  {
    receivedData();
  }
}