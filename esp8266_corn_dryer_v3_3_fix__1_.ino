/*
 * ============================================================================
 * CORN DRYER ESP8266 SERVER + TFT 3.5" ILI9488
 * VERSI 3.3 — FITUR TIMER PENGERINGAN + TAB PANEL KANAN
 *
 * PERUBAHAN v3.3 (dari v3.2c):
 * ─────────────────────────────────────────────────────────────────
 *  FITUR BARU: Timer Pengeringan Otomatis
 *
 *  Panel kanan kini memiliki dua TAB yang bisa disentuh:
 *    [SP]  → Panel Setpoint (seperti sebelumnya, sedikit lebih compact)
 *    [TMR] → Panel Timer (BARU)
 *
 *  Panel Timer memiliki 3 mode tampilan:
 *    IDLE  → Set durasi (menit), tombol [MULAI]
 *    RUN   → Countdown H:MM:SS, progress bar, tombol [PAUSE][STOP]
 *    DONE  → Background merah, notifikasi selesai, tombol [RESET]
 *
 *  Tombol yang disentuh → kirim HTTP GET ke Arduino:8081:
 *    Tap [MULAI]  → /cmd?timer=N  lalu /cmd?tstart=1
 *    Tap [PAUSE]  → /cmd?tstop=1
 *    Tap [STOP]   → /cmd?tstop=1  lalu /cmd?treset=1
 *    Tap [RESET]  → /cmd?treset=1
 *    Tap [+10]    → tambah 10 menit ke set value
 *    Tap [+60]    → tambah 60 menit ke set value
 *
 *  Status timer dari Arduino diterima via POST fields:
 *    timer_dur, timer_rem, timer_run, timer_done
 *
 *  Status bar: tambah indikator timer di sebelah kanan
 *    Contoh: "TMR:01:30" (hijau=jalan, kuning=pause, merah=selesai)
 *
 * ─────────────────────────────────────────────────────────────────
 * LAYOUT BARU v3.3 — Panel Kanan dengan Tab System:
 *
 *  Y:51–66   ┌─[  SP  ]─┬─[ TMR ]─┐  Tab bar 16px
 *  Y:67      ├───────────────────────┤  separator
 *  Y:68–211  │  Konten tab aktif    │  143px konten
 * ─────────────────────────────────────────────────────────────────
 *  FITUR BARU: Tombol TFT mengontrol Arduino secara langsung
 *
 *  Cara kerja:
 *  1. Arduino (v3.1) mengirimkan IP-nya di setiap POST (field arduino_ip)
 *  2. ESP8266 menyimpan IP tersebut ke variabel arduinoIP
 *  3. Saat tombol SP+/SP-/MOTOR/AUTO/MANUAL disentuh:
 *     - ESP8266 mengirim HTTP GET ke Arduino:8081
 *     - Contoh: GET /cmd?sp=80.0 → Arduino set setpoint
 *  4. Layar TFT menampilkan status pengiriman:
 *     - "SENDING..." (cyan) saat sedang kirim
 *     - "ACK OK" (hijau) jika Arduino menjawab "OK:..."
 *     - "TIMEOUT" (merah) jika tidak ada respons dalam 2 detik
 *  5. Indikator koneksi Arduino (dot pojok kanan atas) berkedip
 *     saat proses pengiriman perintah berlangsung
 * ============================================================================
 *
 * ROOT CAUSE FIX dari v3.1:
 *   MASALAH 1: "AP:1 cli" terpotong
 *              → Text "AP:1 client" dimulai X=430, overflow ke X=507 > 480
 *              → FIX: Posisi datetime direkonstruksi total, semua X dihitung
 *
 *   MASALAH 2: "Amb" row menimpa tombol
 *              → Y_AMB_ROW=207, font2=16px → text sampai Y=223
 *              → Y_BTN_DIV=209 → tombol mulai Y=211, TERTIMPA
 *              → FIX: Semua Y konten dihitung ulang presisi,
 *                     Y_AMB=196, ends Y=212, Y_BTN=214 (tidak tumpang tindih)
 *
 *   MASALAH 3: Progress bar terlalu kecil
 *              → Bar hanya 150px lebar, 8px tinggi
 *              → FIX: Bar 160px lebar, 10px tinggi, posisi presisi
 *
 * LAYOUT LAYAR 480×320 LANDSCAPE — KOORDINAT PRESISI v3.2:
 * ┌──────────────────────────────────────────────────┬──────────┐ Y:0
 * │       CORN DRYER CONTROL v3.0              [●]  │          │
 * │───────────────────────────────────────────────── │ SETPOINT │ Y:34
 * │ 10/03/26 15:55:49 WIB   ●NTP ●SHT 6/6 ●1AP    │          │ Y:35
 * │─────────────────────────────────────────────────┤   60 C   │ Y:50
 * │ >> ALIRAN UDARA:                               │          │ Y:51
 * │ TC1 Inlet  : 28.8C  [══════════░░░░░░░░░░░░░] │  +/-2C   │ Y:67
 * │ TC2 Outlet : 29.0C  [══════════░░░░░░░░░░░░░] │─────────── Y:83
 * │ DeltaT     : +0.2C  (Heater OFF?)             │  [▲ SP+]  │ Y:99
 * │───────────────────────────────────────────────── │          │ Y:115
 * │ ~~ DISTRIBUSI CHAMBER:                         │  [▼ SP-]  │ Y:116
 * │ TC3 Kiri   : 29.2C  [══════════░░░░░░░░░░░░░] │          │ Y:132
 * │ TC4 Tengah : 29.0C  [══════════░░░░░░░░░░░░░] │ Stp:1C   │ Y:148
 * │ TC5 Kanan  : 29.0C  [══════════░░░░░░░░░░░░░] │ 60~135C  │ Y:164
 * │ Avg: 29.1C   Dev: -30.9C  (terlalu dingin)    │          │ Y:180
 * │ Amb: 28.5C   Hum: 72%  Unifm: 99.8%          │          │ Y:196
 * ├──────────────────────────────────────────────────┴──────────┤ Y:212
 * │  [  MOTOR OFF  ]      [    AUTO    ]      [   MANUAL   ]   │ Y:214
 * ├─────────────────────────────────────────────────────────────┤ Y:252
 * │  ●HTR:ON  |  ●MOT:OFF  |  ●FAN:50%  |  ●SHT:OK  |  SP:60C │ Y:254
 * ├─────────────────────────────────────────────────────────────┤ Y:267
 * │ TREN: ─TC3(cyan) ─TC4(grn) ─TC5(yel) ─SP(red) [60 titik] │ Y:269
 * └─────────────────────────────────────────────────────────────┘ Y:319
 *
 * KALKULASI VERTIKAL (semua presisi, tidak ada tumpang tindih):
 *   Header   : Y:0-33   (34px)
 *   Border   : Y:34     (1px)
 *   DateTime : Y:35-49  (15px: 3+8+4 = pad+font1+pad)
 *   Divider  : Y:50     (1px)
 *   Content  : Y:51-211 (161px = 10×16px + 1px divider)
 *     AIR_LBL: Y:51-66  (16px font2)
 *     TC1    : Y:67-82  (16px font2)
 *     TC2    : Y:83-98  (16px font2)
 *     DELTA  : Y:99-114 (16px font2)
 *     DIV_MID: Y:115    (1px)
 *     CHM_LBL: Y:116-131(16px font2)
 *     TC3    : Y:132-147(16px font2)
 *     TC4    : Y:148-163(16px font2)
 *     TC5    : Y:164-179(16px font2)
 *     AVG    : Y:180-195(16px font2) ← AVG_ROW+16=196 < DIV_CONT=212 ✓
 *     AMB    : Y:196-211(16px font2) ← AMB_ROW+16=212 < BTN_TOP=214 ✓
 *   Divider  : Y:212    (1px)
 *   DivThick : Y:213    (1px garis tebal)
 *   Buttons  : Y:214-251(38px: 3+32+3)
 *   Divider  : Y:252    (1px)
 *   Status   : Y:254-266(13px font1)
 *   Divider  : Y:267    (1px)
 *   Graph    : Y:269-319(51px)
 *   TOTAL    : 320px ✓
 * ============================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <time.h>

// ══════════════════════════════════════════════════════════════
// OBJEK GLOBAL
// ══════════════════════════════════════════════════════════════
TFT_eSPI         tft = TFT_eSPI();
ESP8266WebServer server(80);

// ══════════════════════════════════════════════════════════════
// PALET WARNA RGB565
// ══════════════════════════════════════════════════════════════
#define C_HEADER    0x0010    // Navy
#define C_AIRFLOW   0xFD20    // Deep Orange → label ALIRAN UDARA
#define C_TC1       0xFBE0    // Light Orange → TC1
#define C_TC2       0xFCA0    // Dark Orange  → TC2
#define C_DELTA_POS 0x07E0    // Green  → Delta T positif
#define C_DELTA_NEG 0xFD40    // Orange → Delta T negatif
#define C_CHAMBER   0x077F    // Cyan tua → label CHAMBER
#define C_TC3       0x07FF    // Cyan   → TC3
#define C_TC4       0x57E0    // Lt Green → TC4
#define C_TC5       0xFFE0    // Yellow → TC5
#define C_AVG       0x87E0    // Lt Green → Avg
#define C_AMB       0xFEE0    // Lt Yellow → Ambient
#define C_HUM       0x467F    // Lt Blue  → Humidity
#define C_UNIF      0x9FFF    // Lt Cyan → Uniformity
#define C_SP_BG     0x0863    // Dark teal → panel setpoint
#define C_SP_VAL    0x07E0    // Green → nilai SP
#define C_SP_BTN    0x0BFF    // Cyan  → tombol SP
#define C_BTN_ON    0x07E0    // Green  → aktif/ON
#define C_BTN_OFF   0xF800    // Red    → tidak aktif/OFF
#define C_BTN_AUTO  0x07E0    // Green  → AUTO aktif
#define C_BTN_MAN   0xFD40    // Orange → MANUAL aktif
#define C_BTN_IDLE  0x39E7    // Dark   → tombol tidak dipilih
#define C_BAR_BG    0x18C3    // Dark bar bg
#define C_DIVIDER   0x4228    // Garis divider
#define C_NTP_OK    0x07E0
#define C_NTP_FAIL  0xF800
#define C_SHT_OK    0x07E0
#define C_G_TC3     0x07FF
#define C_G_TC4     0x07E0
#define C_G_TC5     0xFFE0
#define C_G_SP      0xF800
#define C_EMERG     0xF800
#define C_DEV_OK    0x07E0    // Green  → deviasi kecil
#define C_DEV_WARN  0xFFE0    // Yellow → deviasi sedang
#define C_DEV_BAD   0xF800    // Red    → deviasi besar
// Timer colors — BARU v3.3
#define C_TMR_BG    0x0841    // Dark navy  → background panel timer
#define C_TMR_IDLE  0xC618    // Grey       → timer IDLE
#define C_TMR_RUN   0x07E0    // Green      → timer BERJALAN
#define C_TMR_PAUSE 0xFFE0    // Yellow     → timer PAUSE
#define C_TMR_DONE  0xF800    // Red        → timer SELESAI
#define C_TMR_BTN   0x0BFF    // Cyan       → tombol timer
#define C_TMR_STOP  0xF800    // Red        → tombol STOP
#define C_TAB_ACT   0x0863    // Active tab bg (sama dengan SP_BG)
#define C_TAB_IDLE  0x2124    // Inactive tab bg (dark grey)

// ══════════════════════════════════════════════════════════════
// LAYOUT CONSTANTS — PRESISI PIKSEL v3.2
// ══════════════════════════════════════════════════════════════

// Kolom (X-axis)
#define SCR_W       480      // Lebar layar total
#define SCR_H       320      // Tinggi layar total
#define LX_W        360      // Lebar kolom kiri  (X: 0–359)
#define RX          360      // Kolom kanan mulai (X: 360–479)
#define RX_W        120      // Lebar kolom kanan
#define RX_MID      420      // Tengah kolom kanan (360 + 60)

// Posisi teks di kolom kiri
#define TX_LABEL      4      // X label TC (font2)
#define TX_VALUE    120      // X nilai sensor (font2)
// Progress bar: mulai X=192, width=163 → ends X=355, gap 4px ke divider
#define BAR_X       192
#define BAR_W       163
#define BAR_H        10
#define BAR_YO        3      // Y offset dalam baris 16px (3+10+3=16)

// Zona vertikal (Y-axis)
// ── Header ──────────────────────────────────────────────────
#define Y_HDR_TOP     0
#define Y_HDR_H      34      // Header tinggi
#define Y_HDR_BOT    33      // Baris terakhir header
#define Y_HDR_LINE   34      // Garis bawah header

// ── Info Bar ─────────────────────────────────────────────────
#define Y_DT          35     // DateTime bar (3px pad + 8px font1 + 4px pad = 15px)
#define Y_DT_H        15
#define Y_DT_BOT      49
#define Y_DIV_DT      50     // Divider tipis bawah datetime

// ── Konten Sensor (10 baris × 16px + 1 divider tengah = 161px) ──
#define Y_AIR_LBL     51     // "ALIRAN UDARA"          → Y:51-66
#define Y_TC1         67     // TC1 Inlet               → Y:67-82
#define Y_TC2         83     // TC2 Outlet              → Y:83-98
#define Y_DLT         99     // Delta T                 → Y:99-114
#define Y_DIV_MID    115     // Garis pemisah tengah (1px)
#define Y_CHM_LBL    116     // "DISTRIBUSI CHAMBER"    → Y:116-131
#define Y_TC3        132     // TC3 Kiri                → Y:132-147
#define Y_TC4        148     // TC4 Tengah              → Y:148-163
#define Y_TC5        164     // TC5 Kanan               → Y:164-179
#define Y_AVG        180     // Avg + Deviasi           → Y:180-195
#define Y_AMB        196     // Amb + Hum + Uniformity  → Y:196-211
// VERIFIKASI: Y_AMB(196) + 16px = 212 ≤ Y_DIV_CONT(212) ✓

// ── Zona Full-Width ──────────────────────────────────────────
#define Y_DIV_CONT   212     // Divider tebal bawah konten
#define Y_BTN_TOP    214     // Tombol kontrol atas
#define Y_BTN_H       38     // Tinggi tombol
#define Y_BTN_BOT    251     // Tombol kontrol bawah (214+38-1=251)
#define Y_DIV_STS    252     // Divider atas status bar
#define Y_STS        254     // Status bar (font1=8px, 2.5px pad atas)
#define Y_STS_H       13
#define Y_DIV_GRF    267     // Divider atas grafik
#define Y_GRF_TOP    269     // Grafik atas
#define Y_GRF_H       51     // Tinggi grafik (269+51=320 ✓)
#define Y_GRF_BOT    319

// ── Panel Kanan — Tab System v3.3 ──────────────────────────
//  Tab row : Y:51–66 (16px), split X:360-419 = [SP], X:420-479 = [TMR]
//  Content : Y:68–211 (143px)
#define Y_TAB_ROW   51       // Tab row Y mulai
#define Y_TAB_H     16       // Tinggi tab
#define Y_PANEL_TOP 68       // Konten panel mulai (setelah tab + separator)
#define X_TAB_SP    360      // SP tab X start
#define X_TAB_TMR   420      // TMR tab X start
#define TAB_W        60      // Lebar satu tab
#define TAB_MID_SP  390      // SP tab center X
#define TAB_MID_TMR 450      // TMR tab center X

// ── Panel SP — Compressed (-15px dari v3.2b untuk beri ruang tab) ─
//  Available: Y:68–211 = 143px
//  Y=70  "SETPOINT"  font1  8px → ends=78   [pad 2]
//  Y=81  nilai SP    font4 26px → ends=107  [gap 3]
//  Y=110 "C"         font2 16px → ends=126  [gap 3]
//  Y=128 "+/-xC"     font1  8px → ends=136  [gap 2]
//  Y=139 separator                           [gap 3]
//  Y=142 SP+ btn H=28px         → ends=170  [gap 3]
//  Y=173 SP- btn H=28px         → ends=201  [gap 3]
//  Y=203 info font1  8px        → ends=211  [gap 2] ✓
#define Y_SP_LBL    70
#define Y_SP_VAL    81
#define Y_SP_UNIT  110
#define Y_SP_HYS   128
#define Y_SP_DIV   139
#define Y_SP_BTN1  142
#define Y_SP_BTN_H  28
#define Y_SP_BTN2  173
#define Y_SP_INFO  203

// ── Panel TMR — Layout per Mode ─────────────────────────────
//  Available: Y:68–211 = 143px
//
//  IDLE / SET mode:
//  Y=70  "DURASI"  font1 8px → ends=78
//  Y=80  [▲ +]    H=24px    → ends=104
//  Y=106 nilai     font4 26px→ ends=132  (max "599" = 3char ~42px < 120px ✓)
//  Y=134 "MENIT"  font1 8px → ends=142
//  Y=144 [▼ -]    H=24px    → ends=168
//  Y=170 separator
//  Y=172 [+10] [+60] H=18px → ends=190
//  Y=192 [MULAI]  H=19px    → ends=211 ✓
//
//  RUNNING / PAUSE mode:
//  Y=70  label     font1 8px → ends=78
//  Y=80  H:MM:SS   font4 26px→ ends=106
//  Y=108 prog bar  H=8px     → ends=116
//  Y=118 status    font1 8px → ends=126
//  Y=128 separator
//  Y=131 [PAUSE/RESUME] H=24 → ends=155
//  Y=158 [STOP]    H=24px    → ends=182
//  Y=184 separator
//  Y=186 info1     font1 8px → ends=194
//  Y=196 info2     font1 8px → ends=204 ✓
//
//  DONE mode:
//  Y=72  "SELESAI!"font2 16px→ ends=88
//  Y=90  "Heater+Motor" f1   → ends=98
//  Y=100 "sudah MATI"   f1   → ends=108
//  Y=112 separator
//  Y=115 [RESET]   H=28px    → ends=143
#define Y_TMR_LBL    70
#define Y_TMR_UP     80
#define Y_TMR_BTN_H  24
#define Y_TMR_VAL   106
#define Y_TMR_UNIT  134
#define Y_TMR_DN    144
#define Y_TMR_SEP1  170
#define Y_TMR_QUICK 172
#define Y_TMR_QUICK_H 18
#define Y_TMR_START 192
#define Y_TMR_START_H 19
// Running mode specifics
#define Y_TMR_CD     80
#define Y_TMR_BAR   108
#define Y_TMR_BAR_H   8
#define Y_TMR_STS   118
#define Y_TMR_SEP2  128
#define Y_TMR_PAUSE 131
#define Y_TMR_STOP  158
#define Y_TMR_SEP3  184
#define Y_TMR_INF1  186
#define Y_TMR_INF2  196
// Done mode specifics
#define Y_TMR_DONE_LBL 72
#define Y_TMR_DONE_TX1 90
#define Y_TMR_DONE_TX2 100
#define Y_TMR_DONE_SEP 112
#define Y_TMR_RESET    115
#define Y_TMR_RESET_H   28

// Tombol kontrol (3 tombol, full width 480px, Y=214, H=38)
// Distribusi merata: 3 tombol, gap=4px
// btn_w = (480 - 4×4) / 3 = 464/3 = 154px per tombol
#define BTN_M_X       4      // MOTOR  : X=4
#define BTN_M_W     154      // → ends X=158
#define BTN_A_X     162      // AUTO   : X=162
#define BTN_A_W     154      // → ends X=316
#define BTN_N_X     320      // MANUAL : X=320
#define BTN_N_W     156      // → ends X=476 (4px margin)

// ══════════════════════════════════════════════════════════════
// KONFIGURASI
// ══════════════════════════════════════════════════════════════
const char* GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/YOUR_SCRIPT_ID_HERE/exec";
const char* ap_ssid      = "ESP8266_Server";
const char* ap_password  = "12345678";
const char* sta_ssid     = "NAMA_WIFI_ROUTER";
const char* sta_password = "PASSWORD_WIFI";

const char*  ntpServer1         = "pool.ntp.org";
const char*  ntpServer2         = "time.google.com";
const long   gmtOffset_sec      = 7 * 3600;
const int    daylightOffset_sec = 0;
bool         ntpSynced          = false;
unsigned long lastNtpSync       = 0;

// ══════════════════════════════════════════════════════════════
// DATA SENSOR & STATUS
// ══════════════════════════════════════════════════════════════
float tc1=0,tc2=0,tc3=0,tc4=0,tc5=0;
float temp_avg=0, delta_temp=0;
float temp_amb=0, hum_amb=0;
float setpoint=60.0f, hysteresis=2.0f;

bool   heater_on  = false;
bool   motor_on   = false;
int    fan_spd    = 128;
String ctrl_mode  = "AUTO";
bool   emg_stop   = false;

// Riwayat grafik (60 titik)
#define HIST_LEN 60
float h_tc3[HIST_LEN], h_tc4[HIST_LEN], h_tc5[HIST_LEN];
int   hist_idx = 0;

// Status koneksi
bool          dataReceived  = false;
unsigned long lastDataTime  = 0;
bool          sheetsOK      = false;
int           sheetsOK_cnt  = 0;
int           sheetsFail_cnt= 0;
unsigned long lastSheets    = 0;
const unsigned long SHEETS_INTERVAL = 60000UL;

// ── BARU v3.2c: Koneksi balik ke Arduino ─────────────────────────
IPAddress     arduinoIP;              // IP Arduino (diisi dari POST field arduino_ip)
bool          arduinoIPknown = false; // Sudah tahu IP Arduino?
const int     ARDUINO_CMD_PORT = 8081;

// Status pengiriman perintah (untuk tampilan ACK di layar)
enum CmdStatus { CMD_IDLE, CMD_SENDING, CMD_ACK_OK, CMD_TIMEOUT, CMD_NO_IP };
CmdStatus     cmdStatus      = CMD_IDLE;
unsigned long cmdStatusTime  = 0;
const unsigned long CMD_ACK_DISPLAY_MS = 2500;

// ── BARU v3.3: Timer state (dari Arduino via POST) ───────────────
uint32_t timerDuration  = 0;      // Durasi yang di-set (detik)
uint32_t timerRemaining = 0;      // Sisa waktu (detik)
bool     timerRunning   = false;  // Sedang berjalan
bool     timerDone      = false;  // Sudah selesai

// ── Tab panel kanan & setting timer lokal ───────────────────────
bool rightTabTMR    = false;   // false=tab SP, true=tab TMR
int  timerSetMin    = 60;      // Menit yang akan di-set (lokal UI, 1–599)
bool timerDoneNotif = false;   // Notifikasi timer selesai sedang tampil
unsigned long timerDoneNotifTime = 0;
// ── Guard flag: abaikan timer_done=1 selama N detik setelah RESET dikirim ──
// Mencegah re-trigger overlay saat Arduino belum memproses treset
bool          timerResetGuard     = false;
unsigned long timerResetGuardTime = 0;
const unsigned long TIMER_RESET_GUARD_MS = 6000; // 6 detik grace period

// ── Forward declarations (diperlukan karena urutan definisi) ──
void drawTimerDoneAlert();
void drawSensorArea();
void drawRightPanel();
void drawPanelSP();
void drawPanelTMR();
void drawStatusBar();
void drawButtons();
void drawGraph();
void drawInfoBar();
void drawStatic();
void updateCmdStatus();
void updateTimerDoneNotif();

// ══════════════════════════════════════════════════════════════
// SETUP
// ══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== CORN DRYER ESP8266 v3.2 ==="));

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Touch calibration (sesuaikan dengan layar Anda)
  uint16_t cal[5] = { 275, 3620, 264, 3532, 1 };
  tft.setTouch(cal);

  // Init riwayat grafik
  for (int i=0; i<HIST_LEN; i++) {
    h_tc3[i]=25.0f; h_tc4[i]=25.0f; h_tc5[i]=25.0f;
  }

  showSplash();

  // WiFi: AP + STA bersamaan
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  delay(500);
  Serial.print(F("AP IP: ")); Serial.println(WiFi.softAPIP());

  WiFi.begin(sta_ssid, sta_password);
  Serial.print(F("Menghubungkan ke WiFi"));
  int tries=0;
  while (WiFi.status()!=WL_CONNECTED && tries<25) {
    delay(400); Serial.print('.'); tries++;
  }
  Serial.println();

  if (WiFi.status()==WL_CONNECTED) {
    Serial.print(F("STA IP: ")); Serial.println(WiFi.localIP());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    time_t now=time(nullptr); int s=0;
    while (now<86400 && s<25) { delay(500); now=time(nullptr); s++; }
    if (now > 86400) { ntpSynced=true; lastNtpSync=millis(); }
    Serial.println(ntpSynced ? F("NTP OK") : F("NTP GAGAL"));
  }

  server.on("/data",   HTTP_POST, handlePost);
  server.on("/data",   HTTP_GET,  handleGet);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println(F("HTTP Server aktif port 80"));

  tft.fillScreen(TFT_BLACK);
  drawStatic();
  updateDynamic();
}

// ══════════════════════════════════════════════════════════════
// MAIN LOOP
// ══════════════════════════════════════════════════════════════
void loop() {
  server.handleClient();

  // Re-sync NTP setiap 1 jam
  if (WiFi.status()==WL_CONNECTED && ntpSynced &&
      millis()-lastNtpSync > 3600000UL) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
    lastNtpSync = millis();
  }

  // Kirim ke Google Sheets setiap 60 detik
  if (WiFi.status()==WL_CONNECTED && dataReceived && ntpSynced &&
      millis()-lastSheets >= SHEETS_INTERVAL) {
    sendSheets();
    lastSheets = millis();
  }

  handleTouch();

  // Update layar setiap 1 detik
  static unsigned long lastUpd = 0;
  if (millis()-lastUpd >= 1000) {
    updateDynamic();
    lastUpd = millis();
  }
  delay(10);
}

// ══════════════════════════════════════════════════════════════
// HTTP HANDLERS
// ══════════════════════════════════════════════════════════════
void handlePost() {
  if (!dataReceived) {
    dataReceived = true;
    Serial.println(F("[DATA] Koneksi Arduino pertama OK!"));
  }
  lastDataTime = millis();

  auto getF = [&](const char* k, float def=0.0f) -> float {
    return server.hasArg(k) ? server.arg(k).toFloat() : def;
  };
  auto getI = [&](const char* k, int def=0) -> int {
    return server.hasArg(k) ? server.arg(k).toInt() : def;
  };
  auto getS = [&](const char* k, String def="") -> String {
    return server.hasArg(k) ? server.arg(k) : def;
  };

  tc1        = getF("tc1");
  tc2        = getF("tc2");
  tc3        = getF("tc3");
  tc4        = getF("tc4");
  tc5        = getF("tc5");
  temp_avg   = getF("temp_avg");
  delta_temp = getF("delta_temp");
  temp_amb   = getF("temp_ambient");
  hum_amb    = getF("humidity");
  setpoint   = getF("setpoint", 60.0f);
  hysteresis = getF("hysteresis", 2.0f);
  heater_on  = getI("heater");
  motor_on   = getI("motor");
  fan_spd    = getI("fan_speed", 128);
  ctrl_mode  = getS("mode", "AUTO");
  emg_stop   = getI("emergency");

  // ── v3.3: Parse timer fields dari Arduino ─────────────────────
  timerDuration  = (uint32_t)server.arg("timer_dur").toInt();
  timerRemaining = (uint32_t)server.arg("timer_rem").toInt();
  timerRunning   = server.arg("timer_run").toInt() == 1;
  bool prevDone  = timerDone;
  timerDone      = server.arg("timer_done").toInt() == 1;

  // Aktifkan guard jika sudah kadaluarsa
  if (timerResetGuard &&
      millis() - timerResetGuardTime > TIMER_RESET_GUARD_MS) {
    timerResetGuard = false;
  }

  // Trigger notifikasi HANYA jika:
  // 1. Timer baru saja berubah dari false → true (transisi pertama kali)
  // 2. Tidak sedang dalam guard window (setelah RESET)
  if (!prevDone && timerDone && !timerResetGuard) {
    timerDoneNotif     = true;
    timerDoneNotifTime = millis();
    rightTabTMR        = true;   // Otomatis pindah ke tab TMR
    drawTimerDoneAlert();
  }

  // BARU v3.2c: Simpan IP Arduino dari field arduino_ip
  if (server.hasArg("arduino_ip")) {
    String ipStr = server.arg("arduino_ip");
    if (ipStr.length() > 6 && ipStr != "0.0.0.0") {
      if (arduinoIP.fromString(ipStr)) {
        if (!arduinoIPknown) {
          arduinoIPknown = true;
          Serial.print(F("[CMD] IP Arduino tersimpan: "));
          Serial.println(arduinoIP);
        }
      }
    }
  }
  // Fallback: gunakan IP pengirim POST jika arduino_ip tidak ada
  if (!arduinoIPknown) {
    arduinoIP    = server.client().remoteIP();
    arduinoIPknown = true;
    Serial.print(F("[CMD] IP Arduino (dari client): "));
    Serial.println(arduinoIP);
  }

  // Simpan ke riwayat grafik
  h_tc3[hist_idx] = tc3;
  h_tc4[hist_idx] = tc4;
  h_tc5[hist_idx] = tc5;
  hist_idx = (hist_idx + 1) % HIST_LEN;

  server.send(200, "text/plain", "OK");
}

void handleGet() {
  char j[180];
  snprintf(j, sizeof(j),
    "{\"tc1\":%.1f,\"tc2\":%.1f,\"tc3\":%.1f,\"tc4\":%.1f,\"tc5\":%.1f,"
    "\"avg\":%.1f,\"delta\":%.1f,\"sp\":%.1f,\"htr\":%d,\"mot\":%d}",
    tc1,tc2,tc3,tc4,tc5,temp_avg,delta_temp,setpoint,
    heater_on?1:0, motor_on?1:0);
  server.send(200, "application/json", j);
}

void handleStatus() {
  String h = F("<html><body style='font-family:monospace;background:#111;color:#0f0'>");
  h += F("<h2 style='color:#ff0'>Corn Dryer v3.2</h2><table border=1 style='border-color:#0f0'>");
  char row[80];
  auto tr = [&](const char* k, const char* v) {
    snprintf(row, sizeof(row),
      "<tr><td style='padding:4px'>%s</td><td style='padding:4px;color:#fff'>%s</td></tr>", k, v);
    h += row;
  };
  char buf[20];
  snprintf(buf,sizeof(buf),"%.1f C", tc1);   tr("TC1 Inlet", buf);
  snprintf(buf,sizeof(buf),"%.1f C", tc2);   tr("TC2 Outlet", buf);
  snprintf(buf,sizeof(buf),"%+.2f C",delta_temp); tr("Delta T", buf);
  snprintf(buf,sizeof(buf),"%.1f C", tc3);   tr("TC3 Kiri", buf);
  snprintf(buf,sizeof(buf),"%.1f C", tc4);   tr("TC4 Tengah", buf);
  snprintf(buf,sizeof(buf),"%.1f C", tc5);   tr("TC5 Kanan", buf);
  snprintf(buf,sizeof(buf),"%.2f C", temp_avg); tr("Avg Chamber", buf);
  snprintf(buf,sizeof(buf),"%.1f C", setpoint); tr("Setpoint", buf);
  h += F("</table></body></html>");
  server.send(200, "text/html", h);
}

// ══════════════════════════════════════════════════════════════
// NOTIFIKASI TIMER SELESAI — FIX v3.3.1
// ══════════════════════════════════════════════════════════════
// PERBAIKAN:
// - Overlay TIDAK lagi menimpa panel sensor kiri (panel kiri ≤ X=359)
// - Notifikasi ditampilkan HANYA di header bar (Y:0–33) sebagai
//   text berkedip, dan tab TMR otomatis aktif (sudah merah di sana)
// - Auto-clear setelah 4 detik melalui updateTimerDoneNotif()
//
void drawTimerDoneAlert() {
  // ── 1. Gambar banner di header (Y:0-33) — area yang sudah milik header
  //       Timpa sebagian teks header dengan background merah
  //       Hanya di X:120–340 agar tidak menimpa jam dan dot kanan
  tft.fillRect(120, 2, 220, 30, C_TMR_DONE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, C_TMR_DONE);
  tft.drawString("!! TIMER SELESAI !!", 230, 17, 2);
  tft.setTextDatum(TL_DATUM);

  // ── 2. Garis tebal merah di bawah header sebagai penanda
  tft.fillRect(0, 34, SCR_W, 3, C_TMR_DONE);

  // ── 3. Tab TMR sudah menampilkan "SELESAI!" dengan bg merah
  //       (drawRightPanel → drawPanelTMR → mode DONE)
  //       Tidak perlu overlay tambahan di panel kiri.
  rightTabTMR = true;
  drawRightPanel();   // Panel kanan langsung tampil mode DONE
}

// Auto-bersih notifikasi header setelah 4 detik
void updateTimerDoneNotif() {
  if (!timerDoneNotif) return;
  if (millis() - timerDoneNotifTime > 4000) {
    timerDoneNotif = false;
    // Redraw header untuk hapus banner merah "!! TIMER SELESAI !!"
    // (drawInfoBar menggambar ulang seluruh baris info bar Y:35–49,
    //  tapi header Y:0–33 perlu di-redraw secara manual)
    tft.fillRect(120, 2, 220, 30, C_HEADER);   // Hapus banner merah
    // Tulis ulang teks header yang tertimpa
    tft.setTextColor(TFT_WHITE, C_HEADER);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("CORN DRYER CONTROL v3.0", 10, 17, 4);
    tft.setTextDatum(TL_DATUM);
    // Hapus garis merah di Y:34-36, kembalikan ke warna normal
    tft.fillRect(0, 34, SCR_W, 3, C_DIVIDER);
    // Panel sensor kiri tidak perlu di-redraw (tidak tersentuh overlay baru)
  }
}

// ══════════════════════════════════════════════════════════════
// KIRIM PERINTAH KE ARDUINO
// ══════════════════════════════════════════════════════════════
//
//  params : query string, contoh "sp=80.0", "mode=AUTO", "motor=1"
//
//  Alur:
//  1. Tampilkan "SENDING..." di layar
//  2. Kirim HTTP GET ke Arduino IP:8081/cmd?params
//  3. Tunggu respons maksimal 2 detik
//  4. Parse "OK:..." atau "ERR:..." → tampilkan ACK di layar
//
void sendCmdToArduino(const String& params) {
  // Update status layar: SENDING
  cmdStatus     = CMD_SENDING;
  cmdStatusTime = millis();
  drawCmdStatus();   // Langsung tampilkan (tidak tunggu update loop)

  if (!arduinoIPknown) {
    Serial.println(F("[CMD] IP Arduino belum diketahui!"));
    cmdStatus     = CMD_NO_IP;
    cmdStatusTime = millis();
    drawCmdStatus();
    return;
  }

  Serial.print(F("[CMD] Kirim ke Arduino ")); Serial.print(arduinoIP);
  Serial.print(F(":8081/cmd?")); Serial.println(params);

  WiFiClient ac;
  ac.setTimeout(2000);

  if (!ac.connect(arduinoIP, ARDUINO_CMD_PORT)) {
    Serial.println(F("[CMD] Gagal terhubung ke Arduino!"));
    cmdStatus     = CMD_TIMEOUT;
    cmdStatusTime = millis();
    drawCmdStatus();
    return;
  }

  // Kirim HTTP GET
  String req  = "GET /cmd?" + params + " HTTP/1.1\r\n";
  req        += "Host: " + arduinoIP.toString() + "\r\n";
  req        += "Connection: close\r\n\r\n";
  ac.print(req);

  // Tunggu respons (maks 2 detik) — yield() cegah WDT reset ESP8266
  unsigned long tWait = millis();
  while (!ac.available() && millis()-tWait < 2000) {
    delay(10);
    yield();  // ← wajib di ESP8266 agar watchdog tidak reset
  }

  String resp = "";
  while (ac.available()) resp += (char)ac.read();
  ac.stop();

  // Parse: cari body setelah \r\n\r\n
  int bodyIdx = resp.indexOf("\r\n\r\n");
  String body = (bodyIdx >= 0) ? resp.substring(bodyIdx+4) : resp;
  body.trim();

  Serial.print(F("[CMD] Respons Arduino: ")); Serial.println(body);

  if (body.startsWith("OK") || body.startsWith("PONG")) {
    cmdStatus = CMD_ACK_OK;
  } else {
    cmdStatus = CMD_TIMEOUT;
  }
  cmdStatusTime = millis();
  drawCmdStatus();
}

// Gambar indikator status CMD di area kecil atas status bar
// Y area: Y_DIV_CONT+1 sampai Y_BTN_TOP-1 (Y=213, 1px)
// Karena area ini hanya 1px tipis, kita tampilkan di overlay kecil
// di bagian atas tombol (Y=214, 12px)
void drawCmdStatus() {
  // Tampilkan di baris sempit antara divider dan tombol (Y=213, h=1px tidak cukup)
  // Gunakan baris status bar sebagai tempat notifikasi sementara
  // Hapus hanya zona X=380-479, Y=Y_STS sampai Y_STS+Y_STS_H
  int nx = 380, ny = Y_STS, nw = 100, nh = Y_STS_H;
  tft.fillRect(nx, ny, nw, nh, TFT_BLACK);

  if (cmdStatus == CMD_IDLE) return;  // Bersih, tidak perlu gambar

  // Warna dan teks sesuai status
  uint16_t col; const char* msg;
  switch (cmdStatus) {
    case CMD_SENDING: col=TFT_CYAN;    msg=">> SENDING"; break;
    case CMD_ACK_OK:  col=TFT_GREEN;   msg="ACK OK";     break;
    case CMD_TIMEOUT: col=TFT_RED;     msg="TIMEOUT!";   break;
    case CMD_NO_IP:   col=TFT_YELLOW;  msg="IP UNKNOWN"; break;
    default:          return;
  }

  tft.setTextColor(col, TFT_BLACK);
  tft.drawString(msg, nx+2, ny+2, 1);

  // Bersihkan status setelah beberapa detik (dilakukan di loop)
}

// Dipanggil di updateDynamic() untuk auto-clear status CMD
void updateCmdStatus() {
  if (cmdStatus == CMD_IDLE) return;
  if (millis() - cmdStatusTime > CMD_ACK_DISPLAY_MS) {
    cmdStatus = CMD_IDLE;
    // Redraw status bar untuk bersihkan area notifikasi
    drawStatusBar();
  }
}

// ══════════════════════════════════════════════════════════════
// TOUCH HANDLER
// ══════════════════════════════════════════════════════════════
bool hitBox(int bx, int by, int bw, int bh, uint16_t tx, uint16_t ty) {
  return (tx>=bx && tx<=bx+bw && ty>=by && ty<=by+bh);
}

void handleTouch() {
  uint16_t tx, ty;
  if (!tft.getTouch(&tx, &ty)) return;

  static unsigned long lastTouch = 0;
  if (millis()-lastTouch < 280) return;
  lastTouch = millis();

  // ── TAB SP / TMR ─────────────────────────────────────────────
  if (hitBox(X_TAB_SP, Y_TAB_ROW, TAB_W, Y_TAB_H, tx, ty)) {
    if (rightTabTMR) { rightTabTMR = false; drawRightPanel(); }
    return;
  }
  if (hitBox(X_TAB_TMR, Y_TAB_ROW, TAB_W, Y_TAB_H, tx, ty)) {
    if (!rightTabTMR) { rightTabTMR = true; drawRightPanel(); }
    return;
  }

  // ── Tab SP aktif — Setpoint controls ─────────────────────────
  if (!rightTabTMR) {
    if (hitBox(RX+3, Y_SP_BTN1, RX_W-6, Y_SP_BTN_H, tx, ty)) {
      float step = (setpoint >= 100.0f) ? 5.0f : 1.0f;
      setpoint = constrain(setpoint + step, 60.0f, 135.0f);
      drawRightPanel(); drawGraph();
      char spBuf[10]; sprintf(spBuf, "%.1f", setpoint);
      sendCmdToArduino("sp=" + String(spBuf));
    }
    else if (hitBox(RX+3, Y_SP_BTN2, RX_W-6, Y_SP_BTN_H, tx, ty)) {
      float step = (setpoint > 100.0f) ? 5.0f : 1.0f;
      setpoint = constrain(setpoint - step, 60.0f, 135.0f);
      drawRightPanel(); drawGraph();
      char spBuf[10]; sprintf(spBuf, "%.1f", setpoint);
      sendCmdToArduino("sp=" + String(spBuf));
    }
  }

  // ── Tab TMR aktif — Timer controls ───────────────────────────
  else {
    // Tentukan state timer saat ini
    bool timerPaused = (!timerRunning && !timerDone &&
                        timerDuration > 0 && timerRemaining > 0 &&
                        timerRemaining < timerDuration);

    if (timerDone) {
      // ── Mode DONE: hanya tombol RESET ──
      if (hitBox(RX+3, Y_TMR_RESET, RX_W-6, Y_TMR_RESET_H, tx, ty)) {
        sendCmdToArduino("treset=1");

        // ── FIX: Aktifkan guard agar POST berikutnya yang masih
        //         membawa timer_done=1 tidak memicu overlay lagi ──
        timerResetGuard     = true;
        timerResetGuardTime = millis();

        // Reset state lokal
        timerDone      = false;
        timerRunning   = false;
        timerRemaining = timerDuration;

        // Bersihkan notifikasi yang mungkin masih aktif
        timerDoneNotif = false;

        // Hapus sisa banner merah di header (jika masih ada)
        tft.fillRect(120, 2, 220, 30, C_HEADER);
        tft.setTextColor(TFT_WHITE, C_HEADER);
        tft.setTextDatum(ML_DATUM);
        tft.drawString("CORN DRYER CONTROL v3.0", 10, 17, 4);
        tft.setTextDatum(TL_DATUM);
        tft.fillRect(0, 34, SCR_W, 3, C_DIVIDER);

        // Redraw panel kanan (keluar dari mode DONE → mode IDLE)
        drawRightPanel();
      }
    }
    else if (timerRunning || timerPaused) {
      // ── Mode RUNNING atau PAUSED ──
      // [PAUSE] saat running → PAUSE; [RESUME] saat paused → lanjut
      if (hitBox(RX+3, Y_TMR_PAUSE, RX_W-6, Y_TMR_BTN_H, tx, ty)) {
        if (timerRunning) {
          sendCmdToArduino("tstop=1");
          timerRunning = false;
        } else {
          // RESUME dari kondisi pause
          sendCmdToArduino("tstart=1");
          timerRunning = true;
        }
        drawRightPanel();
      }
      // [STOP] → pause + reset ke durasi awal
      else if (hitBox(RX+3, Y_TMR_STOP, RX_W-6, Y_TMR_BTN_H, tx, ty)) {
        sendCmdToArduino("tstop=1");
        delay(200);
        sendCmdToArduino("treset=1");
        timerRunning   = false;
        timerRemaining = timerDuration;
        drawRightPanel();
      }
    }
    else {
      // ── Mode IDLE / SET ──
      // [▲ tambah 1 menit]
      if (hitBox(RX+3, Y_TMR_UP, RX_W-6, Y_TMR_BTN_H, tx, ty)) {
        timerSetMin = constrain(timerSetMin + 1, 1, 599);
        drawRightPanel();
      }
      // [▼ kurangi 1 menit]
      else if (hitBox(RX+3, Y_TMR_DN, RX_W-6, Y_TMR_BTN_H, tx, ty)) {
        timerSetMin = constrain(timerSetMin - 1, 1, 599);
        drawRightPanel();
      }
      // [+10 menit]
      else if (hitBox(RX+3, Y_TMR_QUICK, (RX_W-9)/2, Y_TMR_QUICK_H, tx, ty)) {
        timerSetMin = constrain(timerSetMin + 10, 1, 599);
        drawRightPanel();
      }
      // [+60 menit]
      else if (hitBox(RX+3+(RX_W-9)/2+3, Y_TMR_QUICK, (RX_W-9)/2, Y_TMR_QUICK_H, tx, ty)) {
        timerSetMin = constrain(timerSetMin + 60, 1, 599);
        drawRightPanel();
      }
      // [MULAI] → kirim set lalu start
      else if (hitBox(RX+3, Y_TMR_START, RX_W-6, Y_TMR_START_H, tx, ty)) {
        char buf[20];
        sprintf(buf, "timer=%d", timerSetMin);
        sendCmdToArduino(String(buf));
        delay(300);
        sendCmdToArduino("tstart=1");
        timerDuration  = (uint32_t)timerSetMin * 60UL;
        timerRemaining = timerDuration;
        timerRunning   = true;
        timerDone      = false;
        drawRightPanel();
      }
    }
  }

  // ── Tombol MOTOR (full-width area bawah) ─────────────────────
  if (hitBox(BTN_M_X, Y_BTN_TOP, BTN_M_W, Y_BTN_H, tx, ty)) {
    motor_on = !motor_on;
    drawButtons(); drawStatusBar();
    sendCmdToArduino("motor=" + String(motor_on ? "1" : "0"));
  }
  else if (hitBox(BTN_A_X, Y_BTN_TOP, BTN_A_W, Y_BTN_H, tx, ty)) {
    ctrl_mode = "AUTO";
    drawButtons();
    sendCmdToArduino("mode=AUTO");
  }
  else if (hitBox(BTN_N_X, Y_BTN_TOP, BTN_N_W, Y_BTN_H, tx, ty)) {
    ctrl_mode = "MANUAL";
    drawButtons();
    sendCmdToArduino("mode=MANUAL");
  }
}

// ══════════════════════════════════════════════════════════════
// SPLASH SCREEN
// ══════════════════════════════════════════════════════════════
void showSplash() {
  tft.fillScreen(TFT_BLACK);

  // Header splash
  tft.fillRect(0, 0, SCR_W, 42, C_HEADER);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, C_HEADER);
  tft.drawString("CORN DRYER IoT v3.2", 240, 12, 4);
  tft.setTextColor(TFT_CYAN, C_HEADER);
  tft.drawString("Sistem Pengering Jagung Infrared", 240, 32, 1);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Konfigurasi:", 10, 58, 2);

  struct { const char* k; const char* v; uint16_t c; } info[] = {
    {"TC1", "Inlet  (suhu udara masuk)",  C_TC1},
    {"TC2", "Outlet (suhu udara keluar)", C_TC2},
    {"TC3", "Chamber Kiri",               C_TC3},
    {"TC4", "Chamber Tengah",             C_TC4},
    {"TC5", "Chamber Kanan",              C_TC5},
  };
  for (int i=0; i<5; i++) {
    tft.setTextColor(info[i].c, TFT_BLACK);
    char buf[50]; sprintf(buf, "  %s: %s", info[i].k, info[i].v);
    tft.drawString(buf, 10, 80+i*17, 2);
  }

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Heater: 2x500W + 125W Ceramic = 1125W", 10, 172, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Setpoint range: 60 C ~ 135 C", 10, 190, 2);

  // Progress bar
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Menginisialisasi...", 10, 220, 2);
  tft.drawRect(10, 244, 460, 16, TFT_DARKGREY);
  for (int i=0; i<=456; i+=4) {
    tft.fillRect(12, 246, i, 12, TFT_GREEN);
    if (i % 40 == 0) delay(8);
  }
  delay(600);
  tft.setTextDatum(TL_DATUM);
}

// ══════════════════════════════════════════════════════════════
// GAMBAR SIMBOL IKON
// ══════════════════════════════════════════════════════════════

// Ikon angin ">>>" untuk ALIRAN UDARA
// Digambar di koordinat (x, y), ukuran ~16×10px
void drawIconWind(int x, int y, uint16_t col) {
  // 3 garis horizontal dengan ujung panah kecil
  for (int i=0; i<3; i++) {
    int yy = y + i*4;
    tft.drawLine(x, yy, x+10, yy, col);
    // Arrow head
    tft.drawLine(x+10, yy, x+7, yy-2, col);
    tft.drawLine(x+10, yy, x+7, yy+2, col);
  }
}

// Ikon termometer "T" untuk DISTRIBUSI CHAMBER
// Digambar di koordinat (x, y), ukuran ~10×14px
void drawIconThermo(int x, int y, uint16_t col) {
  // Batang termometer
  tft.drawRect(x+3, y, 4, 10, col);
  tft.fillRect(x+4, y+1, 2, 8, col);
  // Bola bawah
  tft.fillCircle(x+5, y+13, 4, col);
  // Skala (3 garis kecil kanan batang)
  for (int i=0; i<3; i++) {
    tft.drawLine(x+7, y+2+i*3, x+10, y+2+i*3, col);
  }
}

// Panah ke atas (untuk tombol SP+)
// cx,cy = titik tengah panah, sz = ukuran
void drawArrowUp(int cx, int cy, int sz, uint16_t col) {
  tft.fillTriangle(cx, cy-sz, cx-sz, cy+sz/2, cx+sz, cy+sz/2, col);
}

// Panah ke bawah (untuk tombol SP-)
void drawArrowDown(int cx, int cy, int sz, uint16_t col) {
  tft.fillTriangle(cx, cy+sz, cx-sz, cy-sz/2, cx+sz, cy-sz/2, col);
}

// Bulatan status kecil
void drawDot(int cx, int cy, uint16_t col, bool active) {
  tft.fillCircle(cx, cy, 5, active ? col : TFT_DARKGREY);
  if (active) tft.drawCircle(cx, cy, 6, TFT_WHITE);
}

// ══════════════════════════════════════════════════════════════
// GAMBAR ELEMEN STATIS (sekali saat startup)
// ══════════════════════════════════════════════════════════════
void drawStatic() {

  // ── Header ─────────────────────────────────────────────────
  tft.fillRect(0, Y_HDR_TOP, SCR_W, Y_HDR_H, C_HEADER);
  // Garis bawah header warna cyan
  tft.fillRect(0, Y_HDR_LINE, SCR_W, 1, TFT_CYAN);
  // Judul header
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, C_HEADER);
  tft.drawString("CORN DRYER CONTROL v3.0", 10, Y_HDR_H/2, 4);
  tft.setTextDatum(TL_DATUM);

  // ── Divider datetime ─────────────────────────────────────
  tft.drawLine(0, Y_DIV_DT, SCR_W, Y_DIV_DT, C_DIVIDER);

  // ── Garis vertikal kolom kiri/kanan ──────────────────────
  // Dari bawah header hingga divider konten
  tft.drawLine(LX_W, Y_DIV_DT+1, LX_W, Y_DIV_CONT, C_DIVIDER);

  // ── Label section: ALIRAN UDARA ────────────────────────
  // Kotak indikator warna sebelum label
  tft.fillRect(TX_LABEL, Y_AIR_LBL+4, 8, 8, C_AIRFLOW);
  drawIconWind(TX_LABEL+11, Y_AIR_LBL+3, C_AIRFLOW);
  tft.setTextColor(C_AIRFLOW, TFT_BLACK);
  tft.drawString("ALIRAN UDARA:", TX_LABEL+30, Y_AIR_LBL, 2);

  // ── Label baris TC1, TC2, Delta ──────────────────────────
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("TC1 Inlet  :", TX_LABEL, Y_TC1, 2);
  tft.drawString("TC2 Outlet :", TX_LABEL, Y_TC2, 2);
  tft.drawString("Delta T    :", TX_LABEL, Y_DLT, 2);

  // ── Divider tengah ────────────────────────────────────────
  tft.fillRect(0, Y_DIV_MID, LX_W, 1, C_DIVIDER);

  // ── Label section: DISTRIBUSI CHAMBER ────────────────────
  tft.fillRect(TX_LABEL, Y_CHM_LBL+4, 8, 8, C_CHAMBER);
  drawIconThermo(TX_LABEL+11, Y_CHM_LBL+1, C_CHAMBER);
  tft.setTextColor(C_CHAMBER, TFT_BLACK);
  tft.drawString("DISTRIBUSI CHAMBER:", TX_LABEL+24, Y_CHM_LBL, 2);

  // ── Label baris TC3/4/5 ───────────────────────────────────
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("TC3 Kiri   :", TX_LABEL, Y_TC3, 2);
  tft.drawString("TC4 Tengah :", TX_LABEL, Y_TC4, 2);
  tft.drawString("TC5 Kanan  :", TX_LABEL, Y_TC5, 2);

  // ── Background progress bar (kotak gelap kosong) ─────────
  const int barRows[] = { Y_TC1, Y_TC2, Y_TC3, Y_TC4, Y_TC5 };
  for (int i=0; i<5; i++) {
    int by = barRows[i] + BAR_YO;
    tft.fillRect(BAR_X, by, BAR_W, BAR_H, C_BAR_BG);
    tft.drawRect(BAR_X-1, by-1, BAR_W+2, BAR_H+2, C_DIVIDER);
  }

  // ── Divider tebal bawah konten ────────────────────────────
  tft.fillRect(0, Y_DIV_CONT,   SCR_W, 1, C_DIVIDER);
  tft.fillRect(0, Y_DIV_CONT+1, SCR_W, 1, 0x8410);  // Shadow effect

  // ── Divider bawah status ──────────────────────────────────
  tft.drawLine(0, Y_DIV_STS, SCR_W, Y_DIV_STS, C_DIVIDER);

  // ── Divider atas grafik ───────────────────────────────────
  tft.drawLine(0, Y_DIV_GRF, SCR_W, Y_DIV_GRF, C_DIVIDER);

  // ── Gambar panel kanan dan tombol ────────────────────────
  drawRightPanel();
  drawButtons();
}

// ══════════════════════════════════════════════════════════════
// UPDATE DINAMIS (dipanggil setiap 1 detik)
// ══════════════════════════════════════════════════════════════
void updateDynamic() {
  drawInfoBar();
  drawSensorArea();
  drawStatusBar();
  drawGraph();
  updateCmdStatus();
  updateTimerDoneNotif();   // ← v3.3: auto-bersih notifikasi timer selesai

  // Indikator koneksi Arduino — pojok kanan atas header
  bool arOK = dataReceived && (millis()-lastDataTime < 5000);
  // Kedip kuning saat sedang kirim perintah
  if (cmdStatus == CMD_SENDING) {
    bool blink = (millis()/200) % 2;
    tft.fillCircle(466, 17, 8, blink ? TFT_YELLOW : TFT_BLACK);
    return;
  }
  tft.fillCircle(466, 17, 8, arOK ? TFT_GREEN : TFT_RED);
  if (arOK) {
    tft.fillCircle(466, 17, 4, TFT_BLACK);
    tft.fillCircle(466, 17, 2, TFT_GREEN);
  }
}

// ══════════════════════════════════════════════════════════════
// BARIS INFO (DATETIME + NTP + SHT + AP)
// ══════════════════════════════════════════════════════════════
// Layout presisi (font1 = 6px/char, 8px tinggi):
//   X=3   : Tanggal  "10/03/2026"  (10ch = 66px → ends X=69)
//   X=75  : Waktu    "15:55:49 WIB"(12ch = 80px → ends X=155)
//   X=200 : ●NTP     (dot X=200, text X=208, 3ch=21px → ends X=229)
//   X=235 : ●SHT     (dot X=235, text X=243, "n/m" → ends ~X=275)
//   X=285 : ●        (dot X=285, "1AP" X=293, 3ch=21px → ends X=314)
// TOTAL MAX: X=314 < 480 ✓ (tidak ada overflow)
void drawInfoBar() {
  tft.fillRect(0, Y_DT, SCR_W, Y_DT_H, TFT_BLACK);

  int ty = Y_DT + 3;   // Offset agar teks vertikal centered (pad 3px)

  if (ntpSynced) {
    time_t now = time(nullptr);
    struct tm ti; localtime_r(&now, &ti);
    char date[14], clk[14];
    sprintf(date, "%02d/%02d/%04d", ti.tm_mday, ti.tm_mon+1, 1900+ti.tm_year);
    sprintf(clk,  "%02d:%02d:%02d WIB", ti.tm_hour, ti.tm_min, ti.tm_sec);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(date, 3,  ty, 1);   // X:3–69
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(clk,  75, ty, 1);   // X:75–155
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("NTP tidak aktif - waktu tidak tersedia", 3, ty, 1);
  }

  // ●NTP (X=200)
  tft.fillCircle(204, ty+4, 4, ntpSynced ? C_NTP_OK : C_NTP_FAIL);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("NTP", 211, ty, 1);   // X:211-232

  // ●SHT + hitungan (X=238)
  tft.fillCircle(242, ty+4, 4, sheetsOK ? C_SHT_OK : TFT_DARKGREY);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("SHT", 249, ty, 1);   // "SHT" X:249-270
  char sc[10];
  int total = sheetsOK_cnt + sheetsFail_cnt;
  sprintf(sc, "%d/%d", sheetsOK_cnt, total > 0 ? total : 0);
  tft.setTextColor(sheetsOK ? C_SHT_OK : TFT_DARKGREY, TFT_BLACK);
  tft.drawString(sc, 275, ty, 1);      // "n/m" X:275-299

  // ●AP (X=308)
  int nAP = WiFi.softAPgetStationNum();
  tft.fillCircle(312, ty+4, 4, nAP>0 ? TFT_CYAN : TFT_DARKGREY);
  char apStr[8]; sprintf(apStr, "%dAP", nAP);
  tft.setTextColor(nAP>0 ? TFT_CYAN : TFT_DARKGREY, TFT_BLACK);
  tft.drawString(apStr, 319, ty, 1);   // "1AP" X:319-340
}

// ══════════════════════════════════════════════════════════════
// AREA SENSOR (nilai + progress bar)
// ══════════════════════════════════════════════════════════════
void drawSensorArea() {
  // Hapus area nilai di kolom kiri (X=TX_VALUE hingga X=BAR_X+BAR_W)
  // Setiap baris: hapus tepat di Y baris, tinggi 16px
  const int rows[] = {Y_TC1, Y_TC2, Y_DLT, Y_TC3, Y_TC4, Y_TC5};
  for (int i=0; i<6; i++) {
    tft.fillRect(TX_VALUE, rows[i], BAR_X+BAR_W-TX_VALUE, 16, TFT_BLACK);
  }
  tft.fillRect(0, Y_AVG, LX_W, 16, TFT_BLACK);
  tft.fillRect(0, Y_AMB, LX_W, 16, TFT_BLACK);

  // ── TC1 Inlet ────────────────────────────────────────────
  drawTCvalue(tc1, C_TC1, Y_TC1);

  // ── TC2 Outlet ───────────────────────────────────────────
  drawTCvalue(tc2, C_TC2, Y_TC2);

  // ── Delta T ──────────────────────────────────────────────
  {
    char buf[10]; sprintf(buf, "%+.1f C", delta_temp);
    bool pos = (delta_temp > 0.5f);
    bool neg = (delta_temp < -0.5f);
    uint16_t col = pos ? C_DELTA_POS : (neg ? C_DELTA_NEG : TFT_DARKGREY);
    tft.setTextColor(col, TFT_BLACK);
    tft.drawString(buf, TX_VALUE, Y_DLT, 2);
    // Keterangan kecil
    const char* hint = pos ? "(OK: Out>In)" : (neg ? "(!ANOMALI!)" : "(Heater OFF?)");
    tft.setTextColor(pos ? C_DELTA_POS : (neg ? C_DELTA_NEG : TFT_DARKGREY), TFT_BLACK);
    tft.drawString(hint, TX_VALUE+80, Y_DLT+4, 1);
  }

  // ── TC3/4/5 ──────────────────────────────────────────────
  drawTCvalue(tc3, C_TC3, Y_TC3);
  drawTCvalue(tc4, C_TC4, Y_TC4);
  drawTCvalue(tc5, C_TC5, Y_TC5);

  // ── Avg Chamber + Deviasi ─────────────────────────────────
  {
    char avBuf[14]; sprintf(avBuf, "Avg:%.1fC", temp_avg);
    tft.setTextColor(C_AVG, TFT_BLACK);
    tft.drawString(avBuf, TX_LABEL, Y_AVG, 2);        // X:4

    float dev = temp_avg - setpoint;
    char dvBuf[16];
    sprintf(dvBuf, " Dev:%+.1fC", dev);
    uint16_t dc = (fabsf(dev) < 2.0f) ? C_DEV_OK :
                  (fabsf(dev) < 5.0f) ? C_DEV_WARN : C_DEV_BAD;
    tft.setTextColor(dc, TFT_BLACK);
    tft.drawString(dvBuf, TX_LABEL+110, Y_AVG, 2);    // X:114

    // Keterangan kecil di sebelah kanan
    const char* devHint = (fabsf(dev) < 2.0f) ? "(optimal)" :
                          (dev < 0) ? "(terlalu dingin)" : "(terlalu panas)";
    tft.setTextColor(dc, TFT_BLACK);
    tft.drawString(devHint, TX_LABEL+233, Y_AVG+4, 1); // X:237, font1
  }

  // ── Ambient + Humidity + Uniformity ──────────────────────
  {
    // Hitung uniformity TC3/4/5
    float tArr[3] = {tc3, tc4, tc5};
    float mn=tArr[0], mx=tArr[0], sm=0;
    for (int i=0;i<3;i++) {
      if(tArr[i]<mn) mn=tArr[i];
      if(tArr[i]>mx) mx=tArr[i];
      sm+=tArr[i];
    }
    float avgT = sm/3.0f;
    float spread = mx-mn;
    float unif = (avgT>0.5f) ? constrain(100.0f - (spread/avgT)*100.0f, 0.0f, 100.0f) : 0.0f;

    char amBuf[14]; sprintf(amBuf, "Amb:%.1fC", temp_amb);
    char huBuf[12]; sprintf(huBuf, "Hum:%.0f%%", hum_amb);
    char unBuf[14]; sprintf(unBuf, "Unif:%.1f%%", unif);

    tft.setTextColor(C_AMB, TFT_BLACK);
    tft.drawString(amBuf, TX_LABEL,     Y_AMB, 2);     // X:4
    tft.setTextColor(C_HUM, TFT_BLACK);
    tft.drawString(huBuf, TX_LABEL+100, Y_AMB, 2);     // X:104

    uint16_t uc = (unif>97.0f)?C_DEV_OK : (unif>92.0f)?C_DEV_WARN : C_DEV_BAD;
    tft.setTextColor(uc, TFT_BLACK);
    tft.drawString(unBuf, TX_LABEL+195, Y_AMB, 2);     // X:199
  }
}

// Gambar nilai satu baris TC + progress bar
void drawTCvalue(float val, uint16_t col, int y) {
  // Teks nilai
  char buf[10];
  if (val < 0.5f) {
    strcpy(buf, " --.- C");
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  } else {
    sprintf(buf, "%5.1f C", val);
    tft.setTextColor(col, TFT_BLACK);
  }
  tft.drawString(buf, TX_VALUE, y, 2);   // font2, X=TX_VALUE=120

  // Progress bar
  // Skala: tMin=20°C → tMax=setpoint+20°C (atau 80°C minimum)
  float tMin = 20.0f;
  float tMax = max(setpoint + 20.0f, 80.0f);
  float pct  = (val > 0.5f)
               ? constrain((val-tMin)/(tMax-tMin), 0.0f, 1.0f)
               : 0.0f;
  int   fill = (int)(pct * (float)(BAR_W - 2));

  int bx = BAR_X;
  int by = y + BAR_YO;

  // Background bar (kosong)
  tft.fillRect(bx+1, by+1, BAR_W-2, BAR_H-2, C_BAR_BG);

  // Isi bar dengan warna kondisional
  if (fill > 0) {
    uint16_t barCol = (pct > 0.90f) ? C_DEV_BAD  :    // Merah: hampir max
                      (pct > 0.70f) ? TFT_YELLOW  :    // Kuning: mendekati
                                      col;              // Warna TC normal
    tft.fillRect(bx+1, by+1, fill, BAR_H-2, barCol);

    // Segmen di bar (setiap 25%) sebagai grid
    for (int seg=1; seg<4; seg++) {
      int segX = bx+1 + (BAR_W-2)*seg/4;
      tft.drawLine(segX, by+1, segX, by+BAR_H-2, C_BAR_BG);
    }
  }

  // Marker garis putih di ujung isian bar (sebagai penunjuk level)
  if (fill > 4) {
    tft.drawLine(bx+fill, by+1, bx+fill, by+BAR_H-2, TFT_WHITE);
  }
}

// ══════════════════════════════════════════════════════════════
// PANEL KANAN — TAB SYSTEM v3.3
// Tab [SP] = Setpoint, Tab [TMR] = Timer
// ══════════════════════════════════════════════════════════════
void drawRightPanel() {
  // ── Background panel ──────────────────────────────────────
  uint16_t panelBg = rightTabTMR ? C_TMR_BG : C_SP_BG;
  tft.fillRect(RX, Y_DIV_DT+1, RX_W, Y_DIV_CONT-Y_DIV_DT-1, panelBg);
  tft.drawLine(LX_W, Y_DIV_DT+1, LX_W, Y_DIV_CONT, C_DIVIDER);

  // ── TAB BAR (Y:51–66) ─────────────────────────────────────
  // Tab SP (X:360–419)
  uint16_t spTabBg  = rightTabTMR ? C_TAB_IDLE : C_TAB_ACT;
  uint16_t tmrTabBg = rightTabTMR ? C_TAB_ACT  : C_TAB_IDLE;
  tft.fillRect(X_TAB_SP,  Y_TAB_ROW, TAB_W, Y_TAB_H, spTabBg);
  tft.fillRect(X_TAB_TMR, Y_TAB_ROW, TAB_W, Y_TAB_H, tmrTabBg);

  // Garis pemisah vertikal antar tab
  tft.drawLine(X_TAB_TMR, Y_TAB_ROW, X_TAB_TMR, Y_TAB_ROW+Y_TAB_H, C_DIVIDER);

  // Label tab — TC_DATUM
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(rightTabTMR ? TFT_DARKGREY : TFT_WHITE, spTabBg);
  tft.drawString("SP", TAB_MID_SP, Y_TAB_ROW+4, 1);
  tft.setTextColor(rightTabTMR ? TFT_WHITE : TFT_DARKGREY, tmrTabBg);
  tft.drawString("TMR", TAB_MID_TMR, Y_TAB_ROW+4, 1);

  // Garis bawah tab bar (separator ke konten)
  tft.drawLine(RX, Y_PANEL_TOP-1, RX+RX_W, Y_PANEL_TOP-1, C_DIVIDER);

  // ── Pilih konten berdasarkan tab aktif ────────────────────
  if (!rightTabTMR) drawPanelSP();
  else              drawPanelTMR();

  tft.setTextDatum(TL_DATUM);
}

// ── Panel SP (Setpoint) ───────────────────────────────────────
void drawPanelSP() {
  tft.setTextDatum(TC_DATUM);
  uint16_t bg = C_SP_BG;

  // (1) Label "SETPOINT" — font1, Y=70
  tft.setTextColor(0xC618, bg);
  tft.drawString("SETPOINT", RX_MID, Y_SP_LBL, 1);

  // (2) Nilai SP — font4 (26px), Y=81
  char spStr[6]; sprintf(spStr, "%d", (int)setpoint);
  tft.setTextColor(C_SP_VAL, bg);
  tft.drawString(spStr, RX_MID, Y_SP_VAL, 4);

  // (3) "C" — font2 (16px), Y=110
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString("C", RX_MID, Y_SP_UNIT, 2);

  // (4) Hysteresis — font1, Y=128
  char hyStr[10]; sprintf(hyStr, "+/-%.0fC", hysteresis);
  tft.setTextColor(TFT_YELLOW, bg);
  tft.drawString(hyStr, RX_MID, Y_SP_HYS, 1);

  // (5) Separator Y=139
  tft.drawLine(RX+6, Y_SP_DIV, RX+RX_W-6, Y_SP_DIV, C_DIVIDER);

  // (6) Tombol SP+ — Y=142, H=28
  {
    int bx=RX+3, by=Y_SP_BTN1, bw=RX_W-6, bh=Y_SP_BTN_H;
    int cx=bx+bw/2;
    tft.fillRoundRect(bx, by, bw, bh, 5, C_SP_BTN);
    tft.fillRoundRect(bx+1, by+1, bw-2, bh/2, 4, 0x2DFF);
    tft.drawRoundRect(bx, by, bw, bh, 5, TFT_WHITE);
    drawArrowUp(cx, by+9, 6, TFT_WHITE);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_BLACK, C_SP_BTN);
    tft.drawString("SP +", cx, by+16, 2);
  }

  // (7) Tombol SP- — Y=173, H=28
  {
    int bx=RX+3, by=Y_SP_BTN2, bw=RX_W-6, bh=Y_SP_BTN_H;
    int cx=bx+bw/2;
    tft.fillRoundRect(bx, by, bw, bh, 5, C_SP_BTN);
    tft.fillRoundRect(bx+1, by+1, bw-2, bh/2, 4, 0x2DFF);
    tft.drawRoundRect(bx, by, bw, bh, 5, TFT_WHITE);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_BLACK, C_SP_BTN);
    tft.drawString("SP -", cx, by+4, 2);
    drawArrowDown(cx, by+20, 6, TFT_WHITE);
  }

  // (8) Info step/range — font1, Y=203
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_DARKGREY, bg);
  const char* stepStr = (setpoint >= 100.0f) ? "5C | 60~135" : "1C | 60~135";
  tft.drawString(stepStr, RX_MID, Y_SP_INFO, 1);
}

// ── Panel TMR (Timer) — 3 mode ────────────────────────────────
void drawPanelTMR() {
  tft.setTextDatum(TC_DATUM);
  uint16_t bg = C_TMR_BG;

  if (timerDone) {
    // ════ MODE DONE ════════════════════════════════════════
    // Background merah di seluruh konten panel
    tft.fillRect(RX, Y_PANEL_TOP, RX_W, Y_DIV_CONT-Y_PANEL_TOP, 0x8000);

    tft.setTextColor(TFT_WHITE, 0x8000);
    tft.drawString("SELESAI!", RX_MID, Y_TMR_DONE_LBL, 2);
    tft.setTextColor(TFT_YELLOW, 0x8000);
    tft.drawString("Heater+Motor", RX_MID, Y_TMR_DONE_TX1, 1);
    tft.drawString("sudah MATI", RX_MID, Y_TMR_DONE_TX2, 1);

    tft.drawLine(RX+6, Y_TMR_DONE_SEP, RX+RX_W-6, Y_TMR_DONE_SEP, TFT_WHITE);

    // Tombol RESET
    int bx=RX+3, by=Y_TMR_RESET, bw=RX_W-6, bh=Y_TMR_RESET_H;
    tft.fillRoundRect(bx, by, bw, bh, 5, TFT_YELLOW);
    tft.drawRoundRect(bx, by, bw, bh, 5, TFT_WHITE);
    tft.setTextColor(TFT_BLACK, TFT_YELLOW);
    tft.drawString("RESET", bx+bw/2, by+8, 2);

  } else if (timerRunning || (timerDuration > 0 && timerRemaining < timerDuration)) {
    // ════ MODE RUNNING / PAUSE ═════════════════════════════
    const char* stLabel = timerRunning ? "BERJALAN" : "PAUSE";
    uint16_t stColor    = timerRunning ? C_TMR_RUN  : C_TMR_PAUSE;

    // Label status
    tft.setTextColor(stColor, bg);
    tft.drawString(stLabel, RX_MID, Y_TMR_LBL, 1);

    // Countdown H:MM:SS — font4 (26px) — Y=80
    uint32_t rem = timerRemaining;
    uint8_t  hh  = rem / 3600;
    uint8_t  mm  = (rem % 3600) / 60;
    uint8_t  ss  = rem % 60;
    char cdBuf[10];
    if (hh > 0) sprintf(cdBuf, "%d:%02d:%02d", hh, mm, ss);
    else         sprintf(cdBuf, "%02d:%02d", mm, ss);
    tft.setTextColor(stColor, bg);
    // Font4 untuk countdown (ukuran lebih kecil jika H > 0 agar muat)
    int cdFont = (hh > 0) ? 2 : 4;
    int cdY    = (hh > 0) ? Y_TMR_CD+4 : Y_TMR_CD;
    tft.drawString(cdBuf, RX_MID, cdY, cdFont);

    // Progress bar — Y=108
    {
      float pct = (timerDuration > 0)
                  ? 1.0f - ((float)timerRemaining / (float)timerDuration)
                  : 0.0f;
      int bx  = RX+4, by  = Y_TMR_BAR;
      int bw  = RX_W-8, bh  = Y_TMR_BAR_H;
      tft.drawRect(bx, by, bw, bh, TFT_DARKGREY);
      tft.fillRect(bx+1, by+1, bw-2, bh-2, C_BAR_BG);
      int fill = (int)(pct * (bw-2));
      if (fill > 0) tft.fillRect(bx+1, by+1, fill, bh-2, stColor);
    }

    // Persentase selesai — Y=118
    {
      float pct = (timerDuration > 0)
                  ? (1.0f - (float)timerRemaining/(float)timerDuration)*100.0f
                  : 0.0f;
      char pBuf[10]; sprintf(pBuf, "%.0f%%", pct);
      tft.setTextColor(stColor, bg);
      tft.drawString(pBuf, RX_MID, Y_TMR_STS, 1);
    }

    tft.drawLine(RX+6, Y_TMR_SEP2, RX+RX_W-6, Y_TMR_SEP2, C_DIVIDER);

    // Tombol PAUSE / RESUME — Y=131
    {
      int bx=RX+3, by=Y_TMR_PAUSE, bw=RX_W-6, bh=Y_TMR_BTN_H;
      uint16_t btnBg = timerRunning ? C_TMR_PAUSE : C_TMR_RUN;
      tft.fillRoundRect(bx, by, bw, bh, 4, btnBg);
      tft.drawRoundRect(bx, by, bw, bh, 4, TFT_WHITE);
      tft.setTextColor(TFT_BLACK, btnBg);
      tft.drawString(timerRunning ? "PAUSE" : "RESUME", bx+bw/2, by+7, 2);
    }

    // Tombol STOP — Y=158
    {
      int bx=RX+3, by=Y_TMR_STOP, bw=RX_W-6, bh=Y_TMR_BTN_H;
      tft.fillRoundRect(bx, by, bw, bh, 4, C_TMR_STOP);
      tft.drawRoundRect(bx, by, bw, bh, 4, TFT_WHITE);
      tft.setTextColor(TFT_WHITE, C_TMR_STOP);
      tft.drawString("STOP", bx+bw/2, by+7, 2);
    }

    tft.drawLine(RX+6, Y_TMR_SEP3, RX+RX_W-6, Y_TMR_SEP3, C_DIVIDER);

    // Info durasi total — Y=186, Y=196
    {
      uint32_t td = timerDuration;
      char buf1[16], buf2[16];
      sprintf(buf1, "Dur: %lum", (unsigned long)(td/60));
      sprintf(buf2, "Set:%dm", timerSetMin);
      tft.setTextColor(TFT_DARKGREY, bg);
      tft.drawString(buf1, RX_MID, Y_TMR_INF1, 1);
      tft.drawString(buf2, RX_MID, Y_TMR_INF2, 1);
    }

  } else {
    // ════ MODE IDLE / SET ══════════════════════════════════
    // Label atas
    tft.setTextColor(C_TMR_IDLE, bg);
    tft.drawString("DURASI", RX_MID, Y_TMR_LBL, 1);

    // Tombol ▲ tambah — Y=80, H=24
    {
      int bx=RX+3, by=Y_TMR_UP, bw=RX_W-6, bh=Y_TMR_BTN_H;
      tft.fillRoundRect(bx, by, bw, bh, 4, C_TMR_BTN);
      tft.drawRoundRect(bx, by, bw, bh, 4, TFT_WHITE);
      drawArrowUp(bx+bw/2, by+8, 7, TFT_WHITE);
      tft.setTextColor(TFT_BLACK, C_TMR_BTN);
      tft.drawString("+1", bx+bw/2, by+14, 1);
    }

    // Nilai menit — font4 (26px) — Y=106
    char minBuf[6]; sprintf(minBuf, "%d", timerSetMin);
    tft.setTextColor(TFT_WHITE, bg);
    tft.drawString(minBuf, RX_MID, Y_TMR_VAL, 4);

    // Label "MENIT" — Y=134
    tft.setTextColor(C_TMR_IDLE, bg);
    tft.drawString("MENIT", RX_MID, Y_TMR_UNIT, 1);

    // Tombol ▼ kurangi — Y=144, H=24
    {
      int bx=RX+3, by=Y_TMR_DN, bw=RX_W-6, bh=Y_TMR_BTN_H;
      tft.fillRoundRect(bx, by, bw, bh, 4, C_TMR_BTN);
      tft.drawRoundRect(bx, by, bw, bh, 4, TFT_WHITE);
      drawArrowDown(bx+bw/2, by+15, 7, TFT_WHITE);
      tft.setTextColor(TFT_BLACK, C_TMR_BTN);
      tft.drawString("-1", bx+bw/2, by+2, 1);
    }

    tft.drawLine(RX+6, Y_TMR_SEP1, RX+RX_W-6, Y_TMR_SEP1, C_DIVIDER);

    // Tombol [+10] dan [+60] — Y=172, H=18
    {
      int hw = (RX_W-9)/2;    // lebar setengah tombol
      int by = Y_TMR_QUICK, bh = Y_TMR_QUICK_H;
      // +10
      tft.fillRoundRect(RX+3, by, hw, bh, 3, 0x2D6B);
      tft.drawRoundRect(RX+3, by, hw, bh, 3, TFT_DARKGREY);
      tft.setTextColor(TFT_CYAN, 0x2D6B);
      tft.drawString("+10", RX+3+hw/2, by+4, 1);
      // +60
      tft.fillRoundRect(RX+3+hw+3, by, hw, bh, 3, 0x2D6B);
      tft.drawRoundRect(RX+3+hw+3, by, hw, bh, 3, TFT_DARKGREY);
      tft.setTextColor(TFT_CYAN, 0x2D6B);
      tft.drawString("+60", RX+3+hw+3+hw/2, by+4, 1);
    }

    // Tombol [MULAI] — Y=192, H=19
    {
      int bx=RX+3, by=Y_TMR_START, bw=RX_W-6, bh=Y_TMR_START_H;
      tft.fillRoundRect(bx, by, bw, bh, 4, C_TMR_RUN);
      tft.drawRoundRect(bx, by, bw, bh, 4, TFT_WHITE);
      tft.setTextColor(TFT_BLACK, C_TMR_RUN);
      tft.drawString("MULAI", bx+bw/2, by+5, 2);
    }
  }
}

// ══════════════════════════════════════════════════════════════
// TOMBOL KONTROL UTAMA (full width)
// ══════════════════════════════════════════════════════════════
void drawButtons() {
  tft.fillRect(0, Y_BTN_TOP, SCR_W, Y_BTN_H, TFT_BLACK);

  bool isAuto = (ctrl_mode == "AUTO");
  bool isMan  = (ctrl_mode == "MANUAL");

  // MOTOR ON/OFF
  uint16_t mc  = motor_on ? C_BTN_ON : C_BTN_OFF;
  const char* ml = motor_on ? "  MOTOR ON  " : " MOTOR OFF  ";
  drawBigBtn(BTN_M_X, Y_BTN_TOP, BTN_M_W, Y_BTN_H, ml, mc, TFT_WHITE, false);

  // AUTO
  drawBigBtn(BTN_A_X, Y_BTN_TOP, BTN_A_W, Y_BTN_H, "  AUTO  ",
             isAuto ? C_BTN_AUTO : C_BTN_IDLE, TFT_WHITE, isAuto);

  // MANUAL
  drawBigBtn(BTN_N_X, Y_BTN_TOP, BTN_N_W, Y_BTN_H, "  MANUAL  ",
             isMan ? C_BTN_MAN : C_BTN_IDLE, TFT_WHITE, isMan);
}

void drawBigBtn(int bx, int by, int bw, int bh,
                const char* lbl, uint16_t bg, uint16_t fg, bool active) {
  tft.fillRoundRect(bx, by, bw, bh, 8, bg);
  // Border: lebih terang jika aktif
  uint16_t borderCol = active ? TFT_WHITE : 0x632C;
  tft.drawRoundRect(bx,   by,   bw,   bh,   8, borderCol);
  tft.drawRoundRect(bx+1, by+1, bw-2, bh-2, 7, active ? 0xAD55 : borderCol);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(lbl, bx+bw/2, by+bh/2-7, 2);
  tft.setTextDatum(TL_DATUM);
}

// ══════════════════════════════════════════════════════════════
// STATUS BAR — v3.3 (tambah indikator TIMER)
// ══════════════════════════════════════════════════════════════
// Layout (font1 6px/char, 8px tinggi):
//   ●HTR | ●MOT | ●FAN | ●SHT | MODE | SP | TMR
void drawStatusBar() {
  tft.fillRect(0, Y_STS, SCR_W, Y_STS_H, TFT_BLACK);
  int sy = Y_STS + 2;

  if (emg_stop) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("!!! EMERGENCY STOP — RESET ARDUINO !!!", 240, sy, 2);
    tft.setTextDatum(TL_DATUM);
    return;
  }

  // ● HTR
  drawDot(10, sy+4, C_BTN_ON, heater_on);
  tft.setTextColor(heater_on ? C_BTN_ON : TFT_DARKGREY, TFT_BLACK);
  tft.drawString(heater_on ? "HTR:ON" : "HTR:OFF", 20, sy, 1);
  tft.drawLine(67, Y_STS+1, 67, Y_STS+Y_STS_H-2, C_DIVIDER);

  // ● MOT
  drawDot(75, sy+4, C_BTN_ON, motor_on);
  tft.setTextColor(motor_on ? C_BTN_ON : TFT_DARKGREY, TFT_BLACK);
  tft.drawString(motor_on ? "MOT:ON" : "MOT:OFF", 85, sy, 1);
  tft.drawLine(135, Y_STS+1, 135, Y_STS+Y_STS_H-2, C_DIVIDER);

  // ● FAN
  drawDot(143, sy+4, TFT_CYAN, true);
  char fanBuf[10];
  sprintf(fanBuf, "FAN:%d%%", map(fan_spd, 0, 255, 0, 100));
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(fanBuf, 153, sy, 1);
  tft.drawLine(200, Y_STS+1, 200, Y_STS+Y_STS_H-2, C_DIVIDER);

  // ● SHT
  drawDot(208, sy+4, C_SHT_OK, sheetsOK);
  tft.setTextColor(sheetsOK ? C_SHT_OK : TFT_DARKGREY, TFT_BLACK);
  tft.drawString(sheetsOK ? "SHT:OK" : "SHT:--", 218, sy, 1);
  tft.drawLine(260, Y_STS+1, 260, Y_STS+Y_STS_H-2, C_DIVIDER);

  // MODE
  bool isAuto = (ctrl_mode == "AUTO");
  tft.setTextColor(isAuto ? C_BTN_AUTO : C_BTN_MAN, TFT_BLACK);
  tft.drawString(ctrl_mode.c_str(), 267, sy, 1);
  tft.drawLine(308, Y_STS+1, 308, Y_STS+Y_STS_H-2, C_DIVIDER);

  // SP
  char spBuf[10]; sprintf(spBuf, "SP:%.0fC", setpoint);
  tft.setTextColor(C_SP_VAL, TFT_BLACK);
  tft.drawString(spBuf, 315, sy, 1);
  tft.drawLine(355, Y_STS+1, 355, Y_STS+Y_STS_H-2, C_DIVIDER);

  // TMR — BARU v3.3 (X:357–479)
  if (timerDone) {
    // Merah: DONE
    tft.setTextColor(C_TMR_DONE, TFT_BLACK);
    tft.drawString("TMR:DONE", 362, sy, 1);
  } else if (timerRunning && timerRemaining > 0) {
    // Hijau: countdown
    uint32_t rem = timerRemaining;
    uint8_t hh = rem/3600, mm=(rem%3600)/60, ss=rem%60;
    char tBuf[12];
    if (hh > 0) sprintf(tBuf, "%d:%02d:%02d", hh, mm, ss);
    else         sprintf(tBuf, "%02d:%02d", mm, ss);
    tft.setTextColor(C_TMR_RUN, TFT_BLACK);
    tft.drawString(tBuf, 362, sy, 1);
  } else if (timerDuration > 0) {
    // Kuning: pause/siap
    uint32_t rem = timerRemaining;
    uint8_t hh = rem/3600, mm=(rem%3600)/60, ss=rem%60;
    char tBuf[12];
    if (hh > 0) sprintf(tBuf, "%d:%02d:%02d", hh, mm, ss);
    else         sprintf(tBuf, "%02d:%02d", mm, ss);
    tft.setTextColor(C_TMR_PAUSE, TFT_BLACK);
    tft.drawString(tBuf, 362, sy, 1);
  } else {
    // Grey: belum di-set
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("TMR:--:--", 362, sy, 1);
  }
}

// ══════════════════════════════════════════════════════════════
// GRAFIK TREN SUHU
// ══════════════════════════════════════════════════════════════
void drawGraph() {
  int gx=0, gy=Y_GRF_TOP, gw=SCR_W, gh=Y_GRF_H;

  float tMin = 20.0f;
  float tMax = max(setpoint + 20.0f, 80.0f);
  int   plotH = gh - 13;    // 13px untuk legenda di atas

  tft.fillRect(gx, gy, gw, gh, TFT_BLACK);

  // Legenda
  int lx = 4;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("TREN:", lx, gy+2, 1); lx += 38;
  tft.setTextColor(C_G_TC3, TFT_BLACK);
  tft.drawString("TC3", lx, gy+2, 1); lx += 22;
  tft.setTextColor(C_G_TC4, TFT_BLACK);
  tft.drawString("TC4", lx, gy+2, 1); lx += 22;
  tft.setTextColor(C_G_TC5, TFT_BLACK);
  tft.drawString("TC5", lx, gy+2, 1); lx += 22;
  tft.setTextColor(C_G_SP, TFT_BLACK);
  tft.drawString("SP", lx, gy+2, 1);

  // Garis batas atas area plot
  tft.drawLine(gx, gy+11, gx+gw, gy+11, C_DIVIDER);

  // Mapping suhu → piksel Y
  auto mapY = [&](float t) -> int {
    float clamped = constrain(t, tMin, tMax);
    return gy + gh - 1 - (int)((clamped-tMin)/(tMax-tMin) * (float)plotH);
  };

  // Grid horizontal (25%, 50%, 75%)
  for (int pct=25; pct<=75; pct+=25) {
    float temp = tMin + (tMax-tMin)*pct/100.0f;
    int ygrid = mapY(temp);
    for (int x=0; x<gw; x+=6) {
      tft.drawPixel(x, ygrid, C_DIVIDER);
    }
  }

  // Data lines TC3/4/5
  for (int i=1; i<HIST_LEN; i++) {
    int i1 = (hist_idx + i - 1) % HIST_LEN;
    int i2 = (hist_idx + i)     % HIST_LEN;
    int x1 = gx + (i-1)*gw/HIST_LEN;
    int x2 = gx +  i   *gw/HIST_LEN;
    tft.drawLine(x1, mapY(h_tc3[i1]), x2, mapY(h_tc3[i2]), C_G_TC3);
    tft.drawLine(x1, mapY(h_tc4[i1]), x2, mapY(h_tc4[i2]), C_G_TC4);
    tft.drawLine(x1, mapY(h_tc5[i1]), x2, mapY(h_tc5[i2]), C_G_TC5);
  }

  // Garis setpoint (putus-putus merah)
  int ysp = mapY(setpoint);
  for (int x=gx; x<gx+gw; x+=5) {
    tft.drawPixel(x,   ysp, C_G_SP);
    tft.drawPixel(x+1, ysp, C_G_SP);
  }
  // Label SP di akhir kanan
  char sl[8]; sprintf(sl, "%.0fC", setpoint);
  tft.setTextColor(C_G_SP, TFT_BLACK);
  tft.drawString(sl, gx+gw-28, ysp > gy+20 ? ysp-9 : ysp+2, 1);
}

// ══════════════════════════════════════════════════════════════
// KIRIM KE GOOGLE SHEETS
// ══════════════════════════════════════════════════════════════
void sendSheets() {
  if (String(GOOGLE_SCRIPT_URL).indexOf("YOUR_SCRIPT_ID") >= 0) return;

  String ts = "NO_NTP";
  if (ntpSynced) {
    time_t now=time(nullptr); struct tm ti; localtime_r(&now,&ti);
    char buf[28];
    sprintf(buf, "%02d/%02d/%04d %02d:%02d:%02d",
            ti.tm_mday, ti.tm_mon+1, 1900+ti.tm_year,
            ti.tm_hour, ti.tm_min, ti.tm_sec);
    ts = String(buf);
  }

  // Hitung parameter derived
  float spread = max({tc3,tc4,tc5}) - min({tc3,tc4,tc5});
  float avgTC  = (tc3+tc4+tc5)/3.0f;
  float unif   = (avgTC>0.5f) ?
    constrain(100.0f-(spread/avgTC)*100.0f, 0.0f, 100.0f) : 0.0f;

  String pd = "timestamp="    + ts;
  pd += "&tc1="               + String(tc1,2);
  pd += "&tc2="               + String(tc2,2);
  pd += "&tc3="               + String(tc3,2);
  pd += "&tc4="               + String(tc4,2);
  pd += "&tc5="               + String(tc5,2);
  pd += "&temp_avg="          + String(temp_avg,2);
  pd += "&delta_temp="        + String(delta_temp,2);
  pd += "&temp_ambient="      + String(temp_amb,2);
  pd += "&humidity="          + String(hum_amb,2);
  pd += "&setpoint="          + String(setpoint,1);
  pd += "&hysteresis="        + String(hysteresis,1);
  pd += "&heater="            + String(heater_on?"ON":"OFF");
  pd += "&motor="             + String(motor_on?"ON":"OFF");
  pd += "&fan_speed="         + String(map(fan_spd,0,255,0,100));
  pd += "&mode="              + ctrl_mode;
  pd += "&emergency="         + String(emg_stop?"YES":"NO");

  WiFiClientSecure wc; wc.setInsecure();
  HTTPClient https;
  https.begin(wc, GOOGLE_SCRIPT_URL);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  https.setTimeout(10000);

  int code = https.POST(pd);
  if (code > 0) {
    String res = https.getString();
    sheetsOK = (res.indexOf("success") >= 0);
  } else {
    sheetsOK = false;
  }

  if (sheetsOK) sheetsOK_cnt++;
  else          sheetsFail_cnt++;

  https.end();
  Serial.print(F("[SHEETS] "));
  Serial.println(sheetsOK ? F("OK") : F("GAGAL"));
}
