#include <Wire.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SoftwareSerial.h>
#include <HardwareSerial.h>
#include <JC_Button.h>
#include <Hash.h>
#include <ArduinoJson.h>
#include "qrcoded.h"
#include "Bitcoin.h"
#include <Adafruit_Thermal.h>
#include "mbedtls/aes.h"
#include "mbedtls/md5.h"
#include <TFT_eSPI.h>
#define FORMAT_ON_FAIL true
#define PARAM_FILE "/elements.json"

///////////////////////////////////////////////////
//////CHANGE MANUALLY OR USE FOSSA.lnbits.com//////
///////////////////////////////////////////////////

#define BTN1 39        // Screen tap button
#define BILL_RX 32     // RX Bill acceptor
#define BILL_TX 33     // TX Bill acceptor
#define COIN_TX 4      // TX Coinmech
#define COIN_INHIBIT 2 // Coinmech
#define PRINTER_RX 22  // RX of the thermal printer
#define PRINTER_TX 23  // TX of the thermal printer

// default settings
#include "hardcoded_user_config.h"

// Web installer build (-DINSTALLER_BUILD): settings are written via USB serial, never hardcoded
#ifdef INSTALLER_BUILD
#undef HARDCODED
#define HARDCODED false
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
// Time window after boot in which a screen tap (or a serial /config) starts the config mode
#define CONFIG_TAP_WINDOW_MS 6000

bool hardcoded = HARDCODED; // set to true if you want to use the above hardcoded settings
bool printerBool = false;
float billAmountFloat[10];
float coinAmountFloat[10];

///////////////////////////////////////////////////
///////////////////////////////////////////////////
///////////////////////////////////////////////////

String deviceString = DEVICE_STRING;
String language = LANGUAGE;
String coinAmounts = COIN_AMOUNTS;
String billAmounts = BILL_AMOUNTS;
int charge = CHARGE;
int maxAmount = MAX_AMOUNT;
int maxBeforeReset = MAX_BEFORE_RESET;

String baseURLATM;
String baseUrlAtmPage;
String secretATM;
String currencyATM;

fs::SPIFFSFS &FlashFS = SPIFFS;

String qrData;

int maxBeforeResetTally;
int bills;
float coins;
float total;
float billAmountSize = sizeof(billAmountFloat) / sizeof(float);
float coinAmountSize = sizeof(coinAmountFloat) / sizeof(float);
int moneyTimer = 0;
bool waitForTap = true;
struct KeyValue {
  String key;
  String value;
};
String translate(String key);
uint16_t homeScreenColors[] = { TFT_GREEN, TFT_BLUE, TFT_ORANGE };
int homeScreenNumColors = sizeof(homeScreenColors) / sizeof(homeScreenColors[0]);
int homeScreenNumColorCount = 0;

String usbT, tapScreenT, scanMeT, totalT, fossaT, satsT, forT, fiatT, feedT, chargeT, printingT, waitT, workingT, thisVoucherT, ofBitcoinT, thankYouT, scanMeClaimT, tooMuchFiatT, contactOwnerT;

HardwareSerial SerialPort1(1);
SoftwareSerial printerSerial(PRINTER_RX, PRINTER_TX);

// Coin pulse counter (interrupt-driven, pulled-to-GND pulses on COIN_TX)
// 2 pulses=0.05 EUR  3=0.10  4=0.20  5=0.50  6=1.00  7=2.00
#ifndef COIN_DEBOUNCE_MS
  #define COIN_DEBOUNCE_MS 50   // min ms between two valid pulses
#endif
#ifndef COIN_EVAL_MS
  #define COIN_EVAL_MS 400      // ms of silence before evaluating pulse count
#endif
volatile int       coinPulseCount    = 0;
volatile uint32_t  lastCoinPulseMs   = 0;
void IRAM_ATTR coinPulseISR() {
  uint32_t now = (uint32_t)millis();
  if (now - lastCoinPulseMs >= COIN_DEBOUNCE_MS) {
    coinPulseCount++;
    lastCoinPulseMs = now;
  }
}
Adafruit_Thermal printer(&printerSerial);
TFT_eSPI tft = TFT_eSPI(320, 480);
Button BTNA(BTN1);

void setup() {
  translateAll(language);
  Serial.setRxBufferSize(2048); // config lines can be several hundred bytes
  Serial.begin(115200);
  Serial.println(workingT);
  Serial.println("Fossa firmware " FIRMWARE_VERSION);
  BTNA.begin();
  FlashFS.begin(FORMAT_ON_FAIL);
  tft.init();
#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 1
#endif
  tft.setRotation(DISPLAY_ROTATION);
  tft.invertDisplay(false);
  printMessage("", "Loading..", "", TFT_WHITE, TFT_BLACK);
  printMessage("", "Loading..", "", TFT_BLACK, TFT_WHITE);

  // No stored config and not hardcoded -> stay in config mode until /config-done (web installer)
  if (!hardcoded && !FlashFS.exists(PARAM_FILE)) {
    Serial.println("/config-mode no config found");
    printMessage(usbT, "", "", TFT_WHITE, TFT_BLACK);
    executeConfig(false); // restarts the device after /config-done
  }

  // wait a few seconds for a tap (or a serial /config) to start config mode
  unsigned long tapStart = millis();
  while (waitForTap && millis() - tapStart < CONFIG_TAP_WINDOW_MS) {
    BTNA.read();
    if (BTNA.wasReleased()) {
      Serial.println("/config-mode tap");
      printMessage(usbT, "", tapScreenT, TFT_WHITE, TFT_BLACK);
      executeConfig(true);
      waitForTap = false;
    }
    // lets the web installer enter config mode without touching the screen
    if (Serial.available() > 0) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd == "/ping") Serial.println("/pong " FIRMWARE_VERSION);
      if (cmd == "/config") {
        Serial.println("/config-mode serial");
        printMessage(usbT, "", "", TFT_WHITE, TFT_BLACK);
        executeConfig(false); // restarts the device after /config-done
      }
    }
    delay(20);
  }
  total = 0;

  if(hardcoded == false){
    readFiles();
  }
  else{
    printDefaultValues();
  }
  splitSettings(deviceString);

  // initialize bill acceptor
  SerialPort1.begin(300, SERIAL_8N2, BILL_TX, BILL_RX);
  printerSerial.begin(9600);
  // coin pulse input: mechanism pulls COIN_TX to GND for each pulse
  pinMode(COIN_TX, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_TX), coinPulseISR, FALLING);
#if !COIN_INHIBIT_BYPASS
  pinMode(COIN_INHIBIT, OUTPUT);
#endif
}

void loop() {
  if (maxBeforeResetTally >= maxBeforeReset) {
    printMessage("", tooMuchFiatT, contactOwnerT, TFT_WHITE, TFT_BLACK);
    delay(100000000);
  } else {
    // initialize printer

    SerialPort1.write(184);
#if !COIN_INHIBIT_BYPASS
    digitalWrite(COIN_INHIBIT, HIGH);
#endif
    tft.fillScreen(TFT_BLACK);
    BTNA.read();
    if (SerialPort1.available()) {
      Serial.println("Bill acceptor connected");
    }
    if (SerialPort1.available()) {
      Serial.println("Coin acceptor connected");
    }
    moneyTimerFun();
    Serial.println("total" + String(total));
    Serial.println("maxBeforeResetTally" + String(maxBeforeResetTally));
    maxBeforeResetTally = maxBeforeResetTally + (total / 100);
    Serial.println("maxBeforeResetTally" + String(maxBeforeResetTally));
    makeLNURL();
    qrShowCodeLNURL(scanMeT);
  }
}

void moneyTimerFun() {
  waitForTap = true;
  coins = 0;
  bills = 0;
  total = 0;
  while (waitForTap || total == 0) {
    if (homeScreenNumColorCount == homeScreenNumColors) {
      homeScreenNumColorCount = 0;
    }
    // Only redraw home screen when no coin pulses are in progress
    bool coinQuiet = coinPulseCount == 0 && ((uint32_t)millis() - lastCoinPulseMs) > COIN_EVAL_MS;
    if (total == 0 && coinQuiet) {
      waitForTap = true;
      feedmefiat();
      feedmefiatloop();
    }
    if (SerialPort1.available()) {
      int x = SerialPort1.read();
      for (int i = 0; i < billAmountSize; i++) {
        if ((i + 1) == x) {
          bills = bills + billAmountFloat[i];
          total = (coins + bills);
          printMessage(billAmountFloat[i] + currencyATM, totalT + String(total) + currencyATM, tapScreenT, TFT_WHITE, TFT_BLACK);
        }
      }
    }
    // Coin pulse evaluation: wait COIN_EVAL_MS after last pulse, then map count→value
    if (coinPulseCount > 0 && ((uint32_t)millis() - lastCoinPulseMs) > COIN_EVAL_MS) {
      int pulses = coinPulseCount;
      coinPulseCount = 0;
      int idx = pulses - 2;  // 2 pulses→index 0 (0.05 EUR) … 7 pulses→index 5 (2 EUR)
      if (idx >= 0 && idx < (int)coinAmountSize) {
        coins = coins + coinAmountFloat[idx];
        total = (coins + bills);
        printMessage(String(coinAmountFloat[idx]) + currencyATM, totalT + String(total) + currencyATM, tapScreenT, TFT_WHITE, TFT_BLACK);
      } else {
        // Invalid pulse count (noise) — refresh the cooldown timer so the
        // home screen is not immediately redrawn while the coin is still dropping
        Serial.println("Unknown coin pulse count: " + String(pulses));
        lastCoinPulseMs = (uint32_t)millis();
      }
    }
    BTNA.read();
    if (BTNA.wasReleased() || total > maxAmount) {
      waitForTap = false;
    }
    homeScreenNumColorCount++;
  }
  total = (coins + bills) * 100;

  // Turn off machines
  SerialPort1.write(185);
#if !COIN_INHIBIT_BYPASS
  digitalWrite(COIN_INHIBIT, LOW);
#endif
}
