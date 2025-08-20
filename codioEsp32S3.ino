#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// -------------------- CONFIG WiFi --------------------
const char* WIFI_SSID = "LASER";
const char* WIFI_PASSWORD = "raul1975";

// -------------------- CONFIG Firebase --------------------
const char* API_KEY = "AIzaSyD9hEtglkx9Mp3YeGx7cRWvHyu_SWNpWvw";
const char* DATABASE_URL = "https://paneles-edificio-default-rtdb.firebaseio.com";
const char* USER_EMAIL = "pedro@pesantez.com";
const char* USER_PASSWORD = "123456";

// Objetos Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// -------------------- PINES --------------------
const int CSalidaPin01 = 5;
const int VBateriaPin01 = 6;
const int VPaneles01 = 7;
const int VSalida01 = 15;

// Variables globales
int mCSalida = 0;
int mVBateria = 0;
int mVPaneles = 0;
int mVSalida = 0;

// -------------------- TAREA Firebase --------------------
void enviarAFirebase(void *parameter) {
  for (;;) {
    if (Firebase.ready()) {
      FirebaseJson json;
      json.set("CSalida", mCSalida);
      json.set("VBateria", mVBateria);
      json.set("VPaneles", mVPaneles);
      json.set("VSalida", mVSalida);

      if (!Firebase.RTDB.setJSON(&fbdo, "/paneles/panel01", &json)) {
        Serial.println("Error al subir JSON: " + fbdo.errorReason());
      } else {
        Serial.println("Datos enviados a Firebase ✅");
      }
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);  // cada 2 segundos
  }
}

// -------------------- WiFi --------------------
void setup_WIFI() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Conectado con IP: ");
  Serial.println(WiFi.localIP());
}

// -------------------- Firebase --------------------
void setupFirebase() {
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectNetwork(true);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);

  setup_WIFI();
  setupFirebase();

  // Crear tarea secundaria para Firebase
  xTaskCreatePinnedToCore(
    enviarAFirebase,    // función de la tarea
    "FirebaseTask",     // nombre
    10000,              // stack size
    NULL,               // parámetro
    1,                  // prioridad
    NULL,               // handle
    1                   // núcleo del ESP32
  );
}

// -------------------- LOOP --------------------
void loop() {
  // Leer sensores y mapear a 0-300
  int valCSalida = analogRead(CSalidaPin01);
  int valVBateria = analogRead(VBateriaPin01);
  int valVPaneles = analogRead(VPaneles01);
  int valVSalida = analogRead(VSalida01);

  mCSalida = map(valCSalida, 0, 4095, 0, 300);
  mVBateria = map(valVBateria, 0, 4095, 0, 260);
  mVPaneles = map(valVPaneles, 0, 4095, 0, 300);
  mVSalida = map(valVSalida, 0, 4095, 0, 300);

  // Mostrar en serial
  Serial.printf("CSalida: %d | VBateria: %d | VPaneles: %d | VSalida: %d\n",
                mCSalida, mVBateria, mVPaneles, mVSalida);

  delay(200);  // lectura rápida
}
