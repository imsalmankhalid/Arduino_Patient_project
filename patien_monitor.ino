/****************************************
 * Include Libraries
 ****************************************/

#include <Wire.h>
#include <ESP8266WiFi.h>
#include "MAX30100_PulseOximeter.h"
#include <PubSubClient.h>


/****************************************
 * Define Constants
 * Credetnials for UBI Dots (Education)
 *  username : johnsmith_p
 *  passwordl: patient1
 *  email: johnsmith@maillink.live
 ****************************************/
#define TOKEN                   "A1E-VnTOfvYuCrPwrVq4s7jDKZMS1j2XB8" // Your Ubidots TOKEN
#define REPORTING_PERIOD_MS     1000
#define MQTT_PORT               1883
#define _server                 "things.ubidots.com"
#define MAX_VALUES              3
#define PAYLOAD_SIZE            1000
#define FIRST_PART_TOPIC        "/v1.6/devices/"
#define _clientName             "Patient"
#define STASSID                 "test"
#define STAPSK                  "qwertyuiop"

WiFiClient espClient;
PubSubClient _client = PubSubClient(espClient); 

// PulseOximeter is the higher level interface to the sensor
// it offers:
//  * beat detection reporting
//  * heart rate calculation
//  * SpO2 (oxidation level) calculation
PulseOximeter pox;

uint32_t  tsLastReport = 0;
int       bpm = 0,SpO2 = 0;
float     temp=0;
uint8_t   currentValue=0, led_state=0;

typedef struct Value {
    char  *_variableLabel;
    float _value;
    char *_context;
    char *_timestamp;
} Value;
Value* val;


/****************************************
 * Auxiliar Functions
 ****************************************/
void WiFi_Connect()
{
    val = (Value *)malloc(MAX_VALUES*sizeof(Value));
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN,LOW);
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(STASSID);
    WiFi.setAutoConnect (true);
    WiFi.setAutoReconnect (true);
    WiFi.begin(STASSID, STAPSK);
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED_BUILTIN,!HIGH); 
}
void client_connect()
{
  while(!_client.connected())
  {
    if (!_client.connected()) 
    {
      Serial.print("Attempting MQTT connection...");
      if (_client.connect(_clientName, TOKEN, NULL)) 
      {
        Serial.println("connected");
      }
      else 
      {
        Serial.print("failed, rc=");
        Serial.print(_client.state());
        Serial.println(" try again in 3 seconds");
        delay(3000);
      }
    }
  }
}
void init_oximeter()
{
     // Initialize the PulseOximeter instance
    Serial.print("HeartBeat Sensor Initialization ");
    while(true)
    {
      if (!pox.begin()) 
      {
        Serial.println("FAILED");
        Serial.println("Restarting");
        delay(500);
        ESP.reset();
      } 
      else 
      {
        Serial.println("SUCCESS");
      }
      break;
    }
}
void add_val(char* variableLabel, float value) 
{
    (val+currentValue)->_variableLabel = variableLabel;
    (val+currentValue)->_value = value;
    (val+currentValue)->_context = NULL;
    (val+currentValue)->_timestamp = NULL;
    currentValue++;
    
}

bool Publish_val(char *deviceLabel) 
{
    char topic[150];
    char payload[PAYLOAD_SIZE];
    String str; int i=0;
    sprintf(topic, "%s%s", FIRST_PART_TOPIC, deviceLabel);
    sprintf(payload, "{");
    for (i = 0; i <= 2; i++) 
    {
        str = String((val+i)->_value, 2);
        sprintf(payload, "%s\"%s\": [{\"value\": %s", payload, (val+i)->_variableLabel, str.c_str());
        if (i >= 2) 
        {
            sprintf(payload, "%s}]}", payload);
            break;
        } 
        else 
        {
            sprintf(payload, "%s}], ", payload);
        }
    }
    if (!true)
    {
        Serial.println("publishing to TOPIC: ");
        Serial.println(topic);
        Serial.print("JSON dict: ");
        Serial.println(payload);
    }
    currentValue = 0;
    return _client.publish(topic, payload, 512);
}
float get_temperature()
{
  int analogValue = analogRead(A0);
  float millivolts = (analogValue/1024.0) * 3300; //3300 is the voltage provided by NodeMCU
  return (millivolts/10); //Temperature in milivolts
}
void callback(char* topic, byte* payload, unsigned int length) 
{
  // Nothing to do
}
/****************************************
 * Main Functions
 ****************************************/
void setup()
{
    Serial.begin(115200);
    WiFi_Connect();
    
    _client.setServer(_server, MQTT_PORT);
    _client.setCallback(callback);
    client_connect();
    delay(500);
    init_oximeter();
}
int count = 0;
void loop()
{

  if(WiFi.status() != WL_CONNECTED)
  {
    WiFi_Connect();
    if(WiFi.status() == WL_CONNECTED) 
    {
      client_connect();
      delay(500);
      init_oximeter();
    }
  }

  digitalWrite(LED_BUILTIN,led_state); 

    // Make sure to call update as fast as possible
    pox.update();
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) 
    {
        bpm = pox.getHeartRate();
        SpO2 = pox.getSpO2();
        temp = get_temperature();
        Serial.print("Heart rate:");
        Serial.print(pox.getHeartRate());
        Serial.print("  bpm / SpO2:");
        Serial.print(pox.getSpO2());
        Serial.print("  Body Temperature: ");
        Serial.print(temp);
        Serial.println(" °C");
        add_val("BPM",bpm);
        add_val("Temp",temp);
        add_val("SpO2",SpO2);
        Publish_val("patient_vitals");
        tsLastReport = millis();
        led_state = !led_state;
    }
}
