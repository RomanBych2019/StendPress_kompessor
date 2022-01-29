#include <Arduino.h>
#include <ESP32Ticker.h>
#include "SoftwareSerial.h"
#include "Adafruit_Thermal.h"
#include "Preferences.h"
#include <in.h>
#include <out.h>
#include <canvas_logo_vert.h>
#include <canvas_logo_hor.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>


struct soft_date
{
  uint8_t YEAR_OF_RELEASE = 0x15;
  uint8_t MONTH_OF_RELEASE = 0x0B;
  uint8_t DAY_OF_RELEASE = 0x1B;
  long SERIAL_NUMBER = 0x02;
  uint8_t MODEL_CODE = 0x20;
  uint8_t FIRMWARE_VERSION = 0x17;
} soft;

struct NextRS232
{
  uint16_t id;
  uint16_t val;
};

NextRS232 NextionRS232;
void analyseDate(NextRS232 &NextionRS232);
void analyseString(String &incStr);

#define WORKTPLATE //выбор платы - WORKTPLATE рабочая  плата //TESTPLATE тестовая плата
#define no_USE_WEB_SERIAL    // Отладка в веб

#ifdef USE_WEB_SERIAL
#include <WebSerial.h>
void recvMsg(uint8_t *data, size_t len);
#endif

const char* ssid = "Stend";
const char* password =  "12#04$19";
// const char* ssid = "DLink-Italgas";
// const char* password =  "DLink-Italgas74";

#ifdef WORKTPLATE
static const int OUTPUT0 = 12;   // вывод управления реле включения компрессора
static const int OUTPUT1 = 27;   // вывод управления реле включения рабочего клапана
static const int OUTPUT2 = 14;   // вывод управления реле включения аварийного клапана
static const int BUZZERPIN = 13; // вывод пьезоизлучателя
static const int RX_NEX = 22;
static const int TX_NEX = 23;
static const int RX_PRINTER = 26; //26
static const int TX_PRINTER = 25; //25
#endif


SoftwareSerial mySerial(RX_PRINTER, TX_PRINTER); // Declare SoftwareSerial obj first
#define no_PRINTER_RUS

// Adafruit_Thermal printer(&mySerial, 32); // Pass addr to printer constructor - быстрая печать без пауз между стоками (с обратной связью)
Adafruit_Thermal printer(&mySerial);            // Pass addr to printer constructor - медленная печать
Preferences flash;

uint8_t countererror, olderror;

Inputanalog pressuremeter0; // датчик давления на входе
Inputanalog pressuremeter1; // датчик давления контроль
Out output0;                // компрессор
Out output1;                // рабочий клапан
Out output2;                // клапан сброса

Ticker tickerprintDebugLog;
// Ticker tickerupdate;
// Ticker tickershowdigit;
Ticker tickergetkompressorTemperature;

#define ONE_WIRE_BUS 15
const float MAX_TEMPERATURE_KOMPRESSOR = 85.0;
const float MIN_TEMPERATURE_KOMPRESSOR = 55.0;
float test = 0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress kompressorThermometer;

TaskHandle_t Task_updatePressure;
TaskHandle_t Task_showNextion;
TaskHandle_t Task_readNextion;

AsyncWebServer server(80);

void updatepressure(void *pvParameters);
void printDebugLog();
void showdigit();
void errors();
void showNextion(void *pvParameters);
void readNextion(void *pvParameters);
void modeMenu();
void modeManual();
void modeSetting_auto();
void modeAuto();
void modeAutoPause();
void modeAutoEnd();
void showText(int color, String *incStr);
void printchek(String, int coun = 0);
void printerPrintLine(int n);

void showPageNextionMenu();
void showPageNextionAutoControl();
void showPageNextionManualControl();

void send(String dev);
void send(String dev, double data);
void send(String dev, String data);
void start_pump();
void stop_pump();

void manualpressuresetting (int settingpressmanual);
void printTemperature(DeviceAddress deviceAddress);
void getTemperature();

//  ошибки
struct error_type
{
  unsigned pres0_break : 1;   //  обрыв датчика 1
  unsigned pres0_closure : 1; //  замыкание датчика 1
  unsigned pres1_break : 1;   //  обрыв датчика 2
  unsigned pres1_closure : 1; //  замыкание датчика 2
  unsigned pres_a_leak : 1;   //  утечка газа при автоматическом контроле
  unsigned pres_off : 1;      //  включение клапана сброса при автоматическом контроле
  unsigned to_hight_temp_kompressor : 1; // перегрев компрессора
  unsigned no_found_temp_kompressor : 1; // не найден датчик температуры
} error;

String mode = "p0", incStr = "", vin = "", timebegin = "", date = "";

const int COUNTERGASPRESSURE = 4; // число ступеней контроля
const int MAXPRESSURE = 350;
const int MAXPRESSURECOMPRESSOR = 220; // давление отсечки компрессора
const int MAXVOLUMEPRESSERROR = 6;
int setting_press_manual;
int counter; //  ступень контроля
static esp_adc_cal_characteristics_t *adc_chars;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;
long timeWorkPress, timePause, timePauseStart;
int savePressure, counPause, fsavePressure;
int Vp1 = 4000, Vp2 = 300;
long periodTimeArray[COUNTERGASPRESSURE] = {60000, 60000, 120000, 300000}; //  время контроля каждой ступени
long timeControl;
unsigned long timeStartControl;
float kompressor_temperature;
boolean pauseAutoMode = false;
boolean startPump = false;
int counterErrorConnectTemper = 0;
