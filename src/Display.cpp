//
// Created by Philipp Tkachev on 2019-03-08.
//

#include "Display.h"

void Display::setTFT(Adafruit_ST7735 *t) {
    this->tft = t;
};

void Display::textxy(uint16_t x, uint16_t y, String *text, uint16_t c, uint16_t b) {
    this->tft->setTextColor(c, b);
    this->tft->setCursor(x, y);
    this->tft->print(*text);
}

void Display::textxy(uint16_t x, uint16_t y, const __FlashStringHelper *ifsh, uint16_t c, uint16_t b) {
    this->tft->setTextColor(c, b);
    this->tft->setCursor(x, y);
    this->tft->print(ifsh);
}

void Display::drawRoundTextBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, String *text, uint16_t c, uint16_t bg) {
    this->tft->fillRoundRect(x, y, w, h, 2, bg);
    this->tft->drawRoundRect(x, y, w, h, 2, c);
    Bounds t = {};
    this->tft->getTextBounds(*text, x, y, &t.x, &t.y, &t.w, &t.h);
    this->textxy(x + (w - t.w)/2, y + (h - t.h) / 2 + 1, text, c, bg);
}

void Display::drawRoundTextBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const __FlashStringHelper *ifsh, uint16_t c, uint16_t bg) {
    this->tft->fillRoundRect(x, y, w, h, 2, bg);
    this->tft->drawRoundRect(x, y, w, h, 2, c);
    Bounds t = {};
    this->tft->getTextBounds(ifsh, x, y, &t.x, &t.y, &t.w, &t.h);
    this->textxy(x + (w - t.w)/2, y + (h - t.h) / 2 + 1, ifsh, c, bg);
}

Display::Display(Adafruit_ST7735 *tft) : tft(tft) {}