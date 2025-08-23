#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <time.h>

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
const int VPaneles01   = 7;
const int VSalida01    = 15;

const int CSalidaPin02 = 16;
const int VBateriaPin02 = 8;
const int VPaneles02    = 3;
const int VSalida02     = 9;

const int solar = 42;
const int calle = 41;

// -------------------- PINES BOMBAS Y TEMPERATURA --------------------
const int bomba01 = 12;
const int bomba02 = 13;
const int temp01Pin = 10;

const int bomba03 = 14;
const int bomba04 = 2;
const int temp02Pin = 11;

// -------------------- Variables bombas --------------------
bool estadoBomba01 = false;
bool estadoBomba02 = false;
bool estadoBomba03 = false;
bool estadoBomba04 = false;

// -------------------- Variables temperatura --------------------
int temperatura01 = 0;
int temperatura02 = 0;

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

// Variables globales para energía
bool estadoSolar = false;
bool estadoCalle = false;

// Guardar qué bombas estaban activas antes del cambio
bool bombasPrevias[4] = {false, false, false, false};

// ==== NUEVO ====
bool bombasDirty = false;        // marcar cuando haya que subir el estado de bombas
unsigned long lastSyncMs = 0;    // rate-limit para Firebase
const unsigned long SYNC_INTERVAL_MS = 2000;

//Estado de bombas en el ciclo
bool enCambioFuente = false;

//-------------------- Tiempo ----------------------
unsigned long lastMensajeMs = 0;
const unsigned long INTERVALO_MENSAJE_MS = 120000; // 1 hora en ms - 3600000
int contadorMensajes01 = 1;
int contadorMensajes02 = 1;

// -------------------- Helpers --------------------
void actualizarBombas() {
  if (enCambioFuente) return;
  int activas = (estadoBomba01 ? 1:0) + (estadoBomba02 ? 1:0) + (estadoBomba03 ? 1:0) + (estadoBomba04 ? 1:0);

  if (activas > 2) {
    if (estadoBomba03 && activas > 2) { estadoBomba03 = false; activas--; }
    if (estadoBomba04 && activas > 2) { estadoBomba04 = false; activas--; }
    if (estadoBomba02 && activas > 2) { estadoBomba02 = false; activas--; }
    if (estadoBomba01 && activas > 2) { estadoBomba01 = false; activas--; }
  }

  digitalWrite(bomba01, estadoBomba01 ? HIGH : LOW);
  digitalWrite(bomba02, estadoBomba02 ? HIGH : LOW);
  digitalWrite(bomba03, estadoBomba03 ? HIGH : LOW);
  digitalWrite(bomba04, estadoBomba04 ? HIGH : LOW);

  bombasDirty = true; // ==== NUEVO ==== cualquier cambio local marca dirty
}

void cambiarFuenteEnergia(bool usarSolar) {
  enCambioFuente = true;  // BLOQUEAR control de bombas
  // Guardar estado previo de bombas
  bombasPrevias[0] = estadoBomba01;
  bombasPrevias[1] = estadoBomba02;
  bombasPrevias[2] = estadoBomba03;
  bombasPrevias[3] = estadoBomba04;

  // Apagar bombas temporalmente
  estadoBomba01 = false;
  estadoBomba02 = false;
  estadoBomba03 = false;
  estadoBomba04 = false;
  digitalWrite(bomba01, LOW);
  digitalWrite(bomba02, LOW);
  digitalWrite(bomba03, LOW);
  digitalWrite(bomba04, LOW);

  delay(5000); // esperar 5s

  // Cambiar pines según fuente
  if (usarSolar) {
    estadoSolar = true;
    estadoCalle = false;
    digitalWrite(solar, HIGH);
    digitalWrite(calle, LOW);
  } else {
    estadoSolar = false;
    estadoCalle = true;
    digitalWrite(solar, LOW);
    digitalWrite(calle, HIGH);
  }

  delay(10000); // esperar 10s

  // Restaurar bombas previas
  estadoBomba01 = bombasPrevias[0];
  estadoBomba02 = bombasPrevias[1];
  estadoBomba03 = bombasPrevias[2];
  estadoBomba04 = bombasPrevias[3];
  digitalWrite(bomba01, estadoBomba01 ? HIGH : LOW);
  digitalWrite(bomba02, estadoBomba02 ? HIGH : LOW);
  digitalWrite(bomba03, estadoBomba03 ? HIGH : LOW);
  digitalWrite(bomba04, estadoBomba04 ? HIGH : LOW);
  enCambioFuente = false; // DESBLOQUEAR control normal
}

void leerTemperaturas() {
  int valTemp01 = analogRead(temp01Pin);
  int valTemp02 = analogRead(temp02Pin);

  temperatura01 = map(valTemp01, 0, 4095, 0, 100);
  temperatura02 = map(valTemp02, 0, 4095, 0, 100);
}

// ---- Lectura de órdenes remotas de bombas ----
void syncBombasDesdeFirebase() {
  if (!Firebase.ready()) return;

  bool n1 = estadoBomba01, n2 = estadoBomba02, n3 = estadoBomba03, n4 = estadoBomba04;

  if (Firebase.RTDB.getBool(&fbdo, "/bombas/grupo01/bomba01")) n1 = fbdo.boolData();
  if (Firebase.RTDB.getBool(&fbdo, "/bombas/grupo01/bomba02")) n2 = fbdo.boolData();
  if (Firebase.RTDB.getBool(&fbdo, "/bombas/grupo02/bomba03")) n3 = fbdo.boolData();
  if (Firebase.RTDB.getBool(&fbdo, "/bombas/grupo02/bomba04")) n4 = fbdo.boolData();

  // Solo aplicar si hubo cambios
  bool changed = (n1 != estadoBomba01) || (n2 != estadoBomba02) || (n3 != estadoBomba03) || (n4 != estadoBomba04);
  if (changed) {
    estadoBomba01 = n1;
    estadoBomba02 = n2;
    estadoBomba03 = n3;
    estadoBomba04 = n4;
    actualizarBombas(); // esto ajusta a la regla <= 2 activas, actualiza pines y marca dirty
  }
}

// ---- Subidas separadas (paneles, temperaturas, energía, bombas) ----
void pushPaneles() {
  if (!Firebase.ready()) return;

  FirebaseJson json01;
  json01.set("CSalida", mCSalida01);
  json01.set("VBateria", mVBateria01);
  json01.set("VPaneles", mVPaneles01);
  json01.set("VSalida", mVSalida01);
  if (!Firebase.RTDB.setJSON(&fbdo, "/paneles/panel01", &json01))
    Serial.println("Error al subir Panel01: " + fbdo.errorReason());

  FirebaseJson json02;
  json02.set("CSalida", mCSalida02);
  json02.set("VBateria", mVBateria02);
  json02.set("VPaneles", mVPaneles02);
  json02.set("VSalida", mVSalida02);
  if (!Firebase.RTDB.setJSON(&fbdo, "/paneles/panel02", &json02))
    Serial.println("Error al subir Panel02: " + fbdo.errorReason());
}

void pushTemperaturas() {
  if (!Firebase.ready()) return;

  FirebaseJson g1;
  g1.set("temp01", temperatura01);
  if (!Firebase.RTDB.updateNode(&fbdo, "/bombas/grupo01", &g1))
    Serial.println("Error al subir temp01: " + fbdo.errorReason());

  FirebaseJson g2;
  g2.set("temp02", temperatura02);
  if (!Firebase.RTDB.updateNode(&fbdo, "/bombas/grupo02", &g2))
    Serial.println("Error al subir temp02: " + fbdo.errorReason());
}

void pushEnergia() {
  if (!Firebase.ready()) return;

  FirebaseJson jsonEnergia;
  jsonEnergia.set("solar", estadoSolar);
  jsonEnergia.set("calle", estadoCalle);
  if (!Firebase.RTDB.updateNode(&fbdo, "/bombas/energia", &jsonEnergia))
    Serial.println("Error al subir energia: " + fbdo.errorReason());
}

void pushMensajePaneles() {
  if (!Firebase.ready()) return;

  // Obtener hora actual
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error al obtener hora");
    return;
  }

  char fecha[12];
  char hora[10];
  strftime(fecha, sizeof(fecha), "%d/%m/%y", &timeinfo);
  strftime(hora, sizeof(hora), "%H:%M", &timeinfo);

  // -------- Panel01 --------
  String ruta01 = "/datos/panel01/mensaje" + String(contadorMensajes01);
  FirebaseJson msg01;
  msg01.set("CSalida", mCSalida01);
  msg01.set("VSalida", mVSalida01);
  msg01.set("dia", fecha);
  msg01.set("hora", hora);
  if (Firebase.RTDB.setJSON(&fbdo, ruta01.c_str(), &msg01)) {
    Serial.println("Mensaje Panel01 enviado: " + ruta01);
    contadorMensajes01++;
  } else {
    Serial.println("Error Panel01: " + fbdo.errorReason());
  }

  // -------- Panel02 --------
  String ruta02 = "/datos/panel02/mensaje" + String(contadorMensajes02);
  FirebaseJson msg02;
  msg02.set("CSalida", mCSalida02);
  msg02.set("VSalida", mVSalida02);
  msg02.set("dia", fecha);
  msg02.set("hora", hora);
  if (Firebase.RTDB.setJSON(&fbdo, ruta02.c_str(), &msg02)) {
    Serial.println("Mensaje Panel02 enviado: " + ruta02);
    contadorMensajes02++;
  } else {
    Serial.println("Error Panel02: " + fbdo.errorReason());
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
  
  configTime(-5 * 3600, 0, "pool.ntp.org");  // UTC-5 Ecuador
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error al obtener hora NTP");
  } else {
    Serial.println(&timeinfo, "Hora sincronizada: %d/%m/%Y %H:%M:%S");
  }
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

void enviarAFirebase(void *parameter) {
  for (;;) {
    if (Firebase.ready()) {
      // Leer bombas desde Firebase (solo lectura)
      syncBombasDesdeFirebase();

      // Subir datos de paneles, temperaturas y energía
      pushPaneles();
      pushTemperaturas();
      pushEnergia();

      // Tiempo
      if (millis() - lastMensajeMs >= INTERVALO_MENSAJE_MS) {
        pushMensajePaneles();
        lastMensajeMs = millis();
      }
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS); // esperar 2s
  }
}

// -------------------- SETUP --------------------
void setup() {
  pinMode(bomba01, OUTPUT);
  pinMode(bomba02, OUTPUT);
  pinMode(bomba03, OUTPUT);
  pinMode(bomba04, OUTPUT);

  pinMode(solar, OUTPUT);
  pinMode(calle, OUTPUT);

  Serial.begin(115200);
  
  setup_WIFI();
  setupFirebase();

  // Lectura inicial para decidir fuente de energía
  int valVBat2 = analogRead(VBateriaPin02);
  mVBateria02 = map(valVBat2, 0, 4095, 0, 260);

  if (mVBateria02 <= 210) {
    cambiarFuenteEnergia(false); // iniciar con calle
  } else if (mVBateria02 >= 230) {
    cambiarFuenteEnergia(true);  // iniciar con solar
  }

  xTaskCreatePinnedToCore(
    enviarAFirebase,    // función de la tarea
    "FirebaseTask",     // nombre de la tarea
    8192,               // stack en bytes
    NULL,               // parámetro
    1,                  // prioridad
    NULL,               // handle
    1                   // core
  );
}

// -------------------- LOOP --------------------
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

  // -------- Lógica de cambio de fuente --------
  if (mVBateria02 <= 210 && estadoSolar) {
    cambiarFuenteEnergia(false); // pasar a calle
  } else if (mVBateria02 >= 230 && estadoCalle) {
    cambiarFuenteEnergia(true);  // pasar a solar
  }

  // Bombas y temperatura locales
  actualizarBombas();  // regla de máximo 2 activas
  leerTemperaturas();

  // -------- Serial debug --------
  Serial.printf("[PANEL01] CSalida:%d | VBateria:%d | VPaneles:%d | VSalida:%d\n",
                mCSalida01, mVBateria01, mVPaneles01, mVSalida01);

  Serial.printf("[PANEL02] CSalida:%d | VBateria:%d | VPaneles:%d | VSalida:%d\n",
                mCSalida02, mVBateria02, mVPaneles02, mVSalida02);
  
  Serial.printf("[BOMBAS] G1: %d %d | Temp01: %d\n", estadoBomba01, estadoBomba02, temperatura01);
  Serial.printf("[BOMBAS] G2: %d %d | Temp02: %d\n", estadoBomba03, estadoBomba04, temperatura02);

  delay(200);  // lectura rápida local
}

