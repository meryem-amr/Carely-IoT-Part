#include <Arduino.h>
#include <WiFi.h>             
#include <WiFiClientSecure.h> 
#include <WiFiManager.h>
#include <PubSubClient.h>   
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "Audio.h"            
#include <driver/i2s.h>


// ====== MQTT CONFIG ======
const char* mqtt_server = "057d262208654037bedcba7215e7f6aa.s1.eu.hivemq.cloud"; 
const int   mqtt_port = 8883; 
const char* mqtt_user = "carely_cluster";
const char* mqtt_pass = "Carely236@";

// ====== BACKEND CONFIG ======
const char* backend_url = "https://carely.runasp.net/api/CryDetection/audio/chunk?babyId=3";

const char* local_pc_url = "http://192.168.1.7:5000/upload";

// ====== PIN CONFIG: SPEAKER (I2S_NUM_0) ======
#define I2S_BCLK      4
#define I2S_LRC       5
#define I2S_DOUT      6

// ====== PIN CONFIG: MICROPHONE (I2S_NUM_1) ======
#define I2S_WS        11
#define I2S_SD        10
#define I2S_SCK       12
#define I2S_MIC_PORT  I2S_NUM_1 

// ====== AUDIO & MIC CONFIG ======
#define SAMPLE_RATE 16000
#define RECORD_TIME 6  
#define NUM_CHANNELS 1
#define BITS_PER_SAMPLE 16

#define AUDIO_DATA_SIZE (SAMPLE_RATE * NUM_CHANNELS * (BITS_PER_SAMPLE / 8) * RECORD_TIME)
#define WAV_HEADER_SIZE 44
#define TOTAL_BUFFER_SIZE (WAV_HEADER_SIZE + AUDIO_DATA_SIZE)

// ====== GLOBALS ======
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Audio audio;

// Double Buffering & Task Variables
uint8_t* wavBufferA; 
uint8_t* wavBufferB;
uint8_t* currentWriteBuffer; 
uint8_t* currentSendBuffer;  

volatile bool uploadReady = false; 
TaskHandle_t uploadTaskHandle;

bool isRecording = false;
uint32_t recordedSamples = 0;
uint32_t totalSamplesNeeded = SAMPLE_RATE * RECORD_TIME;

unsigned long lastStatusUpdate = 0;

// ====== FUNCTION DECLARATIONS ======
void i2s_mic_install();
void i2s_mic_setpin();
void generateWavHeader(uint8_t* header, uint32_t wavSize, uint32_t sampleRate);
void handleMicrophoneRecording();
void uploadTask(void *pvParameters);

// ====== MQTT CALLBACK ======
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) return;

  String command = doc["command"]; 
  command.toUpperCase();

  // Speaker Commands
  if (command == "PLAY") {
    isRecording = false; 
    recordedSamples = 0;
    String urlToPlay = doc["url"]; 
    uint32_t startSeconds = doc["resume_seconds"] | 0; 
    
    if (doc.containsKey("level")) {
      int newVolume = constrain((int)doc["level"], 0, 21);
      audio.setVolume(newVolume);
    }
    audio.stopSong(); 
    audio.connecttohost(urlToPlay.c_str()); 
    
    if (startSeconds > 0) {
        audio.setAudioPlayPosition(startSeconds); 
    }
  }
  else if (command == "STOP") {
    audio.stopSong();
  }
  else if (command == "VOLUME") {
    int newVolume = constrain((int)doc["level"], 0, 21);
    audio.setVolume(newVolume); 
  }
  // Microphone Commands
  else if (command == "START_RECORDING") {
    audio.stopSong();
    isRecording = true;
    recordedSamples = 0; 
    Serial.println("🎤 Recording command received. Starting 6-second recording...");
  }
  else if (command == "STOP_RECORDING") {
    isRecording = false;
    recordedSamples = 0;
    Serial.println("⏹ Recording stopped by backend command.");
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.println("🔌 Connecting to MQTT...");
    String clientId = "Carely-Device-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("✅ MQTT Connected!");
      mqttClient.subscribe("carely/device/audio/command");
      mqttClient.subscribe("carely/device/mic/command"); 
    } else {
      Serial.print("❌ MQTT Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}



// ====== BACKGROUND UPLOAD TASK ======
void uploadTask(void *pvParameters) {
  for (;;) {
    if (uploadReady) {
      if (WiFi.status() == WL_CONNECTED) {
        
        WiFiClientSecure uploadClient;
        uploadClient.setInsecure(); 
        
        HTTPClient http;
        http.begin(uploadClient, backend_url);

        String boundary = "----CarelyBoundary123";
        String head = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"audio\"; filename=\"recording.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
        String tail = "\r\n--" + boundary + "--\r\n";

        uint32_t payloadSize = head.length() + TOTAL_BUFFER_SIZE + tail.length();

        uint8_t* payload = (uint8_t*) heap_caps_malloc(payloadSize, MALLOC_CAP_SPIRAM);

        if (payload != NULL) {
          memcpy(payload, head.c_str(), head.length());
          memcpy(payload + head.length(), currentSendBuffer, TOTAL_BUFFER_SIZE);
          memcpy(payload + head.length() + TOTAL_BUFFER_SIZE, tail.c_str(), tail.length());

          http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

          Serial.println("🌐 Background Task: Uploading public multipart audio...");
          
          int httpResponseCode = http.POST(payload, payloadSize);

          if (httpResponseCode > 0) {
            Serial.print("✅ Sent to Carely API. Response: ");
            Serial.println(httpResponseCode);
            
            String responseBody = http.getString();
            Serial.println("📥 Server said: " + responseBody);
          } else {
            Serial.print("❌ HTTP error: ");
            Serial.println(http.errorToString(httpResponseCode).c_str());
          }

          http.end();

          Serial.println("💻 Background Task: Saving copy to Local PC...");
          
          WiFiClient localClient;
          HTTPClient localHttp;
          localHttp.begin(localClient, local_pc_url);
          localHttp.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

          int localResponse = localHttp.POST(payload, payloadSize);

          if (localResponse > 0) {
            Serial.print("✅ Saved to PC. Response: ");
            Serial.println(localResponse);
          } else {
            Serial.print("❌ PC HTTP error: ");
            Serial.println(localHttp.errorToString(localResponse).c_str());
          }
          localHttp.end();

          free(payload); 
        } else {
          Serial.println("❌ Failed to allocate PSRAM for multipart payload.");
        }
        
      } else {
        Serial.println("❌ Background Task: WiFi disconnected, cannot upload.");
      }
      
      uploadReady = false; 
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

TaskHandle_t audioTaskHandle;

void audioTask(void *pvParameters) {
  for (;;) {
    audio.loop();
    vTaskDelay(pdMS_TO_TICKS(1));
}





void setup() {
  Serial.begin(115200);
  
  delay(4000); 
  Serial.println("\n\n--- ESP32 DEVICE BOOTING ---");

  // Allocate two buffers in PSRAM
  wavBufferA = (uint8_t*) heap_caps_malloc(TOTAL_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
  wavBufferB = (uint8_t*) heap_caps_malloc(TOTAL_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
  
  if (wavBufferA == NULL || wavBufferB == NULL) {
    Serial.println("❌ Failed to allocate PSRAM buffers! Check Tools > PSRAM settings.");
    while (true) {
        delay(100);
    }
  }
  
  generateWavHeader(wavBufferA, TOTAL_BUFFER_SIZE, SAMPLE_RATE);
  generateWavHeader(wavBufferB, TOTAL_BUFFER_SIZE, SAMPLE_RATE);

  currentWriteBuffer = wavBufferA; 

  xTaskCreatePinnedToCore(
    uploadTask,           
    "UploadTask",         
    16384,                
    NULL,                 
    1,                    
    &uploadTaskHandle,    
    0                     
  );

  WiFiManager wm;
  
  Serial.println("📡 Starting WiFi setup...");
  bool res = wm.autoConnect("Carely-Access-Point"); 

  if(!res) {
    Serial.println("❌ Failed to connect to WiFi!");
  } 
  else {
    Serial.println("\n✅ WiFi Connected!");
    WiFi.setSleep(false); 
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  // Speaker Setup 
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21);
  audio.setConnectionTimeout(2000, 7200); 

  // Microphone Setup 
  i2s_mic_install();
  i2s_mic_setpin();
  i2s_start(I2S_MIC_PORT);

  // MQTT Configuration
  espClient.setInsecure();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);


  xTaskCreatePinnedToCore(
    audioTask,           
    "AudioTask",         
    10240,              
    NULL,                
    3,                 
    &audioTaskHandle,    
    tskNO_AFFINITY     
  );
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();


  if (isRecording) {
    handleMicrophoneRecording();
  }

  // Progress Reporting
  if (millis() - lastStatusUpdate > 5000) { 
    if (audio.isRunning()) {
      uint32_t currentSec = audio.getAudioCurrentTime();
      
      char timeBuf[10];
      sprintf(timeBuf, "%02d:%02d:%02d", (currentSec/3600), (currentSec%3600)/60, (currentSec%60));

      StaticJsonDocument<200> statusDoc;
      statusDoc["status"] = "PLAYING";
      statusDoc["current_position"] = timeBuf;
      // statusDoc["deviceId"] = "carely_esp32_01"; 

      char buffer[256];
      serializeJson(statusDoc, buffer);
      
    }
    lastStatusUpdate = millis();
  }
}

// ====== MICROPHONE LOGIC ======
void handleMicrophoneRecording() {
  if (recordedSamples >= totalSamplesNeeded) return;

  size_t bytesRead = 0;
  int32_t rawSample[128]; 
  
  esp_err_t result = i2s_read(I2S_MIC_PORT, &rawSample, sizeof(rawSample), &bytesRead, 0); 

  if (result == ESP_OK && bytesRead > 0) {
    int samplesRead = bytesRead / sizeof(int32_t);
    int16_t* audioData = (int16_t*)(currentWriteBuffer + WAV_HEADER_SIZE);

    for (int i = 0; i < samplesRead; i++) {
      if (recordedSamples < totalSamplesNeeded) {
        audioData[recordedSamples] = (int16_t)(rawSample[i] >> 11);
        recordedSamples++;
      }
    }

    if (recordedSamples >= totalSamplesNeeded) {
      if (uploadReady) {
        Serial.println("⚠️ Warning: Previous upload not finished! Dropping new audio chunk.");
      } else {
        Serial.println("✅ 6 seconds recorded. Swapping buffers and triggering upload...");
        
        currentSendBuffer = currentWriteBuffer;
        currentWriteBuffer = (currentWriteBuffer == wavBufferA) ? wavBufferB : wavBufferA;
        
        uploadReady = true; 
      }
      recordedSamples = 0; 
    }
  }
}

// ====== I2S & WAV HEADER CONFIG ======
void i2s_mic_install() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = true
  };
  i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, NULL);
}

void i2s_mic_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_SCK,
    .ws_io_num    = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_SD
  };
  i2s_set_pin(I2S_MIC_PORT, &pin_config);
}

void audio_info(const char *info){
    Serial.print("AUDIO INFO: "); Serial.println(info);
}

void generateWavHeader(uint8_t* header, uint32_t wavSize, uint32_t sampleRate) {
  uint32_t byteRate = sampleRate * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  uint32_t chunkSize = wavSize - 8;
  header[4] = (uint8_t)(chunkSize & 0xff); header[5] = (uint8_t)((chunkSize >> 8) & 0xff);
  header[6] = (uint8_t)((chunkSize >> 16) & 0xff); header[7] = (uint8_t)((chunkSize >> 24) & 0xff);
  header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
  header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
  header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
  header[20] = 1; header[21] = 0;
  header[22] = (uint8_t)NUM_CHANNELS; header[23] = 0;
  header[24] = (uint8_t)(sampleRate & 0xff); header[25] = (uint8_t)((sampleRate >> 8) & 0xff);
  header[26] = (uint8_t)((sampleRate >> 16) & 0xff); header[27] = (uint8_t)((sampleRate >> 24) & 0xff);
  header[28] = (uint8_t)(byteRate & 0xff); header[29] = (uint8_t)((byteRate >> 8) & 0xff);
  header[30] = (uint8_t)((byteRate >> 16) & 0xff); header[31] = (uint8_t)((byteRate >> 24) & 0xff);
  header[32] = (uint8_t)(NUM_CHANNELS * BITS_PER_SAMPLE / 8); header[33] = 0;
  header[34] = 16; header[35] = 0;
  header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
  uint32_t dataSize = wavSize - 44;
  header[40] = (uint8_t)(dataSize & 0xff); header[41] = (uint8_t)((dataSize >> 8) & 0xff);
  header[42] = (uint8_t)((dataSize >> 16) & 0xff); header[43] = (uint8_t)((dataSize >> 24) & 0xff);
}