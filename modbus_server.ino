#define COUNT_LOW 1500
#define COUNT_HIGH 8500
#define TIMER_WIDTH 16
#include "esp32-hal-ledc.h"

#include <M5StickCPlus.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

#ifdef ESP8266
 #include <ESP8266WiFi.h>
#else //ESP32
 #include <WiFi.h>
#endif
#include <ModbusIP_ESP8266.h>

#define SERVO_PIN 26

const int buttonA = 37;  //Button-B =39, Button-A = 37
const int buttonB = 39;

int connectweb = 0;
int last_valueA = 0;
int last_valueB = 0;
int cur_valueA = 0;
int cur_valueB = 0;

float temp = 0.00;

char MQTTServerip[] = "192.168.23.1";
String HTTPServerip = "192.168.23.1";

WiFiClient espclient;
PubSubClient client(espclient);   

HTTPClient http;

//Modbus IP Object
ModbusIP mb;                  

// Modbus Registers Offsets
const int HREG_SERVO1 = 400;      
//const int IREG_TEMP = 300;           
//const int CREG_PUMP = 10;                
const int ISREG = 100;        

const char* ssid = "SCSC_IOT09";
const char* password = "SCSC_HOME_IOT09";
const char* devname = "miotdev091";

int sendtempdata = 0;
char payloaddata[40]="";

int templevel;
int templevel_old;  

int pump_run;
int pump_run_old;     // ISREG value

int pos;
int pos_old;          // HREG value

void setup() {
  // init lcd
  M5.begin();
  M5.IMU.Init();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(40, 15);
  M5.Lcd.println("Temperature MQTT");
  pinMode(buttonA, INPUT);
  pinMode(buttonB, INPUT);

  pump_run = 0;
  pump_run_old = 0;

  templevel = 40;
  templevel_old = 40;

  pos=0;
  pos_old=0;

  mb.addHreg(HREG_SERVO1, pos);
  //mb.addIreg(IREG_TEMP, templevel);
  //mb.addCoil(CREG_PUMP, pump_run, 2);
  mb.addIsts(ISREG, pump_run);
  ledcSetup(1, 50, TIMER_WIDTH);
  ledcAttachPin(SERVO_PIN, 1);
  ledcWrite(1, 0);
  
  //DisplayCurrentValues();
}

void loop() {
  M5.IMU.getTempData(&temp);
  cur_valueA = digitalRead(buttonA);// read the value of BUTTON_A
  cur_valueB = digitalRead(buttonB);// read the value of BUTTON_B
  if(cur_valueA==0){
    Serial.println("Button A Status: pressed");
    Serial.println("Connecting to Wifi-Network");
    connectToNetwork();
  }
  else{
    Serial.println("Button A Status: released");
  }
  if(cur_valueB==0){
   sendtempdata=1;
  }
  else{
    Serial.println("Button B Status: released");
  }
  Serial.print("Temperature : ");
  Serial.println(temp);
  M5.Lcd.setCursor(30, 95);
  M5.Lcd.printf("Temperature : %.2f C", temp);

  templevel = (int)temp;    // obtain temp in int format
  
  if(connectweb == 1)
  {
    mb.task();
    Serial.println();

    
    if (templevel > 60 && !pump_run)       // >60
    {
      Serial.println("pump_run true!");
      pump_run = true;
      Pump_Activate(pump_run);
    }
    else if (templevel < 58 && pump_run)   // <58
    {
      Serial.println("pump_run false!");
      pump_run = false;
      Pump_Activate(pump_run);
    }
    String str3 = "Ists(ISREG): " + String(mb.Ists(ISREG)) + " HREG: " + String(mb.Hreg(HREG_SERVO1));
    Serial.println(str3);
    
    //Serial.println(pos);

    if (pos != mb.Hreg(HREG_SERVO1)) {

      // servo set to 'close' position
      if (mb.Hreg(HREG_SERVO1) == 1500) {
        for (pos = COUNT_HIGH; pos > COUNT_LOW; pos -= 100) { // goes from 180 degrees to 0 degrees
          ledcWrite(1, pos);                                  // tell servo to go to position in variable 'pos'
          Serial.println(pos);
          delay(15);                                          // waits 15ms for the servo to reach the position
        }
      }

      // servo set to 'open' position
      if (mb.Hreg(HREG_SERVO1) == 8500){
        for (pos = COUNT_LOW; pos < COUNT_HIGH; pos += 100) { // goes from 0 degrees to 180 degrees
          // in steps of 1 degree
          ledcWrite(1, pos);                                  // tell servo to go to position in variable 'pos'
          Serial.println(pos);
          delay(15);                                          // waits 15ms for the servo to reach the position
        }
      }
    }


    Serial.print("Publishing to test, helloworld");
    client.publish("test","hello world");
  }
  if(sendtempdata == 1)
  {
    char buffer[40];
    sprintf(buffer, "%.2f", temp);
    Serial.print("Publishing to test/temp1");
    client.publish("test/temp1",buffer);
    if (temp >=60.00){
      client.publish("test/sens1","Temperature Alert!");
    }
    else if (temp < 60.00){
      client.publish("test/sens1","Normal!");
    }
  }
  if (!client.connected() && connectweb == 1) {
    char strbuf[40];
    sprintf(strbuf,"%s",devname);
    if (client.connect(strbuf)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("test","hello world");
      // ... and resubscribe
      client.subscribe("teston");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      //Wait 3 seconds before retrying
      delay(3000);
    }
  }
  else {
    client.loop();
  }
  
  delay(300);
}

void Pump_Activate(bool pump_run) {
  bool data_has_changed1 = false;

  if (pump_run_old != pump_run) {
    data_has_changed1 = true;
    pump_run_old = pump_run;
  }

  if (data_has_changed1 && pump_run == 1) {
    mb.Ists(ISREG,true); 

  }
  else if (data_has_changed1 && pump_run == 0) {
    mb.Ists(ISREG,false); 

  }
  delay(1000);
}

void connectToNetwork() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Establishing connection to WiFi..");
  }
  connectweb = 1;
  Serial.println("Connected to network");
  Serial.println(WiFi.macAddress());
  Serial.println(WiFi.localIP());
  client.setServer(MQTTServerip, 1883);
  client.setCallback(callback);
  char strbuf[40];
  sprintf(strbuf,"%s",devname);
  client.connect(strbuf);
  client.subscribe("teston");
  client.publish("test","hello world");
  M5.Lcd.setCursor(30,40,2); 
  M5.Lcd.print(WiFi.localIP());// display the status
  sendhttpget();

  mb.server();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
    payloaddata[i]=payload[i];
  }
  M5.Lcd.setCursor(30, 115);
  M5.Lcd.printf("Payload : %s", payloaddata);
  Serial.println();
}

void sendhttpget(){
  String serverAPI = String("http://" + HTTPServerip + ":8000/devices");
  String serverPath = String(serverAPI + "/" + devname);
  http.begin(serverPath.c_str());
  
  // Send HTTP GET request
  int httpResponseCode = http.GET();
}
