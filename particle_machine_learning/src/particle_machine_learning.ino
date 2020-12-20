/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "c:/Users/allussa/Documents/Particle/tfm_allussa/src/tfm_allussa.ino"
/*
 * Project tfm_allussa
 * Description: Sketch d'enviament de dades del sensor HM3301 amb particle Boron cap a un Model Machine Learning TFLite
 * Author: Antoni Llussà
 * Date: 14/12/2020
 */

// This #include statement was automatically added by the Particle IDE.
#include "../lib/Seeed_HM330X.h"


#include "../lib/TensorFlowLite/src/tensorflow/lite/experimental/micro/kernels/all_ops_resolver.h"
#include "../lib/TensorFlowLite/src/tensorflow/lite/experimental/micro/micro_error_reporter.h"
#include "../lib/TensorFlowLite/src/tensorflow/lite/experimental/micro/micro_interpreter.h"
#include "../lib/TensorFlowLite/src/tensorflow/lite/schema/schema_generated.h"
#include "../lib/TensorFlowLite/src/tensorflow/lite/version.h"
#include "model/lstm_model.cpp"

void setup();
void loop();
#line 15 "c:/Users/allussa/Documents/Particle/tfm_allussa/src/tfm_allussa.ino"
namespace
{
tflite::ErrorReporter *error_reporter = nullptr;
const tflite::Model *model = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

const int sensorBuffer = 3; //PM25
const int numSamples = 6;
int samplesRead = 0;

#define SERIAL Serial

HM330X sensor;
u8 buf[30];
String value = "0";

// Create an area of memory to use for input, output, and intermediate arrays.
// Finding the minimum value for your model may require some trial and error.
constexpr int kTensorArenaSize = 2 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
} // namespace

// setup() runs once, when the device is first turned on.
void setup() {
  // Put initialization like pinMode and begin functions here.

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

  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  model = tflite::GetModel(converted_model_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION)
  {
    error_reporter->Report(
        "Model provided is schema version %d not equal "
        "to supported version %d.",
        model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // This pulls in all the operation implementations we need.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::ops::micro::AllOpsResolver resolver;

  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk)
  {
    error_reporter->Report("AllocateTensors() failed");
    return;
  }

  // Obtain pointers to the model's input and output tensors.
  input = interpreter->input(0);
  output = interpreter->output(0);
}

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

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  // The core of your code will likely live here.

  //recollir 6 dades de sensor (cada 10 minuts)
  
  while(samplesRead < numSamples){
    if (Particle.connected() == false) {
      Particle.connect();
    }
    //estandaritzar les dades?
    int val = pm25_measurement(sensorBuffer);
    //construir matriu per entrada de model
    input->data.f[samplesRead] = val;
    samplesRead++;
    //si tenim 6 dades apunt
    if(samplesRead == numSamples)
    {
      //invocar el model
      TfLiteStatus invoke_status = interpreter->Invoke();
      if (invoke_status != kTfLiteOk)
      {
        Serial.println("Invoke failed!");
        while(1);
        return;
      }

      //recollir la predicció
      // Loop through the output tensor values from the model
      for (int i = 0; i < numSamples; i++) {
        Serial.print(numSamples);
        Serial.print(": ");
        Serial.println(output->data.f[i], 6);
      }
      
      //calcular l'rmse-loss de la predicció
      //calcular el threshold
      //comprovar si és anomalia (loss > threshold)
      //guardar els resultats en una fulla de càlcul
     

    }
    delay(600000);
  }
    
}