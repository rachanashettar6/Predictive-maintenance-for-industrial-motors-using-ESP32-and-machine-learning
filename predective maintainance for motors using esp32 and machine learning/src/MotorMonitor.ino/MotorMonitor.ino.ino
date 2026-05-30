#include <WiFi.h> 
#include <FirebaseESP32.h> 
#include <ArduinoJson.h> 
#include "model.h" 
#define WIFI_SSID      "Rachana Shettar " 
#define WIFI_PASSWORD  "ayoooooooo" 
#define FIREBASE_HOST  "motormonitor-f28c9-default-rtdb.firebaseio.com" 
#define FIREBASE_AUTH  "EThiGmb6Znt16DUtFzuYGCY0FAnPwByM59aQLbOG" 
#define LM35_PIN    34 
#define SW420_PIN   35 
#define IR_PIN      25 
FirebaseData   fbdo; 
FirebaseAuth   auth; 
FirebaseConfig config; 
Eloquent::ML::Port::RandomForest classifier; 
volatile int  pulseCount = 0; 
unsigned long lastRPMCheck = 0; 
float         currentRPM = 0; 
unsigned long lastSendTime = 0; 
#define SEND_INTERVAL 3000 
void IRAM_ATTR onIRPulse() { 
  pulseCount++; 
}
void setup() { 
  Serial.begin(115200); 
  Serial.println("Motor Monitor Starting..."); 
  pinMode(IR_PIN, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(IR_PIN), onIRPulse, RISING);  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); Serial.print("."); 
  } 
  Serial.println("\nWiFi Connected!");  
  config.host = FIREBASE_HOST; 
  config.signer.tokens.legacy_token = FIREBASE_AUTH; 
  Firebase.begin(&config, &auth); 
  Firebase.reconnectWiFi(true); 
  Serial.println("Firebase Connected!"); 
}
float readTemperature() { 
  int raw = analogRead(LM35_PIN); 
  float voltage = raw * (3.3 / 4095.0); 
  return voltage * 100.0;  // LM35: 10mV per degree C 
}  
float readVibrationRMS() { 
  int raw = analogRead(SW420_PIN); 
  return raw * (3.3 / 4095.0); 
}  
float calculateRPM() { 
  if (millis() - lastRPMCheck >= 1000) { 
    currentRPM = pulseCount * 60.0; 
    pulseCount = 0; 
    lastRPMCheck = millis(); 
  } 
  return currentRPM; 
}
String predictStatus(float vib_rms, float vib_std, 
                     float cur_rms, float cur_std, 
                     float rpm,    float temp) { 
  float features[6] = {vib_rms, vib_std, cur_rms, cur_std, rpm, temp}; 
  int result = classifier.predict(features); 
 
  // These numbers match what Colab printed as 'Label number mapping' 
  if (result == 0) return "NORMAL"; 
  if (result == 1) return "OVERCURRENT"; 
  if (result == 2) return "OVERHEAT"; 
  if (result == 3) return "STOPPED"; 
  if (result == 4) return "UNBALANCE"; 
  return "UNKNOWN"; 
}
  calculateRPM();  // Must run every loop for accurate RPM 
  if (millis() - lastSendTime >= SEND_INTERVAL) { 
    lastSendTime = millis(); 
    float temp    = readTemperature(); 
    float vib_rms = readVibrationRMS(); 
    float vib_std = 0.1;   // SW420 gives one value; use 0.1 as placeholder 
    float cur_rms = 0.22;  // You don't have current sensor; use placeholder 
    float cur_std = 0.12;  // Same - placeholder 
    float rpm     = currentRPM; 
    String status = predictStatus(vib_rms, vib_std, cur_rms, cur_std, rpm, temp); 
    Serial.println("------------------------------"); 
    Serial.printf("Temp      : %.2f C\n", temp); 
    Serial.printf("Vibration : %.3f\n", vib_rms); 
    Serial.printf("RPM       : %.0f\n", rpm); 
    Serial.printf("Status    : %s\n", status.c_str()); 
    if (Firebase.ready()) { 
      Firebase.setFloat(fbdo,  "/motor/temperature", temp); 
      Firebase.setFloat(fbdo,  "/motor/vibration",   vib_rms); 
      Firebase.setFloat(fbdo,  "/motor/rpm",         rpm); 
      Firebase.setString(fbdo, "/motor/status",      status); 
      Firebase.setInt(fbdo,    "/motor/timestamp",   millis()/1000); 
      Serial.println("Sent to Firebase OK"); 
    } 
  }
}