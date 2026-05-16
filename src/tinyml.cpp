#include "tinyml.h"
#include "risk_label.h"
#include "serial_log.h"

#include <math.h>
#include "tensorflow/lite/core/api/error_reporter.h"

static int tensor_element_count(const TfLiteTensor *tensor)
{
  int n = 1;
  for (int i = 0; i < tensor->dims->size; ++i)
  {
    n *= tensor->dims->data[i];
  }
  return n;
}

static int argmax_float(const float *p, int n)
{
  int best = 0;
  for (int i = 1; i < n; ++i)
  {
    if (p[i] > p[best])
    {
      best = i;
    }
  }
  return best;
}

struct TinyMLRuntime
{
  tflite::MicroErrorReporter error_reporter;
  tflite::AllOpsResolver resolver;
  const tflite::Model *model = nullptr;
  tflite::MicroInterpreter *interpreter = nullptr;
  TfLiteTensor *input = nullptr;
  TfLiteTensor *output = nullptr;
  uint8_t *tensor_arena = nullptr;
  bool ready = false;
};

static void setupTinyML(SharedContext *ctx, TinyMLRuntime &rt)
{
  rt.ready = false;

  serialLogLock(ctx);
  Serial.println("TensorFlow Lite Init....");
  serialLogUnlock(ctx);

  rt.model = tflite::GetModel(dht_anomaly_model_tflite);
  if (rt.model->version() != TFLITE_SCHEMA_VERSION)
  {
    TF_LITE_REPORT_ERROR(&rt.error_reporter, "Model schema version %d != supported %d.",
                         rt.model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  constexpr int kTensorArenaSize = 16 * 1024;
  rt.tensor_arena = new uint8_t[kTensorArenaSize];
  rt.interpreter = new tflite::MicroInterpreter(rt.model, rt.resolver, rt.tensor_arena, kTensorArenaSize, &rt.error_reporter);

  if (rt.interpreter->AllocateTensors() != kTfLiteOk)
  {
    TF_LITE_REPORT_ERROR(&rt.error_reporter, "AllocateTensors() failed");
    return;
  }

  rt.input = rt.interpreter->input(0);
  rt.output = rt.interpreter->output(0);

  if (!rt.input || !rt.output || rt.input->type != kTfLiteFloat32 || rt.output->type != kTfLiteFloat32)
  {
    TF_LITE_REPORT_ERROR(&rt.error_reporter, "TinyML: expected float32 input/output");
    return;
  }
  if (tensor_element_count(rt.input) != 2 || tensor_element_count(rt.output) != 3)
  {
    TF_LITE_REPORT_ERROR(&rt.error_reporter, "TinyML: need input 2 floats, output 3 (softmax). Re-run ml/train_export.py");
    return;
  }

  rt.ready = true;
  serialLogLock(ctx);
  Serial.println("TensorFlow Lite Micro initialized on ESP32.");
  serialLogUnlock(ctx);
}

void tiny_ml_task(void *pvParameters)
{
  SharedContext *ctx = static_cast<SharedContext *>(pvParameters);
  if (!ctx)
  {
    serialLogLock(ctx);
    Serial.println("TinyML: SharedContext is null; task stopped.");
    serialLogUnlock(ctx);
    vTaskDelete(nullptr);
    return;
  }

  TinyMLRuntime rt;
  setupTinyML(ctx, rt);
  if (!rt.ready)
  {
    serialLogLock(ctx);
    Serial.println("TinyML: setup failed; task stopped.");
    serialLogUnlock(ctx);
    vTaskDelete(nullptr);
    return;
  }

  unsigned long inferences = 0;
  unsigned long correct = 0;

  while (1)
  {
    float temperature = 0.0f;
    float humidity = 0.0f;

    if (xSemaphoreTake(ctx->mutexContext, pdMS_TO_TICKS(2000)) == pdTRUE)
    {
      temperature = ctx->temperature;
      humidity = ctx->humidity;
      xSemaphoreGive(ctx->mutexContext);
    }

    if (isnan(temperature) || isnan(humidity) || temperature < 0.0f || humidity < 0.0f)
    {
      serialLogLock(ctx);
      Serial.println("TinyML: skip (invalid DHT reading)");
      serialLogUnlock(ctx);
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    rt.input->data.f[0] = temperature;
    rt.input->data.f[1] = humidity;

    const unsigned long t0 = millis();
    if (rt.interpreter->Invoke() != kTfLiteOk)
    {
      TF_LITE_REPORT_ERROR(&rt.error_reporter, "Invoke failed");
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    const unsigned long t1 = millis();

    const int nout = tensor_element_count(rt.output);
    const int best = argmax_float(rt.output->data.f, nout);
    const int predicted = best + 1; // model classes 0..2 -> labels 1..3
    const int expected = risk_final_label(temperature, humidity);

    if (predicted == expected)
      correct++;
    inferences++;

    serialLogLock(ctx);
    Serial.printf("TinyML T=%.1fC H=%.1f%% | rule=%d pred=%d | p=[%.2f,%.2f,%.2f] | %lums | roll_acc=%.1f%% (%lu/%lu)\n",
                  temperature, humidity, expected, predicted,
                  rt.output->data.f[0], rt.output->data.f[1], rt.output->data.f[2],
                  (unsigned long)(t1 - t0),
                  inferences ? (100.0f * (float)correct / (float)inferences) : 0.0f,
                  correct, inferences);
    serialLogUnlock(ctx);

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
