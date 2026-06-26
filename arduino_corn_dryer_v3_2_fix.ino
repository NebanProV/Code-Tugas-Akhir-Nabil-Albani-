/*
 * ============================================================================
 * SISTEM PENGERING JAGUNG INFRARED-ASSISTED BERBASIS IoT
 * Arduino UNO R4 WiFi — Main Controller
 * VERSI 3.2.1
 * ============================================================================
 *
 * PERUBAHAN v3.2 (dari v3.1):
 * ─────────────────────────────────────────────────────────────────────────
 *  FITUR BARU: TIMER PENGERINGAN OTOMATIS
 *  ─────────────────────────────────────────────────────────────────────
 *  Operator dapat men-set durasi pengeringan (1–599 menit) dari layar TFT.
 *  Saat timer habis → Heater (SSR1) dan Motor (SSR2) mati OTOMATIS.
 *
 *  Perintah HTTP timer (dari ESP8266 port 8081):
 *    GET /cmd?timer=120   → Set durasi 120 menit
 *    GET /cmd?tstart=1    → Mulai countdown
 *    GET /cmd?tstop=1     → Pause countdown
 *    GET /cmd?treset=1    → Reset timer ke durasi awal
 *
 *  Arduino mengirimkan status timer di setiap POST ke ESP8266:
 *    timer_dur  = total durasi (detik)
 *    timer_rem  = sisa waktu (detik)
 *    timer_run  = 1 jika sedang berjalan
 *    timer_done = 1 jika timer telah selesai
 * ─────────────────────────────────────────────────────────────────────────
 *
 * PERUBAHAN v3.1 (dari v3.0):
 * ─────────────────────────────────────────────────────────────────────────
 *  FITUR BARU: HTTP Command Server (port 8081)
 *  ─────────────────────────────────────────────────────────────────────
 *  Arduino kini menjalankan WiFiServer di port 8081 secara bersamaan
 *  dengan fungsi client (kirim data ke ESP8266).
 *
 *  ESP8266 dapat mengirim perintah ke Arduino melalui HTTP GET:
 *
 *    GET /cmd?sp=80        → Set setpoint ke 80°C
 *    GET /cmd?sp=100       → Set setpoint ke 100°C
 *    GET /cmd?mode=AUTO    → Ganti ke mode AUTO
 *    GET /cmd?mode=MANUAL  → Ganti ke mode MANUAL
 *    GET /cmd?motor=1      → Motor ON
 *    GET /cmd?motor=0      → Motor OFF
 *    GET /cmd?fan=200      → Set kecepatan fan (0–255)
 *    GET /cmd?htr=1        → Heater ON (hanya mode MANUAL)
 *    GET /cmd?htr=0        → Heater OFF (hanya mode MANUAL)
 *    GET /cmd?ping         → Test koneksi, response: "PONG"
 *
 *  Arduino membalas dengan HTTP 200 dan body "OK:pesan" atau "ERR:alasan"
 *
 *  PERUBAHAN LAIN:
 *  - setSetpoint() kini menerima semua nilai 60.0–135.0°C (tidak hanya
 *    nilai preset). Ini diperlukan karena TFT step 1°C menghasilkan nilai
 *    seperti 61, 62, 63... yang sebelumnya ditolak.
 *  - Arduino mengirimkan IP-nya sendiri di field "arduino_ip" pada setiap
 *    POST ke ESP8266, agar ESP8266 tahu alamat yang harus dihubungi.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * ARSITEKTUR KOMUNIKASI v3.1:
 *
 *   Arduino (client + server:8081)
 *       │
 *       │  POST /data (setiap 2 detik) ──────────────────────────────→ ESP8266
 *       │                                                              (server:80)
 *       │
 *       ├─ [WiFiServer port 8081] ←── GET /cmd?sp=80 ─────────────── ESP8266
 *       │                              (saat tombol TFT disentuh)     (client)
 *       │
 *       └─ Response "OK:SP=80.0C" ──────────────────────────────────→ ESP8266
 *
 * ─────────────────────────────────────────────────────────────────────────
 * HARDWARE:
 *   5× MAX6675 K-Type Thermocouple (SPI shared bus)
 *     TC1: Inlet         (CS: A0)
 *     TC2: Outlet        (CS: A1)
 *     TC3: Chamber Kiri  (CS: A2)
 *     TC4: Chamber Tengah(CS: A3)
 *     TC5: Chamber Kanan (CS: A4)
 *   1× DHT22             → D2
 *   2× Fan DC 12V via L298N
 *     Fan1: ENA=D5(PWM), IN1=D6, IN2=D7
 *     Fan2: ENB=D9(PWM), IN3=D10, IN4=D11
 *   2× SSR-40DA
 *     SSR1 → D3: Semua Heater (1125W)
 *     SSR2 → D4: Motor 1-fase (0.37kW)
 *   LED Status → D8
 * ============================================================================
 */

#include <WiFiS3.h>
#include <DHT.h>
#include <max6675.h>

// ===================================================================
// PIN CONFIGURATION
// ===================================================================
#define DHT_PIN     2
#define DHT_TYPE    DHT22
#define MAX_SCK     13
#define MAX_SO      12
#define TC1_CS      A0   // Inlet
#define TC2_CS      A1   // Outlet
#define TC3_CS      A2   // Chamber Kiri
#define TC4_CS      A3   // Chamber Tengah
#define TC5_CS      A4   // Chamber Kanan
#define FAN1_ENA    5    // PWM
#define FAN1_IN1    6
#define FAN1_IN2    7
#define FAN2_ENB    9    // PWM
#define FAN2_IN3    10
#define FAN2_IN4    11
#define SSR_HEATERS 3
#define SSR_MOTOR   4
#define LED_STATUS  8

// ===================================================================
// KONFIGURASI WIFI & SERVER
// ===================================================================
const char* ssid       = "ESP8266_Server";
const char* password   = "12345678";
const char* serverIP   = "192.168.4.1";    // IP AP ESP8266
const int   serverPort = 80;               // Port POST data ke ESP8266
const int   CMD_PORT   = 8081;             // Port server perintah dari ESP8266

// ===================================================================
// SETPOINT
// ===================================================================
float SETPOINT_TEMP = 60.0;   // Setpoint aktif, range 60–135°C

// ===================================================================
// SAFETY PARAMETERS
// ===================================================================
const float TEMP_CHAMBER_MAX    = 145.0;
const float TEMP_INLET_MAX      = 160.0;
const unsigned long OVERHEAT_DELAY = 5000UL;

// ===================================================================
// HYSTERESIS & PID
// ===================================================================
float getHysteresis() {
  return (SETPOINT_TEMP >= 100.0) ? 3.0f : 2.0f;
}

float Kp = 2.0, Ki = 0.5, Kd = 1.0;
const float PID_INTEGRAL_LIMIT = 50.0;

// ===================================================================
// STATUS KONTROL
// ===================================================================
bool  heater_state   = false;
bool  motor_state    = false;
bool  auto_mode      = true;
int   fan_speed      = 128;
bool  emergency_stop = false;
unsigned long overheat_timer = 0;

// ===================================================================
// PID VARIABLES
// ===================================================================
float pid_integral   = 0.0;
float pid_last_error = 0.0;
unsigned long pid_last_time = 0;

// ===================================================================
// TIMER VARIABLES — BARU v3.2
// ===================================================================
// Semua waktu dalam satuan DETIK.
// Durasi maksimum: 599 menit = 35940 detik.
uint32_t timerDuration  = 0;      // Durasi total yang di-set (detik)
uint32_t timerRemaining = 0;      // Sisa waktu (detik)
bool     timerRunning   = false;  // Apakah countdown sedang berjalan
bool     timerDone      = false;  // Apakah timer sudah selesai
unsigned long timerLastTick = 0;  // millis() saat detik terakhir dikurangi

// ===================================================================
// SENSOR OBJECTS
// ===================================================================
DHT     dht(DHT_PIN, DHT_TYPE);
MAX6675 tc1(MAX_SCK, TC1_CS, MAX_SO);
MAX6675 tc2(MAX_SCK, TC2_CS, MAX_SO);
MAX6675 tc3(MAX_SCK, TC3_CS, MAX_SO);
MAX6675 tc4(MAX_SCK, TC4_CS, MAX_SO);
MAX6675 tc5(MAX_SCK, TC5_CS, MAX_SO);

WiFiClient  client;                // Untuk kirim POST ke ESP8266
WiFiServer  cmdServer(CMD_PORT);   // ← BARU v3.1: server terima perintah TFT

// ===================================================================
// SENSOR DATA
// ===================================================================
float temp_tc1 = 0, temp_tc2 = 0, temp_tc3 = 0, temp_tc4 = 0, temp_tc5 = 0;
float temp_avg_chamber = 0;
float delta_temp       = 0;
float temp_ambient     = 0;
float humidity_ambient = 0;

// ===================================================================
// TIMING
// ===================================================================
const unsigned long SENSOR_INTERVAL = 1000UL;
const unsigned long SEND_INTERVAL   = 2000UL;
const unsigned long PID_INTERVAL    = 1000UL;
const unsigned long STATUS_INTERVAL = 5000UL;

unsigned long lastSensorRead  = 0;
unsigned long lastSendTime    = 0;
unsigned long lastPIDUpdate   = 0;
unsigned long lastStatusPrint = 0;

// ===================================================================
// SETUP
// ===================================================================
void setup() {
  Serial.begin(9600);
  delay(300);
  printBanner();
  setupPins();

  Serial.println(F("[1] Inisialisasi Sensor..."));
  dht.begin();
  delay(1200);

  float t1=tc1.readCelsius(); delay(120);
  float t2=tc2.readCelsius(); delay(120);
  float t3=tc3.readCelsius(); delay(120);
  float t4=tc4.readCelsius(); delay(120);
  float t5=tc5.readCelsius(); delay(120);
  Serial.print(F("  TC1 Inlet   : ")); Serial.print(t1,1); Serial.println(F(" C"));
  Serial.print(F("  TC2 Outlet  : ")); Serial.print(t2,1); Serial.println(F(" C"));
  Serial.print(F("  TC3 Kiri    : ")); Serial.print(t3,1); Serial.println(F(" C"));
  Serial.print(F("  TC4 Tengah  : ")); Serial.print(t4,1); Serial.println(F(" C"));
  Serial.print(F("  TC5 Kanan   : ")); Serial.print(t5,1); Serial.println(F(" C"));
  float dhtT=dht.readTemperature(), dhtH=dht.readHumidity();
  Serial.print(F("  DHT22 Suhu  : ")); Serial.print(dhtT,1); Serial.println(F(" C"));
  Serial.print(F("  DHT22 Hum   : ")); Serial.print(dhtH,1); Serial.println(F(" %"));
  Serial.println(F("  [OK] Sensor siap\n"));

  Serial.println(F("[2] Inisialisasi Aktuator (semua OFF)..."));
  digitalWrite(SSR_HEATERS, LOW);
  digitalWrite(SSR_MOTOR,   LOW);
  runFan(FAN1_ENA, FAN1_IN1, FAN1_IN2, fan_speed);
  runFan(FAN2_ENB, FAN2_IN3, FAN2_IN4, fan_speed);
  Serial.println(F("  [OK] Aktuator siap\n"));

  Serial.println(F("[3] Menghubungkan ke WiFi ESP8266..."));
  connectWiFi();

  // ── BARU v3.1: Jalankan command server ─────────────────────────
  Serial.println(F("[4] Menjalankan Command Server (port 8081)..."));
  cmdServer.begin();
  Serial.print(F("  [OK] Command server aktif di IP: "));
  Serial.print(WiFi.localIP());
  Serial.println(F(":8081"));

  Serial.println(F("\n============================================================"));
  Serial.println(F("  CORN DRYER v3.1 - SIAP BEROPERASI"));
  Serial.println(F("  TC1=Inlet | TC2=Outlet | TC3=L | TC4=C | TC5=R"));
  Serial.print  (F("  Setpoint : ")); Serial.print(SETPOINT_TEMP,1); Serial.println(F(" C"));
  Serial.println(F("  Port POST  : 192.168.4.1:80  (Arduino → ESP8266)"));
  Serial.print  (F("  Port CMD   : ")); Serial.print(WiFi.localIP()); Serial.println(F(":8081  (ESP8266 → Arduino)"));
  Serial.println(F("  Ketik 'HELP' untuk daftar perintah"));
  Serial.println(F("============================================================\n"));

  pid_last_time = millis();
}

// ===================================================================
// MAIN LOOP
// ===================================================================
void loop() {
  processSerialCommand();
  handleCmdServer();
  updateTimer();

  // Reconnect WiFi jika putus
  if (WiFi.status() != WL_CONNECTED) connectWiFi();

  // Monitor free RAM setiap 30 detik — deteksi awal memory leak
  static unsigned long lastHeapPrint = 0;
  if (millis() - lastHeapPrint >= 30000UL) {
    lastHeapPrint = millis();
    // Arduino UNO R4 WiFi = ARM Cortex-M4, bukan AVR
    // Cara ukur free RAM: buat variabel lokal di stack,
    // lalu hitung jarak antara stack pointer dan heap pointer
    // menggunakan malloc sementara sebagai estimasi
    char stackTop;                         // variabel lokal → ada di stack
    char* heapTop = (char*)malloc(1);      // alokasi kecil → ujung heap
    int freeRAM = 0;
    if (heapTop) {
      freeRAM = (int)(&stackTop) - (int)(heapTop);
      free(heapTop);
    }
    if (freeRAM < 0) freeRAM = -freeRAM;  // stack tumbuh ke bawah di ARM
    Serial.print(F("[MEM] Free RAM ~")); Serial.print(freeRAM); Serial.println(F(" bytes"));
    if (freeRAM < 1024) {
      Serial.println(F("[WARN] RAM < 1KB! Risiko crash. Pertimbangkan reboot."));
    }
  }

  // Baca sensor setiap 1 detik
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    readAllSensors();
    lastSensorRead = millis();
  }

  // Safety check (selalu)
  safetyCheck();

  // PID control
  if (auto_mode && !emergency_stop &&
      millis() - lastPIDUpdate >= PID_INTERVAL) {
    pidControl();
    lastPIDUpdate = millis();
  }

  // Kirim data ke ESP8266
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    sendSensorData();
    lastSendTime = millis();
  }

  // Print status serial
  if (millis() - lastStatusPrint >= STATUS_INTERVAL) {
    printStatus();
    lastStatusPrint = millis();
  }

  delay(50);
}

// ===================================================================
// PIN SETUP
// ===================================================================
void setupPins() {
  pinMode(SSR_HEATERS, OUTPUT); digitalWrite(SSR_HEATERS, LOW);
  pinMode(SSR_MOTOR,   OUTPUT); digitalWrite(SSR_MOTOR,   LOW);
  pinMode(FAN1_ENA,    OUTPUT);
  pinMode(FAN1_IN1,    OUTPUT); digitalWrite(FAN1_IN1,    LOW);
  pinMode(FAN1_IN2,    OUTPUT); digitalWrite(FAN1_IN2,    LOW);
  pinMode(FAN2_ENB,    OUTPUT);
  pinMode(FAN2_IN3,    OUTPUT); digitalWrite(FAN2_IN3,    LOW);
  pinMode(FAN2_IN4,    OUTPUT); digitalWrite(FAN2_IN4,    LOW);
  pinMode(LED_STATUS,  OUTPUT); digitalWrite(LED_STATUS,  LOW);
  Serial.println(F("[0] Konfigurasi pin selesai"));
}

// ===================================================================
// BACA SENSOR
// ===================================================================
void readAllSensors() {
  temp_tc1 = validateTC(tc1.readCelsius()); delay(60);
  temp_tc2 = validateTC(tc2.readCelsius()); delay(60);
  temp_tc3 = validateTC(tc3.readCelsius()); delay(60);
  temp_tc4 = validateTC(tc4.readCelsius()); delay(60);
  temp_tc5 = validateTC(tc5.readCelsius()); delay(60);

  temp_avg_chamber = (temp_tc3 + temp_tc4 + temp_tc5) / 3.0f;
  delta_temp = temp_tc2 - temp_tc1;  // Outlet − Inlet

  float dhtT = dht.readTemperature();
  float dhtH = dht.readHumidity();
  if (!isnan(dhtT)) temp_ambient     = dhtT;
  if (!isnan(dhtH)) humidity_ambient = dhtH;
}

float validateTC(float val) {
  if (isnan(val) || val < 0.0f || val >= 1024.0f) return 0.0f;
  return val;
}

// ===================================================================
// PID CONTROL
// ===================================================================
void pidControl() {
  if (SETPOINT_TEMP >= 100.0f) { Kp=1.5f; Ki=0.3f; Kd=1.5f; }
  else                          { Kp=2.0f; Ki=0.5f; Kd=1.0f; }

  float hys   = getHysteresis();
  float error = SETPOINT_TEMP - temp_avg_chamber;

  unsigned long now = millis();
  float dt = (float)(now - pid_last_time) / 1000.0f;
  if (dt < 0.001f) dt = 1.0f;
  pid_last_time = now;

  pid_integral += error * dt;
  pid_integral  = constrain(pid_integral, -PID_INTEGRAL_LIMIT, PID_INTEGRAL_LIMIT);
  pid_last_error = error;

  // Hysteresis control
  if      (temp_avg_chamber < SETPOINT_TEMP - hys) heater_state = true;
  else if (temp_avg_chamber > SETPOINT_TEMP + hys)  heater_state = false;

  digitalWrite(SSR_HEATERS, heater_state ? HIGH : LOW);
}

// ===================================================================
// SAFETY CHECK
// ===================================================================
void safetyCheck() {
  if (emergency_stop) return;

  bool overheat = false;
  static char reason[60];  // static → tidak alokasi heap tiap loop
  reason[0] = '\0';

  if (temp_avg_chamber > TEMP_CHAMBER_MAX) {
    overheat = true;
    snprintf(reason, sizeof(reason), "CHAMBER OVERHEAT! Avg=%.1fC > %.0fC",
             temp_avg_chamber, TEMP_CHAMBER_MAX);
  }
  if (temp_tc1 > 0 && temp_tc1 > TEMP_INLET_MAX) {
    overheat = true;
    snprintf(reason, sizeof(reason), "INLET OVERHEAT! TC1=%.1fC > %.0fC",
             temp_tc1, TEMP_INLET_MAX);
  }

  if (overheat) {
    if (overheat_timer == 0) {
      overheat_timer = millis();
      Serial.print(F("[PERINGATAN] ")); Serial.println(reason);
      Serial.println(F("[PERINGATAN] Emergency stop dalam 5 detik!"));
    }
    digitalWrite(LED_STATUS, (millis() / 200) % 2);
    if (millis() - overheat_timer > OVERHEAT_DELAY) emergencyStop(reason);
  } else {
    overheat_timer = 0;
    digitalWrite(LED_STATUS, HIGH);
  }
}

void emergencyStop(const char* reason) {
  emergency_stop = true;
  digitalWrite(SSR_HEATERS, LOW);
  heater_state = false;
  Serial.println(F("\n=== !!! EMERGENCY STOP AKTIF !!! ==="));
  Serial.print  (F("  Alasan  : ")); Serial.println(reason);
  Serial.println(F("  Heater dimatikan. Reset Arduino untuk lanjut."));
  while (true) {
    digitalWrite(LED_STATUS, HIGH); delay(150);
    digitalWrite(LED_STATUS, LOW);  delay(150);
  }
}

// ===================================================================
// FAN & MOTOR CONTROL
// ===================================================================
void runFan(int ena, int in1, int in2, int speed) {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  analogWrite(ena, speed);
}

void setFanSpeed(int spd) {
  fan_speed = constrain(spd, 0, 255);
  runFan(FAN1_ENA, FAN1_IN1, FAN1_IN2, fan_speed);
  runFan(FAN2_ENB, FAN2_IN3, FAN2_IN4, fan_speed);
  Serial.print(F("Fan speed: "));
  Serial.print(map(fan_speed, 0, 255, 0, 100));
  Serial.println(F("%"));
}

void setMotor(bool state) {
  motor_state = state;
  digitalWrite(SSR_MOTOR, state ? HIGH : LOW);
  Serial.print(F("Motor: ")); Serial.println(state ? F("ON") : F("OFF"));
}

// ===================================================================
// SETPOINT CONTROL — v3.1: menerima semua nilai 60–135°C
// ===================================================================
void setSetpoint(float sp) {
  // v3.0 hanya menerima nilai preset (60/70/80/.../135).
  // v3.1 menerima semua nilai dalam rentang 60.0–135.0°C
  // agar kompatibel dengan TFT yang mengirim step 1°C atau 5°C.
  if (sp < 60.0f || sp > 135.0f) {
    Serial.println(F("[ERR] Setpoint di luar rentang 60–135°C"));
    return;
  }
  float oldSP    = SETPOINT_TEMP;
  SETPOINT_TEMP  = sp;
  pid_integral   = 0.0f;  // Reset integral saat setpoint berubah
  pid_last_error = 0.0f;
  Serial.print(F("[OK] Setpoint: "));
  Serial.print(oldSP, 1); Serial.print(F(" C → "));
  Serial.print(SETPOINT_TEMP, 1); Serial.print(F(" C"));
  Serial.print(F("  (Hys: +-")); Serial.print(getHysteresis(),1); Serial.println(F("C)"));
}

// ===================================================================
// TIMER COUNTDOWN — BARU v3.2
// ===================================================================
// Dipanggil setiap loop(). Mengurangi timerRemaining setiap 1 detik.
// Jika timerRemaining mencapai 0 → matikan heater + motor secara otomatis.
void updateTimer() {
  if (!timerRunning || timerRemaining == 0) return;

  if (millis() - timerLastTick >= 1000UL) {
    timerLastTick += 1000UL;  // FIX: += bukan = millis() → tidak ada drift akumulasi
    timerRemaining--;

    if (timerRemaining == 0) {
      // ── TIMER SELESAI: matikan semua beban ──────────────────
      timerRunning  = false;
      timerDone     = true;

      // Matikan heater
      heater_state = false;
      digitalWrite(SSR_HEATERS, LOW);

      // Matikan motor
      motor_state = false;
      digitalWrite(SSR_MOTOR, LOW);

      // Ganti ke mode MANUAL agar PID tidak menyalakan ulang heater
      auto_mode = false;

      // Reset PID integral
      pid_integral   = 0.0f;
      pid_last_error = 0.0f;

      Serial.println(F("\n╔══════════════════════════════════════╗"));
      Serial.println(F("║  TIMER SELESAI! Waktu pengeringan    ║"));
      Serial.println(F("║  telah habis. Heater + Motor → OFF   ║"));
      Serial.println(F("╚══════════════════════════════════════╝\n"));
    }
  }
}

// ===================================================================
// HTTP COMMAND SERVER — v3.2.1 (char[] bukan String = nol heap alloc)
// ===================================================================
void handleCmdServer() {
  WiFiClient cmdClient = cmdServer.available();
  if (!cmdClient) return;

  // Baca request line ke char array — TIDAK ada String di sini
  static char reqBuf[180];
  memset(reqBuf, 0, sizeof(reqBuf));
  unsigned long tStart = millis();
  int idx = 0;
  while (cmdClient.connected() && millis()-tStart < 200 &&
         idx < (int)sizeof(reqBuf)-1) {
    if (cmdClient.available()) {
      char c = cmdClient.read();
      if (c == '\n') break;
      if (c != '\r') reqBuf[idx++] = c;
    }
  }
  // Buang sisa header
  {
    int emptyCount = 0;
    while (cmdClient.connected() && millis()-tStart < 350) {
      if (cmdClient.available()) {
        char c = cmdClient.read();
        if (c == '\n') { emptyCount++; if (emptyCount >= 2) break; }
        else if (c != '\r') emptyCount = 0;
      }
    }
  }

  Serial.print(F("[CMD] ")); Serial.println(reqBuf);

  // Parse query string dari "GET /cmd?key=value HTTP/1.1"
  static char query[60];
  memset(query, 0, sizeof(query));
  char* qPtr = strchr(reqBuf, '?');
  if (qPtr) {
    qPtr++;
    char* spPtr = strchr(qPtr, ' ');
    int qLen = spPtr ? (int)(spPtr-qPtr) : (int)strlen(qPtr);
    if (qLen > 0 && qLen < (int)sizeof(query)-1)
      strncpy(query, qPtr, qLen);
  }

  // Susun response tanpa String object
  static char respBody[48];
  if (strstr(reqBuf, "/cmd?ping")) {
    strncpy(respBody, "PONG", sizeof(respBody));
  } else if (strlen(query) == 0) {
    strncpy(respBody, "ERR:no_query", sizeof(respBody));
  } else {
    executeCommand(query, respBody, sizeof(respBody));
  }

  // HTTP response — satu snprintf, nol alokasi heap
  static char httpResp[128];
  snprintf(httpResp, sizeof(httpResp),
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: %u\r\n"
    "Connection: close\r\n\r\n"
    "%s",
    (unsigned int)strlen(respBody), respBody
  );
  cmdClient.print(httpResp);
  delay(5);
  cmdClient.stop();
  Serial.print(F("[CMD] -> ")); Serial.println(respBody);
}

// ── executeCommand: output ke char buffer, nol heap alloc ─────────
// FIX: signature diubah dari String return ke void+char* output
// agar tidak ada String temporary di stack atau heap
void executeCommand(const char* query, char* out, size_t outLen) {
  // Parse key=value
  const char* eqPtr = strchr(query, '=');
  if (!eqPtr) { strncpy(out, "ERR:no_eq", outLen); return; }

  char key[20] = {0};
  char val[20] = {0};
  int kLen = (int)(eqPtr - query);
  if (kLen >= (int)sizeof(key)) kLen = sizeof(key)-1;
  strncpy(key, query, kLen);
  strncpy(val, eqPtr+1, sizeof(val)-1);

  // lowercase key, uppercase val
  for (int i=0; key[i]; i++) key[i] = tolower((unsigned char)key[i]);
  for (int i=0; val[i]; i++) val[i] = toupper((unsigned char)val[i]);

  // ── SP ────────────────────────────────────────────────────────
  if (strcmp(key,"sp")==0) {
    float sp = atof(val);
    if (sp < 60.0f || sp > 135.0f) {
      snprintf(out, outLen, "ERR:sp_range(%.1f)", sp); return;
    }
    setSetpoint(sp);
    snprintf(out, outLen, "OK:SP=%.1fC", SETPOINT_TEMP); return;
  }
  // ── MODE ──────────────────────────────────────────────────────
  if (strcmp(key,"mode")==0) {
    if (strcmp(val,"AUTO")==0)   { auto_mode=true;  strncpy(out,"OK:MODE=AUTO",outLen); }
    else if (strcmp(val,"MANUAL")==0) { auto_mode=false; strncpy(out,"OK:MODE=MANUAL",outLen); }
    else snprintf(out, outLen, "ERR:mode(%s)", val);
    return;
  }
  // ── MOTOR ─────────────────────────────────────────────────────
  if (strcmp(key,"motor")==0) {
    bool on = (val[0]=='1' || strcmp(val,"ON")==0);
    setMotor(on);
    snprintf(out, outLen, "OK:MOTOR=%s", on?"ON":"OFF"); return;
  }
  // ── FAN ───────────────────────────────────────────────────────
  if (strcmp(key,"fan")==0) {
    int spd = atoi(val);
    if (spd<0||spd>255) { strncpy(out,"ERR:fan_range",outLen); return; }
    setFanSpeed(spd);
    snprintf(out, outLen, "OK:FAN=%ld%%", map(fan_speed,0,255,0,100)); return;
  }
  // ── HTR ───────────────────────────────────────────────────────
  if (strcmp(key,"htr")==0) {
    if (auto_mode) { strncpy(out,"ERR:MANUAL_first",outLen); return; }
    bool on = (val[0]=='1' || strcmp(val,"ON")==0);
    heater_state = on;
    digitalWrite(SSR_HEATERS, on?HIGH:LOW);
    snprintf(out, outLen, "OK:HTR=%s", on?"ON":"OFF"); return;
  }
  // ── TIMER ─────────────────────────────────────────────────────
  if (strcmp(key,"timer")==0) {
    int mins = atoi(val);
    if (mins<1||mins>599) { snprintf(out,outLen,"ERR:tmr(%d)",mins); return; }
    timerDuration  = (uint32_t)mins * 60UL;
    timerRemaining = timerDuration;
    timerRunning   = false;
    timerDone      = false;
    snprintf(out, outLen, "OK:TIMER=%dm", mins);
    Serial.print(F("[TMR] Set: ")); Serial.print(mins); Serial.println(F(" mnt"));
    return;
  }
  if (strcmp(key,"tstart")==0) {
    if (timerDuration==0) { strncpy(out,"ERR:set_timer_first",outLen); return; }
    if (timerRemaining==0) timerRemaining = timerDuration;
    timerRunning  = true;
    timerDone     = false;
    timerLastTick = millis();
    snprintf(out, outLen, "OK:TSTART(%lum)", (unsigned long)(timerRemaining/60));
    Serial.print(F("[TMR] Mulai: ")); Serial.print(timerRemaining/60); Serial.println(F(" mnt"));
    return;
  }
  if (strcmp(key,"tstop")==0) {
    timerRunning = false;
    strncpy(out, "OK:TSTOP", outLen);
    Serial.println(F("[TMR] Pause.")); return;
  }
  if (strcmp(key,"treset")==0) {
    timerRunning   = false;
    timerDone      = false;
    timerRemaining = timerDuration;
    strncpy(out, "OK:TRESET", outLen);
    Serial.println(F("[TMR] Reset.")); return;
  }
  if (strcmp(key,"ping")==0) { strncpy(out,"PONG",outLen); return; }

  snprintf(out, outLen, "ERR:unknown(%s)", key);
}

// ===================================================================
// SERIAL COMMAND (tetap berfungsi seperti v3.0)
// ===================================================================
void processSerialCommand() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim(); cmd.toUpperCase();

  if      (cmd.startsWith("SP:"))  setSetpoint(cmd.substring(3).toFloat());
  else if (cmd == "AUTO")          { auto_mode=true;  Serial.println(F("[OK] Mode: AUTO")); }
  else if (cmd == "MANUAL")        { auto_mode=false; Serial.println(F("[OK] Mode: MANUAL")); }
  else if (cmd == "H_ON")  {
    if (!auto_mode) { heater_state=true;  digitalWrite(SSR_HEATERS,HIGH); Serial.println(F("[OK] Heater ON")); }
    else Serial.println(F("[ERR] Set MANUAL dulu!"));
  }
  else if (cmd == "H_OFF") {
    if (!auto_mode) { heater_state=false; digitalWrite(SSR_HEATERS,LOW);  Serial.println(F("[OK] Heater OFF")); }
    else Serial.println(F("[ERR] Set MANUAL dulu!"));
  }
  else if (cmd == "M_ON")           setMotor(true);
  else if (cmd == "M_OFF")          setMotor(false);
  else if (cmd.startsWith("FAN:"))  setFanSpeed(cmd.substring(4).toInt());
  else if (cmd == "STATUS")         printStatus();
  else if (cmd == "HELP") {
    Serial.println(F("\n=== PERINTAH SERIAL ==="));
    Serial.println(F("  SP:60~135  Set setpoint (rentang bebas 60–135)"));
    Serial.println(F("  AUTO       Mode otomatis PID"));
    Serial.println(F("  MANUAL     Mode manual"));
    Serial.println(F("  H_ON/H_OFF Toggle heater (MANUAL)"));
    Serial.println(F("  M_ON/M_OFF Toggle motor"));
    Serial.println(F("  FAN:0~255  Set kecepatan fan"));
    Serial.println(F("  STATUS     Status lengkap"));
    Serial.println(F("===========================\n"));
    Serial.println(F("=== PERINTAH HTTP (dari ESP8266:8081) ==="));
    Serial.println(F("  GET /cmd?sp=80     Set setpoint"));
    Serial.println(F("  GET /cmd?mode=AUTO Mode AUTO/MANUAL"));
    Serial.println(F("  GET /cmd?motor=1   Motor ON/OFF"));
    Serial.println(F("  GET /cmd?fan=200   Fan speed (0-255)"));
    Serial.println(F("  GET /cmd?htr=1     Heater ON/OFF (MANUAL)"));
    Serial.println(F("  GET /cmd?ping      Test koneksi"));
    Serial.println(F("=========================================\n"));
  }
}

// ===================================================================
// WIFI CONNECT
// ===================================================================
void connectWiFi() {
  Serial.print(F("  Connecting to ")); Serial.println(ssid);
  WiFi.begin(ssid, password);
  int n = 0;
  while (WiFi.status() != WL_CONNECTED && n < 20) {
    delay(500); Serial.print(F("."));
    digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
    n++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F(" OK!"));
    Serial.print(F("  IP Arduino: ")); Serial.println(WiFi.localIP());
    digitalWrite(LED_STATUS, HIGH);
  } else {
    Serial.println(F(" GAGAL (lanjut tanpa WiFi)"));
    digitalWrite(LED_STATUS, LOW);
  }
}

// ===================================================================
// KIRIM DATA KE ESP8266
// FIX v3.2.1: Ganti String += dengan static char[] + snprintf
// String += tiap 2 detik → 38.000+ alokasi heap dalam 54 menit
// → heap fragmentation → Arduino crash → SSR off → heater/motor mati
// static char[] dialokasikan SEKALI di BSS segment, tidak di heap
// ===================================================================
void sendSensorData() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!client.connect(serverIP, serverPort)) return;

  // IP Arduino tanpa String object
  IPAddress myIP = WiFi.localIP();
  char ipBuf[16];
  snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d",
           myIP[0], myIP[1], myIP[2], myIP[3]);

  // ── Body POST — satu snprintf, nol alokasi heap ──────────────
  static char data[768];
  snprintf(data, sizeof(data),
    "tc1=%.2f&tc2=%.2f&tc3=%.2f&tc4=%.2f&tc5=%.2f"
    "&temp_avg=%.2f&delta_temp=%.2f"
    "&temp_ambient=%.2f&humidity=%.2f"
    "&heater=%d&motor=%d&fan_speed=%d"
    "&setpoint=%.1f&mode=%s"
    "&emergency=%d&hysteresis=%.1f"
    "&arduino_ip=%s"
    "&timer_dur=%lu&timer_rem=%lu"
    "&timer_run=%d&timer_done=%d",
    temp_tc1, temp_tc2, temp_tc3, temp_tc4, temp_tc5,
    temp_avg_chamber, delta_temp,
    temp_ambient, humidity_ambient,
    heater_state ? 1 : 0, motor_state ? 1 : 0, fan_speed,
    SETPOINT_TEMP, auto_mode ? "AUTO" : "MANUAL",
    emergency_stop ? 1 : 0, getHysteresis(),
    ipBuf,
    (unsigned long)timerDuration, (unsigned long)timerRemaining,
    timerRunning ? 1 : 0, timerDone ? 1 : 0
  );

  // ── HTTP Request header — satu snprintf ──────────────────────
  static char req[900];
  snprintf(req, sizeof(req),
    "POST /data HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Content-Type: application/x-www-form-urlencoded\r\n"
    "Content-Length: %u\r\n"
    "Connection: close\r\n\r\n"
    "%s",
    serverIP, (unsigned int)strlen(data), data
  );

  client.print(req);

  unsigned long t = millis();
  while (!client.available() && millis()-t < 5000);
  while (client.available()) client.read();
  client.stop();
}

// ===================================================================
// PRINT STATUS
// ===================================================================
void printStatus() {
  Serial.println(F("\n============================================================"));
  Serial.println(F("  CORN DRYER v3.1 - STATUS SISTEM"));
  Serial.println(F("  ---- SENSOR ----"));
  Serial.print(F("  TC1 Inlet    : ")); Serial.print(temp_tc1,1); Serial.println(F(" C"));
  Serial.print(F("  TC2 Outlet   : ")); Serial.print(temp_tc2,1); Serial.println(F(" C"));
  Serial.print(F("  Delta T      : ")); Serial.print(delta_temp,2); Serial.println(F(" C"));
  Serial.print(F("  TC3 Kiri     : ")); Serial.print(temp_tc3,1); Serial.println(F(" C"));
  Serial.print(F("  TC4 Tengah   : ")); Serial.print(temp_tc4,1); Serial.println(F(" C"));
  Serial.print(F("  TC5 Kanan    : ")); Serial.print(temp_tc5,1); Serial.println(F(" C"));
  Serial.print(F("  Avg Chamber  : ")); Serial.print(temp_avg_chamber,2); Serial.println(F(" C"));
  Serial.print(F("  Ambient      : ")); Serial.print(temp_ambient,1); Serial.println(F(" C"));
  Serial.print(F("  Humidity     : ")); Serial.print(humidity_ambient,1); Serial.println(F(" %"));
  Serial.println(F("  ---- KONTROL ----"));
  Serial.print(F("  Setpoint     : ")); Serial.print(SETPOINT_TEMP,1); Serial.println(F(" C"));
  Serial.print(F("  Hysteresis   : +-")); Serial.print(getHysteresis(),1); Serial.println(F(" C"));
  Serial.print(F("  Mode         : ")); Serial.println(auto_mode ? F("AUTO") : F("MANUAL"));
  Serial.print(F("  Heater       : ")); Serial.println(heater_state ? F("ON") : F("OFF"));
  Serial.print(F("  Motor        : ")); Serial.println(motor_state  ? F("ON") : F("OFF"));
  Serial.print(F("  Fan Speed    : ")); Serial.print(map(fan_speed,0,255,0,100)); Serial.println(F("%"));
  Serial.print(F("  Emergency    : ")); Serial.println(emergency_stop ? F("AKTIF!") : F("Normal"));
  Serial.println(F("  ---- TIMER ----"));
  if (timerDuration == 0) {
    Serial.println(F("  Timer        : Belum di-set"));
  } else {
    uint32_t h = timerRemaining/3600, m = (timerRemaining%3600)/60, s = timerRemaining%60;
    char tbuf[20]; sprintf(tbuf, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    Serial.print(F("  Durasi set   : ")); Serial.print(timerDuration/60); Serial.println(F(" menit"));
    Serial.print(F("  Sisa waktu   : ")); Serial.println(tbuf);
    Serial.print(F("  Status       : "));
    if (timerDone)         Serial.println(F("SELESAI"));
    else if (timerRunning) Serial.println(F("BERJALAN"));
    else                   Serial.println(F("PAUSE/SIAP"));
  }
  Serial.println(F("  ---- JARINGAN ----"));
  Serial.print(F("  IP Arduino   : ")); Serial.println(WiFi.localIP());
  Serial.print(F("  CMD Server   : port ")); Serial.println(CMD_PORT);
  Serial.println(F("============================================================\n"));
}

// ===================================================================
// PRINT BANNER
// ===================================================================
void printBanner() {
  Serial.println(F("\n============================================================"));
  Serial.println(F("  CORN DRYER CONTROL SYSTEM v3.2.1 FIX"));
  Serial.println(F("  Infrared-Assisted + IoT + TFT Touch + TIMER"));
  Serial.println(F("  Arduino UNO R4 WiFi (Renesas RA4M1, 48MHz)"));
  Serial.println(F("  TC1=Inlet | TC2=Outlet | TC3/4/5=Chamber L/C/R"));
  Serial.println(F("  SSR1=All Heaters (1125W) | SSR2=Motor (0.37kW)"));
  Serial.println(F("  Timer: 1–599 menit | Auto-shutdown saat selesai"));
  Serial.println(F("  CMD Server port 8081 | Timer via TFT touchscreen"));
  Serial.println(F("============================================================\n"));
}
