// This #include statement was automatically added by the Particle IDE.
#include "Seeed_HM330X.h"

// This #include statement was automatically added by the Particle IDE.
#include <HttpClient.h>


// This #include statement was automatically added by the Particle IDE.
#include "Seeed_HM330X.h"

#include "application.h"  


// This #include statement was automatically added by the Particle IDE.
/*
 * Project: TFM Allussà
 * Description: Sketch d'integració del sensor HM3301 amb particle Boron i predicció anomalies 
 * Author: Antoni Llussà
 * Date: 19/12/2020
 */


#define SERIAL Serial

HM330X sensor;
u8 buf[30];
String value = "0";

const int sensorBuffer = 3; //PM25
const int numSamples = 6;
int samplesRead = 0;
String telemetry = "[";

HttpClient http;  
http_header_t headers[] = {  
{ "Content-Type", "application/json" },  
{ NULL, NULL }   
};  
http_request_t request;  
http_response_t response;  


err_t parse_results(u8 *data, u8 pm)
{
  err_t NO_ERROR;
  if (NULL == data)
  {
    char error[255] = "HM330X read result failed!!!";
    Particle.publish("errorSensor", error, PRIVATE);
    return ERROR_PARAM;
  }
  return (u16)data[pm * 2] << 8 | data[pm * 2 + 1];
}

float pm25_measurement(int sense)
{

  if (sensor.read_sensor_value(buf, 29))
  {
    char error[255] = "HM330X read result failed!!!";
    Particle.publish("errorSensor", error, PRIVATE);
    Serial.println(F("Error al leer del sensor!"));
  }

  if (sense == 2)
    return parse_results(buf, 2); // PM1.0 Std
  else if (sense == 3)
    return parse_results(buf, 3); // PM2.5 Std
  else if (sense == 4)
    return parse_results(buf, 4); // PM10 Std
  else if (sense == 5)
    return parse_results(buf, 5); // PM1.0 Atm
  else if (sense == 6)
    return parse_results(buf, 6); // PM2.5 Atm
  else if (sense == 7)
    return parse_results(buf, 7); // PM10 Atm
  else if (sense == 8)
    return parse_results(buf, 8); // 0.3um
  else if (sense == 9)
    return parse_results(buf, 9); // 0.5um
  else if (sense == 10)
    return parse_results(buf, 10); // 1.0um
  else if (sense == 11)
    return parse_results(buf, 11); // 2.5um
  else if (sense == 12)
    return parse_results(buf, 12); // 5.0um
  else
    return parse_results(buf, 13); // 10.0um
}


void printResponse(http_response_t &response) {  
  //Particle.publish("HTTP Response: "); 
  //String status = " "+response.status;
  //Particle.publish("status",status);  
  Particle.publish("response body",response.body);
} 

void postRequestNeuralNetwork(String telemetry, String model) {  
  request.path = "/post/NeuralNetworkApp.html";  
  request.body = "{\"telemetry\":"+telemetry+", \"model\":\""+model+"\" }";  
  Particle.publish("model",model);  
  Particle.publish("body",request.body);    
  http.post(request, response, headers);  
  printResponse(response);  
} 

void postRequestIsolationForest(String telemetry) {  
   
  request.path = "/post/IsolationForestApp.html";  
  request.body = "{\"telemetry\":"+telemetry+"}";  
  Particle.publish("model","isolationForest");  
  Particle.publish("body",request.body);    
  http.post(request, response, headers);  
  printResponse(response);  
}  


void setup()
{
  Particle.keepAlive(120);

  Particle.variable("value", value);
  
  SERIAL.begin(115200);
  delay(100);
  SERIAL.println("Serial start");

  if (sensor.init())
  {
    SERIAL.println("HM330X init failed!!!");
    while (1)
      ;
  }
  
  request.ip = IPAddress(217,69,5,141);  
  request.port = 8000;   
}


void loop()
{
   if (Particle.connected() == false) {
     Particle.connect();
   }  
    
    
   float val = pm25_measurement(sensorBuffer);
   String time = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL);
   Particle.publish("Time",time);
   String json = "{\"Time\":\""+time+"\",\"PM25\":"+val+"},";

   telemetry = telemetry+json;
   samplesRead++;
   
   if(samplesRead == numSamples){

    //si tenim 6 dades apunt
    int len = telemetry.length()-1;
    telemetry = telemetry.substring(0,len);
    telemetry = telemetry+"]";
    postRequestNeuralNetwork(telemetry, "LSTM");
    postRequestNeuralNetwork(telemetry, "GRU");
    postRequestNeuralNetwork(telemetry, "DNN");
    postRequestIsolationForest(telemetry);
    telemetry = "[";
    samplesRead = 0;

    
  }
  
  delay(600000);
  
}