//
// Created by Philipp Tkachev on 2019-05-20.
//

#include "Spinner.h"

template <typename T>
void Spinner<T>::draw() {
    if (!this->isVisible) {
        return;
    }
    String text(this->value);
    this->display->drawTextBox(
            this->left,
            this->top,
            this->width,
            this->height / uint8_t(2),
            this->label,
            this->color,
            ST7735_BLACK,
            AlignLeft
    );
    this->display->drawRoundTextBox(
            this->left,
            this->top + this->height / uint8_t(2),
            this->width - this->height / uint8_t(2),
            this->height / uint8_t(2),
            &text,
            this->color,
            ST7735_BLACK
    );

    // arrow up
    this->display->tft->drawTriangle(
            this->width - this->height / uint8_t(4),
            this->top + this->height / uint8_t(4) * uint8_t(3) - 2,

            this->width - this->height / uint8_t(8),
            this->top + this->height / uint8_t(2),

            this->width,
            this->top + this->height / uint8_t(4) * uint8_t(3) - 2,

            this->color
    );

    this->display->tft->drawTriangle(
            this->width - this->height / uint8_t(4),
            this->top + this->height / uint8_t(4) * uint8_t(3) + 1,

            this->width - this->height / uint8_t(8) ,
            this->top + this->height - 1,

            this->width,
            this->top + this->height / uint8_t(4) * uint8_t(3) + 1,

            this->color
    );


    Bounds t = {};
    this->display->tft->getTextBounds(
            text,
            this->left,
            this->top + this->height / uint8_t(2),
            &t.x,
            &t.y,
            &t.w,
            &t.h
    );


    this->display->tft->drawFastHLine(
            this->left +
            (this->width - this->height / uint8_t(2))/2 + t.w/2 - (this->lg() * 6),
            this->top + this->height - 3,
            5,
            this->color
    );
}

template <typename T>
Spinner<T>::Spinner(Display *display) {
    this->step = 1;
    this->display = display;
    this->left = 0;
    this->top = 0;
    this->width = static_cast<uint8_t>(this->display->tft->width());
    this->height = 32;
    this->color = COLOR_GRAY_MEDIUM;
    this->isFocused = false;
}

template <typename T>
void Spinner<T>::inc() {
    this->value += this->step;
    this->isRedraw = true;
}

template <typename T>
void Spinner<T>::dec() {
    this->value -= this->step;
    this->isRedraw = true;
}

template <typename T>
T Spinner<T>::getValue() {
    return this->value;
}

template <typename T>
void Spinner<T>::setValue(T value) {
    this->value = value;
    this->isRedraw = true;
}

template <typename T>
void Spinner<T>::loop() {
    if (this->isRedraw) {
        this->draw();
    }
}

template <typename T>
void Spinner<T>::changeStep() {
    if ((this->step * 10u) > this->value) {
        this->step = 1;
    } else {
        this->step *= 10u;
    }
    this->isRedraw = true;
}

template <typename T>
uint8_t Spinner<T>::lg() {
    T s = this->step;
    uint8_t i = 1;
    while (s > 1) {
        s /= 10;
        i++;
    }
    return i;
}

// required to make a proper linker work
template class Spinner<uint32_t>;
template class Spinner<int32_t>;