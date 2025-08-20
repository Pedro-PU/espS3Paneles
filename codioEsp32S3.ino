#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// -------------------- CONFIG WiFi --------------------
const char* WIFI_SSID = "CNT_GPON_PESANTEZ";
const char* WIFI_PASSWORD = "Lampara2016";

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

const int CSalidaPin02 = 16;
const int VBateriaPin02 = 8;
const int VPaneles02 = 3;
const int VSalida02 = 9;

const int bomba01 = 38;
const int bomba02 = 37;
const int temp01 = 10;

const int bomba03 = 36;
const int bomba04 = 35;
const int temp02 = 11;

// -------------------- Variables globales Panel01 --------------------
int mCSalida01 = 0;
int mVBateria01 = 0;
int mVPaneles01 = 0;
int mVSalida01 = 0;

// -------------------- Variables globales Panel02 --------------------
int mCSalida02 = 0;
int mVBateria02 = 0;
int mVPaneles02 = 0;
int mVSalida02 = 0;

// -------------------- Variables globales Bombas y Temp --------------------
bool mBomba01 = false;
bool mBomba02 = false;
bool mBomba03 = false;
bool mBomba04 = false;
int mTemp01 = 0;
int mTemp02 = 0;

// -------------------- TAREA Firebase --------------------
void enviarAFirebase(void *parameter) {
  for (;;) {
    if (Firebase.ready()) {
      // -------- JSON Panel01 --------
      FirebaseJson json01;
      json01.set("CSalida", mCSalida01);
      json01.set("VBateria", mVBateria01);
      json01.set("VPaneles", mVPaneles01);
      json01.set("VSalida", mVSalida01);
      Firebase.RTDB.setJSON(&fbdo, "/paneles/panel01", &json01);

      // -------- JSON Panel02 --------
      FirebaseJson json02;
      json02.set("CSalida", mCSalida02);
      json02.set("VBateria", mVBateria02);
      json02.set("VPaneles", mVPaneles02);
      json02.set("VSalida", mVSalida02);
      Firebase.RTDB.setJSON(&fbdo, "/paneles/panel02", &json02);

      // -------- JSON Bombas Grupo01 --------
      FirebaseJson grupo01;
      grupo01.set("bomba01", mBomba01);
      grupo01.set("bomba02", mBomba02);
      grupo01.set("temp01", mTemp01);
      Firebase.RTDB.setJSON(&fbdo, "/bombas/grupo01", &grupo01);

      // -------- JSON Bombas Grupo02 --------
      FirebaseJson grupo02;
      grupo02.set("bomba03", mBomba03);
      grupo02.set("bomba04", mBomba04);
      grupo02.set("temp02", mTemp02);
      Firebase.RTDB.setJSON(&fbdo, "/bombas/grupo02", &grupo02);

      Serial.println("Datos de bombas y temperaturas enviados ✅");
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

  // Configuración pines de bombas como salidas
  pinMode(bomba01, OUTPUT);
  pinMode(bomba02, OUTPUT);
  pinMode(bomba03, OUTPUT);
  pinMode(bomba04, OUTPUT);

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

// --- Función para controlar máximo 2 bombas ---
void controlarBombas() {
  int activas = mBomba01 + mBomba02 + mBomba03 + mBomba04;  // cuenta bombas encendidas

  if (activas > 2) {
    // Si hay más de 2, apagamos la última que quedó activa
    if (mBomba04) { mBomba04 = false; }
    else if (mBomba03) { mBomba03 = false; }
    else if (mBomba02) { mBomba02 = false; }
    else if (mBomba01) { mBomba01 = false; }
  }

  // Aplicar estados a pines
  digitalWrite(bomba01, mBomba01);
  digitalWrite(bomba02, mBomba02);
  digitalWrite(bomba03, mBomba03);
  digitalWrite(bomba04, mBomba04);
}

void loop() {
  // -------- Lecturas Panel01 --------
  int valCSalida01 = analogRead(CSalidaPin01);
  int valVBateria01 = analogRead(VBateriaPin01);
  int valVPaneles01 = analogRead(VPaneles01);
  int valVSalida01 = analogRead(VSalida01);

  mCSalida01 = map(valCSalida01, 0, 4095, 0, 300);
  mVBateria01 = map(valVBateria01, 0, 4095, 0, 260);
  mVPaneles01 = map(valVPaneles01, 0, 4095, 0, 300);
  mVSalida01 = map(valVSalida01, 0, 4095, 0, 300);

  // -------- Lecturas Panel02 --------
  int valCSalida02 = analogRead(CSalidaPin02);
  int valVBateria02 = analogRead(VBateriaPin02);
  int valVPaneles02 = analogRead(VPaneles02);
  int valVSalida02 = analogRead(VSalida02);

  mCSalida02 = map(valCSalida02, 0, 4095, 0, 300);
  mVBateria02 = map(valVBateria02, 0, 4095, 0, 260);
  mVPaneles02 = map(valVPaneles02, 0, 4095, 0, 300);
  mVSalida02 = map(valVSalida02, 0, 4095, 0, 300);

  // -------- Lecturas Temperaturas --------
  mTemp01 = map(analogRead(temp01), 0, 4095, 0, 100);
  mTemp02 = map(analogRead(temp02), 0, 4095, 0, 100);

  // -------- Lógica Bombas (máx. 2 encendidas) --------
  controlarBombas();

  // -------- Serial Debug --------
  Serial.printf("[PANEL01] CSalida:%d | VBateria:%d | VPaneles:%d | VSalida:%d\n",
                mCSalida01, mVBateria01, mVPaneles01, mVSalida01);
  Serial.printf("[PANEL02] CSalida:%d | VBateria:%d | VPaneles:%d | VSalida:%d\n",
                mCSalida02, mVBateria02, mVPaneles02, mVSalida02);
  Serial.printf("[BOMBAS] B1:%d | B2:%d | B3:%d | B4:%d | T1:%d°C | T2:%d°C\n",
                mBomba01, mBomba02, mBomba03, mBomba04, mTemp01, mTemp02);

  delay(200);
}
