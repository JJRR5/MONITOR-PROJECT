#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "MAX30100_PulseOximeter.h"
#include <Adafruit_MLX90614.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_I2CDevice.h>


PulseOximeter pox;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char* ssid = "RamirezRodriguez_2.4Gnormal";
const char* password = "Juggerwicho1";
const char* mqtt_server = "192.168.100.168";
int port = 1883;

int heartRate;
int spo2;

#define width 128
#define height 64
#define oled_reset -1
#define oled_adress 0x3C

Adafruit_SSD1306 display(width, height, &Wire, oled_reset); 


#define ledTemp 14 //R
#define ledHeart 12  //G
#define ledSpo2 27  //B

#define I2C_SDA 21
#define I2C_SCL 22
#define REPORTING_PERIOD_MS 1000

uint32_t tsLastReport = 0;
uint8_t max30100_address = 0x57;
uint8_t irmlx90614_address = 0x5A;

//Functions 
void clean(){
    display.clearDisplay();
    display.display();
}

void oled(){
    clean();
    display.setTextSize(3);
    display.setCursor(0,0);
    display.println("MONITOR");
    display.setCursor(36,44);
    display.setTextSize(2);
    display.println("T-800");
    display.display();

}

void buzzer(){

}

void rgb(int led){
    if(led == ledTemp){ 
        digitalWrite(ledTemp, HIGH);
    }
    else if(led == -1){
        digitalWrite(ledTemp, LOW);
    }
    else if(led == ledHeart){
        digitalWrite(ledHeart, HIGH);
    }
    else if(led == -2){
        digitalWrite(ledHeart, LOW);
    }
    else if(led == ledSpo2){
        digitalWrite(ledSpo2, HIGH);
    }
    else if(led == -3){
        digitalWrite(ledSpo2, LOW);
    }
    else{
        digitalWrite(ledTemp, LOW);
        digitalWrite(ledHeart, LOW);
        digitalWrite(ledSpo2, LOW);
    }
}

void alarm(int temp,int heart, int spo2){
    int errorState = 0;
    char errorArray[8];
    dtostrf(errorState,1,2,errorArray);
    if(temp == 0){
        rgb(ledTemp);
        errorState = 1; //Temperature is not a number
        dtostrf(errorState,1,2,errorArray);
        mqttClient.publish("MONITOR/tempError",errorArray);
    }else if(temp < 35){
        rgb(ledTemp);
        errorState = 2; //Temperature is too low
        dtostrf(errorState,1,2,errorArray);
        mqttClient.publish("MONITOR/tempError",errorArray);
    }else if(temp > 37){
        rgb(ledTemp);
        errorState = 3; //Temperature is too high
        dtostrf(errorState,1,2,errorArray);
        mqttClient.publish("MONITOR/tempError",errorArray);
    }else{
        rgb(-1);
        errorState = 0;
        dtostrf(errorState,1,2,errorArray); //Normal state
        mqttClient.publish("MONITOR/tempError",errorArray);
    }
    // HEART//////////////////////////////////////////////
    if(heart == 0){
        rgb(ledHeart);
        errorState = 1; //Heart Rate Sensor disconnected
        dtostrf(errorState,1,2,errorArray);
        mqttClient.publish("MONITOR/heartError",errorArray);
    }else if(heart < 50){
        rgb(ledHeart);
        errorState = 2; //Warning!, Bradycardia
        dtostrf(errorState,1,2,errorArray);
        mqttClient.publish("MONITOR/heartError",errorArray);
    }else if (heart > 120){
        rgb(ledHeart);
        errorState = 3; //Warning!,Tachycardia
        dtostrf(errorState,1,2,errorArray);
        mqttClient.publish("MONITOR/heartError",errorArray);
    }else if(heart > 50 && heart < 120){
        rgb(-2);
        errorState = 0;
        dtostrf(errorState,1,2,errorArray); //Normal state
        mqttClient.publish("MONITOR/heartError",errorArray);
    }
    /////SPO2////////////////////////////////////////////
    if(spo2 == 0){
        rgb(ledHeart);
        errorState = 1; //SPO2 sensor disconnected
        dtostrf(errorState,1,2,errorArray);
        mqttClient.publish("MONITOR/spo2Error",errorArray);
    }else if(spo2 < 90){
        rgb(ledHeart);
        errorState = 2; //Warning!, Hypoxia
        dtostrf(errorState,1,2,errorArray);
        mqttClient.publish("MONITOR/spo2Error",errorArray);
    }else{
        rgb(-3);
        errorState = 0;
        dtostrf(errorState,1,2,errorArray); //Normal state
        mqttClient.publish("MONITOR/spo2Error",errorArray);
    }
}

void wifiInit() {
    Serial.println("Connencting to");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);  
    }
    Serial.println("");
    Serial.println("WiFi connected!!!!");
    Serial.println("Dirección IP: ");
    Serial.println(WiFi.localIP());
}

void startMAX30100() {
    pox.begin(); 
    Serial.println("Initializing pulse oximeter..");
    if (!pox.begin()) {
            Serial.println("MAX FAILED");
            // for(;;);
        } else {
            Serial.println("MAX SUCCESS");
    }
}

void mqttInit(){
    mqttClient.setServer(mqtt_server,port);
    mqttClient.connect("MONITOR CLIENT");
    Serial.println("MQTT State: ");
    Serial.println(mqttClient.state());
    if(mqttClient.state() == 0) {
        digitalWrite(2,HIGH);
    }else{
        digitalWrite(2,LOW);
        Serial.println("mqtt Error");
    }
}

void welcome(){
    int message = 0;
    char messageArray[8];
    dtostrf(message,1,2,messageArray);
    mqttClient.publish("MONITOR/welcome",messageArray);
}

void pubData(){
    pox.update();
    
    float temp = mlx.readObjectTempC();
    heartRate = pox.getHeartRate();
    spo2 = pox.getSpO2();

    char heartArray[8];
    dtostrf(heartRate,1,2,heartArray);

    char spo2Array[8];
    dtostrf(spo2,1,2,spo2Array);
    
    char tempArray[8];
    dtostrf(temp,1,2,tempArray);

    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        Serial.println("Heart rate:" + String(heartRate));
        Serial.println("SpO2:" + String(spo2) + "%");
        Serial.println("Temperature:" + String(temp));
        mqttClient.publish("MONITOR/heart",heartArray);
        mqttClient.publish("MONITOR/spo2",spo2Array);
        mqttClient.publish("MONITOR/temp",tempArray);
        alarm(temp,heartRate,spo2);
        tsLastReport = millis();
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(2,OUTPUT);
    pinMode(ledTemp,OUTPUT);
    pinMode(ledHeart,OUTPUT);
    pinMode(ledSpo2,OUTPUT);
    wifiInit();
    mlx.begin();
    Wire.begin();
    mqttInit();
    startMAX30100();
    welcome();
    if(!display.begin(SSD1306_SWITCHCAPVCC,oled_adress)){
    Serial.println(F("Failed to initialize display"));
    }
    oled();
}

void loop() {
    pubData();
}
