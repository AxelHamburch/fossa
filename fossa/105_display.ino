

// Split a string near the middle at a space and print two centered lines
void printCenteredTwoLines(String text, int textSize, int yTop) {
  int w = tft.width();
  int mid = text.length() / 2;
  int splitPos = mid;
  for (int d = 0; d <= mid; d++) {
    if (mid - d >= 0 && text.charAt(mid - d) == ' ') { splitPos = mid - d; break; }
    if (mid + d < (int)text.length() && text.charAt(mid + d) == ' ') { splitPos = mid + d; break; }
  }
  String line1 = text.substring(0, splitPos);
  String line2 = text.substring(splitPos + 1);
  int lineH = textSize * 8 + 4;
  tft.setTextSize(textSize);
  tft.setCursor((w - textWidth(line1, textSize)) / 2, yTop);
  tft.println(line1);
  tft.setCursor((w - textWidth(line2, textSize)) / 2, yTop + lineH);
  tft.println(line2);
}

void printMessage(String text1, String text2, String text3, int ftcolor, int bgcolor) {
  int w = tft.width();
  int h = tft.height();
  tft.fillScreen(bgcolor);
  tft.setTextColor(ftcolor, bgcolor);
  tft.setTextSize(4);
  tft.setCursor((w - textWidth(text1, 4)) / 2, h / 8);
  tft.println(text1);
  tft.setTextSize(3);
  tft.setCursor((w - textWidth(text2, 3)) / 2, h * 3 / 8);
  tft.println(text2);
  printCenteredTwoLines(text3, 2, h * 5 / 8);
}
void feedmefiat() {
  int w = tft.width();
  int h = tft.height();
  tft.setTextColor(TFT_WHITE);
  tft.setCursor((w - textWidth(fossaT, 2)) / 2, h / 8);
  tft.setTextSize(2);
  tft.println(fossaT);
  tft.setCursor((w - textWidth(feedT + " " + String(charge) + chargeT, 2)) / 2, h * 7 / 8);
  tft.setTextSize(2);
  tft.println(feedT + " " + String(charge) + chargeT);
}
void feedmefiatloop() {
  int w = tft.width();
  int h = tft.height();
  tft.setTextColor(homeScreenColors[homeScreenNumColorCount]);
  tft.setTextSize(8);
  tft.setCursor((w - textWidth(satsT, 8)) / 2, h / 4);
  tft.println(satsT);
  tft.setCursor((w - textWidth(forT, 8)) / 2, h * 7 / 16);
  tft.println(forT);
  tft.setCursor((w - textWidth(fiatT, 8)) / 2, h * 5 / 8);
  tft.println(fiatT);
  delay(100);
}
void qrShowCodeLNURL(String message) {
  Serial.println(qrData);
  tft.fillScreen(TFT_WHITE);
  const char *qrDataChar = qrData.c_str();
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(20)];
  qrcode_initText(&qrcoded, qrcodeData, 11, 0, qrDataChar);

  // Reserve ~50 px at the bottom for two lines of text
  int reservedBottom = 50;
  int qrPx = qrcoded.size * 4;
  int qrX  = (tft.width()  - qrPx) / 2;
  int qrY  = (tft.height() - reservedBottom - qrPx) / 2;

  for (uint8_t y = 0; y < qrcoded.size; y++) {
    for (uint8_t x = 0; x < qrcoded.size; x++) {
      if (qrcode_getModule(&qrcoded, x, y)) {
        tft.fillRect(qrX + 4 * x, qrY + 4 * y, 4, 4, TFT_BLACK);
      } else {
        tft.fillRect(qrX + 4 * x, qrY + 4 * y, 4, 4, TFT_WHITE);
      }
    }
  }

  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  printCenteredTwoLines(message, 2, tft.height() - reservedBottom + 4);
  Serial.println("Printing if printer exists");
  printReceipt();
  // Minimum lock-out before tap is accepted (prevents accidental dismiss)
  delay(5000);
  waitForTap = true;
  while (waitForTap) {
    BTNA.read();
    if (BTNA.wasReleased()) {
      waitForTap = false;
    }
  }
}
int textWidth(String text, int textSize) {
  tft.setTextSize(textSize);
  return tft.textWidth(text);
}