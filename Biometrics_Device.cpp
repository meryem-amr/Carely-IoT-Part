#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "ClosedCube_MAX30205.h"
#include <math.h>

// ================== OBJECTS ==================
Adafruit_MPU6050 mpu;
MAX30105 particleSensor;
ClosedCube_MAX30205 max30205;

void readMPU6050();
void readMAX30102();
void publishHeartRate();
void readMAX30205();
void publishTemperature();
void publishMotion();

// ================== WIFI / MQTT ==================
const char* ssid          = "Meryem";
const char* pass          = "jjjjjjjj";
const char* mqtt_server   = "e9f9d68104894cf1bee955e9d58d80ed.s1.eu.hivemq.cloud";
const char* mqtt_username = "esp32_device1";
const char* mqtt_password = "Device1biometric";
const int   mqtt_port     = 8883;

WiFiClientSecure espClient;
PubSubClient client(espClient);

static const char *root_ca PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// ================== CLINICAL THRESHOLDS ==================

#define HR_MIN_VALID      60
#define HR_MAX_VALID     220
#define HR_NO_CONTACT  30000L

#define TEMP_MIN_VALID   34.0f
#define TEMP_MAX_VALID   42.0f

#define MOTION_AXIS_MIN -20.0f
#define MOTION_AXIS_MAX  20.0f

// 30 seconds, = 60 samples at 500ms
#define STILLNESS_THRESHOLD  0.05f
#define APNEA_ALARM_SEC        30
#define APNEA_SAMPLES  ((APNEA_ALARM_SEC * 1000UL) / 500UL)   // = 60 samples

// ================== HEART RATE STATE ==================
// RATE_SIZE = 4: rolling average over last 4 beats (~2 seconds at 120 BPM)
const byte RATE_SIZE = 4;
byte  rates[RATE_SIZE];
byte  rateSpot       = 0;
long  lastBeat       = 0;
float beatsPerMinute = 0;
int   beatAvg        = 0;

// ================== SENSOR VALUES ==================
float accX = 0, accY = 0, accZ = 0;
float prevAccX = 0, prevAccY = 0, prevAccZ = 0;
float temperatureC = 0;

// ================== MOTION AVERAGING FOR SLEEP MODEL ==================
// accumulate 10 raw samples (10 × 500ms = 5s), publish the mean
#define MODEL_SAMPLES 10
float accX_buf[MODEL_SAMPLES];
float accY_buf[MODEL_SAMPLES];
float accZ_buf[MODEL_SAMPLES];
int   modelBufIdx = 0;

// ================== APNEA TRACKING ==================
unsigned long stillnessSamples = 0;
bool apneaAlertSent   = false;

// ================== TIMING ==================
unsigned long lastHRPub        = 0;
unsigned long lastTempPub      = 0;
unsigned long lastMotionSample = 0;

const unsigned long HR_INTERVAL      =  2000;
const unsigned long TEMP_INTERVAL    = 15000;
const unsigned long MOTION_SAMPLE_MS =   500;

// ================== WIFI ==================
void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected");
}

// ================== MQTT ==================
void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32-Baby-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      client.publish("baby/status", "online");
      Serial.println("MQTT connected");
    } else {
      Serial.print("MQTT failed rc="); Serial.println(client.state());
      delay(5000);
    }
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);
  Wire.setClock(100000);

  if (!mpu.begin()) { Serial.println("MPU6050 NOT FOUND"); while (1); }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 NOT FOUND"); while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x15);
  particleSensor.setPulseAmplitudeIR(0x20);
  particleSensor.setPulseAmplitudeGreen(0);

  max30205.begin(0x48);

  setup_wifi();
  espClient.setCACert(root_ca);
  client.setServer(mqtt_server, mqtt_port);
}

// ================== LOOP ==================
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();

  if (now - lastMotionSample >= MOTION_SAMPLE_MS) {
    lastMotionSample = now;
    readMPU6050();
  }

  readMAX30102();

  if (now - lastHRPub >= HR_INTERVAL) {
    lastHRPub = now;
    publishHeartRate();
  }

  if (now - lastTempPub >= TEMP_INTERVAL) {
    lastTempPub = now;
    readMAX30205();
    publishTemperature();
  }

}

// ================== MPU6050 ==================
void readMPU6050() {
  sensors_event_t a;
  mpu.getAccelerometerSensor()->getEvent(&a);

  float nx = a.acceleration.x;
  float ny = a.acceleration.y;
  float nz = a.acceleration.z;

  if (nx < MOTION_AXIS_MIN || nx > MOTION_AXIS_MAX ||
      ny < MOTION_AXIS_MIN || ny > MOTION_AXIS_MAX ||
      nz < MOTION_AXIS_MIN || nz > MOTION_AXIS_MAX) {
    Serial.println("Motion sample discarded (out of sensor range)");
    return;
  }

  prevAccX = accX; prevAccY = accY; prevAccZ = accZ;
  accX = nx; accY = ny; accZ = nz;

  // Apnea detection — uses raw 500ms samples
  float delta = abs(accX - prevAccX) +
                abs(accY - prevAccY) +
                abs(accZ - prevAccZ);

  if (delta < STILLNESS_THRESHOLD) {
    stillnessSamples++;
    if (stillnessSamples >= APNEA_SAMPLES && !apneaAlertSent) {
      client.publish("baby/alerts/apnea", "1");
      Serial.println("!!! APNEA ALERT: no movement for 30 seconds !!!");
      apneaAlertSent = true;
    }
  } else {
    stillnessSamples = 0;
    if (apneaAlertSent) {
      client.publish("baby/alerts/apnea", "0");
      Serial.println("Apnea alert cleared — movement resumed");
      apneaAlertSent = false;
    }
  }

  // 5-second averaging for sleep model
  accX_buf[modelBufIdx] = accX;
  accY_buf[modelBufIdx] = accY;
  accZ_buf[modelBufIdx] = accZ;
  modelBufIdx++;

  if (modelBufIdx >= MODEL_SAMPLES) {
    modelBufIdx = 0;
    publishMotion();
  }
}

// ================== MAX30205 ==================
void readMAX30205() {
  float t = max30205.readTemperature();

  if (t < TEMP_MIN_VALID || t > TEMP_MAX_VALID) {
    Serial.print("Temp discarded: "); Serial.println(t);
    temperatureC = 0;  
    return;
  }
  temperatureC = t;
  Serial.print("Temp: "); Serial.println(temperatureC);
}

// ================== MAX30102 ==================
void readMAX30102() {
  long irValue = particleSensor.getIR();

  if (irValue < HR_NO_CONTACT) {
    beatAvg        = 0;
    beatsPerMinute = 0;
    return;
  }

  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat   = millis();
    float bpm  = 60.0 / (delta / 1000.0);

    if (bpm >= HR_MIN_VALID && bpm <= HR_MAX_VALID) {
      beatsPerMinute    = bpm;
      rates[rateSpot++] = (byte)bpm;
      rateSpot         %= RATE_SIZE;
      beatAvg = 0;
      for (byte i = 0; i < RATE_SIZE; i++) beatAvg += rates[i];
      beatAvg /= RATE_SIZE;
    }
  }
}

// ================== PUBLISH: HEART RATE ==================
void publishHeartRate() {
  char buf[16];

  if (beatAvg == 0) {
    client.publish("baby/sensors/hr/contact", "0");
    return;
  }
  client.publish("baby/sensors/hr/contact", "1");

  itoa(beatAvg, buf, 10);
  client.publish("baby/sensors/hr/avg", buf);

  Serial.print("HR avg: "); Serial.println(beatAvg);
}

// ================== PUBLISH: TEMPERATURE ==================
void publishTemperature() {
  if (temperatureC == 0) return;

  char buf[16];
  dtostrf(temperatureC, 1, 2, buf);
  client.publish("baby/sensors/temperature", buf);

  Serial.print("Temp published: "); Serial.println(temperatureC);
}

// ================== PUBLISH: MOTION (5-second average) ==================
void publishMotion() {
  float mx = 0, my = 0, mz = 0;
  for (int i = 0; i < MODEL_SAMPLES; i++) {
    mx += accX_buf[i];
    my += accY_buf[i];
    mz += accZ_buf[i];
  }
  mx /= MODEL_SAMPLES;
  my /= MODEL_SAMPLES;
  mz /= MODEL_SAMPLES;

  char buf[16];
  dtostrf(mx, 1, 3, buf); client.publish("baby/sensors/motion/x", buf);
  dtostrf(my, 1, 3, buf); client.publish("baby/sensors/motion/y", buf);
  dtostrf(mz, 1, 3, buf); client.publish("baby/sensors/motion/z", buf);

  Serial.print("Motion avg  X:"); Serial.print(mx);
  Serial.print("  Y:"); Serial.print(my);
  Serial.print("  Z:"); Serial.println(mz);
}