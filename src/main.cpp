#include "config.h"

#include <Wire.h>

#include <Encoder.h>
#include <EEPROM.h>

#include <si5351.h>
#include <wiring_private.h>

#include "ui/SMeter.h"
#include "ui/Band.h"
#include "button.h"
#include "ui/Pano.h"
#include "swr.h"
#include "menu/Action.h"
#include "menu/Menu.h"
#include "Storage.h"
#include "ui/Spinner.h"


volatile long setupCalls = 0;
volatile long encoderPosition = 0;
volatile unsigned long loopMS;
bool menuPreviousState = false;
//
Message subMenuScreen = MsgExit;
//
Encoder freqEncoder(ENCODER_LEFT_PIN, ENCODER_RIGHT_PIN); // pin (2 = D2, 3 = D3)
Button *freqEncButton = new Button(ENCODER_PUSH_PIN);
Si5351 *si5351 = new Si5351();



Button *txButton = new Button(TX_BTN_PIN);
Button *menuButton = new Button(MEM_BTN_PIN);   // PC3
Button *modeButton = new Button(MODE_BTN_PIN); // PC4
Button *vfoButton = new Button(VFO_BTN_PIN);   // PC5
Button *stepButton = new Button(STEP_BTN_PIN); // PA5
Button *bandButton = new Button(BAND_BTN_PIN); // PA4

SWRMeter swrMeter(SWR_REF_INPUT_PIN, SWR_FOR_INPUT_PIN);

Display *display = new Display(&tft);
SMeter sMeter(SMETER_INPUT_PIN, display);

Pano *pano = new Pano(PANO_INPUT_PIN, si5351, &state, display);
Band *band = new Band(display, &state);


Message mainMenuMessages[] = {
        MsgMemory,
        MsgSWR,
        MsgCW,
        MsgSettings,
        MsgAbout,
        MsgExit
};
Message settingsMenuMessages[] = {
        MsgIF,
        MsgSSBOffset,
        MsgDDSCalibration,
        MsgSWRCalibration,
        MsgTogglePano,
        MsgExit
};

Message memoryMenuMessages[] = {
        MsgLoadFromTheCell,
        MsgSaveToNewCell,
        MsgErase,
        MsgExit
};

Menu mainMenu(display, mainMenuMessages, 6);
Menu settingsMenu(display, settingsMenuMessages, 5);
Menu memoryMenu(display, memoryMenuMessages, 4);

Menu *currentMenu = &mainMenu;

Storage *storage = new Storage(0x50);



Spinner<uint32_t> *spinner = new Spinner<uint32_t>(display);
Spinner<int32_t> *ssbSpinner = new Spinner<int32_t>(display);


int64_t getIntermediateFrequency() {
    int64_t fOffset;
    switch (state.mode) {
        case LSB:
            fOffset = state.ssbOffset;
            break;
        case USB:
            fOffset = -state.ssbOffset;
            break;
        case CW:
            fOffset = -state.cwOffset;
            break;
        default:
            fOffset = 0;
    }
    return state.iFrequency + fOffset;
}

void setFrequency() {
    oFrequency = state.frequency;
    si5351->set_freq(
            static_cast<uint64_t>(state.frequency + getIntermediateFrequency() +
                                  (state.isRIT && !state.tx ? state.RITFrequency : 0)) * 100ULL,
            SI5351_CLK0
    );
    if (!currentMenu->isActive()) {
        displayFrequency();
        band->loop();
        // displayScale(false);
    }
}

void render() {
    tft.fillScreen(ST77XX_BLACK);
    displayModulation();
    displayMode();
    band->draw();
    changeFrequencyStep(0);
    // displayScale(true);
    displayFrequency();
    sMeter.setup();
    displayRIT();
    displayVFO();
    //displaySWR();
}

/*
TX - must be separate, external 1k
BAND short, MENU long
MODE
RIT

*/
unsigned long bs;

void switchBand() {
    switch (subMenuScreen) {
        case MsgExit:
            band->next();
            // displayScale(true);
            break;
        default:
            ;
            // nothing
    }
}


void setIntermediateFrequency() {
    si5351->set_freq(
            static_cast<uint64_t>(getIntermediateFrequency()) * 100ULL,
            SI5351_CLK1
    );
}

void switchBacklight() {
    int led = digitalRead(BACKLIGHT_PIN);
    if (led == LOW) {
        led = HIGH;
    } else {
        led = LOW;
    }
    digitalWrite(BACKLIGHT_PIN, static_cast<uint8_t>(led));
}

void enableCW() {
    state.mode = CW;
    displayWPM();
}

void enableLSB() {
    state.mode = LSB;
}

void enableUSB() {
    state.mode = USB;
}

void enableAM() {
    state.mode = AM;
}

void enableFM() {
    state.mode = FM;
}

void switchModulation() {
    switch (subMenuScreen) {
        case MsgExit:
            switch (state.mode) {
                case CW:
                    enableLSB();
                    break;
                case LSB:
                    enableUSB();
                    break;
                case USB:
                    enableAM();
                    break;
                case AM:
                    enableFM();
                    break;
                case FM:
                    enableCW();
                    break;
            }
            displayModulation();
            setFrequency();

            break;
        default:
            ;
    }
}

void switchVFO() {
    if (subMenuScreen != MsgExit) {
        state.isAltFrequency = !state.isAltFrequency;
        uint32_t t = state.frequency;
        state.frequency = state.altFrequency;
        state.altFrequency = t;
        displayVFO();
        setFrequency();
    }
}


void stepButtonShortClickCb() {
    switch (subMenuScreen) {
        case MsgIF:
            spinner->changeStep();
            spinner->loop();
            break;
        case MsgSSBOffset:
            ssbSpinner->changeStep();
            ssbSpinner->loop();
            break;
        default:
            changeFrequencyStep(1);
    }
}

void menuClick() {
    if (&(*currentMenu) != &mainMenu) {
        currentMenu = &mainMenu;
    }
    if (!currentMenu->isActive()) {
        currentMenu->setActive(true);
    }
    currentMenu->render();
}

void exitMenu() {
    currentMenu->exit();
}

void saveStateToACell() {
    display->clear();
    display->textxy(20, 70, F("Saving..."), ST7735_WHITE, ST7735_BLACK);
    EEPROM.put(0, state);

    display->clear();
    render();
    // storage->saveState(&state, 0);
    // exitMenu();
}

void displayAbout() {
    display->clear();
    subMenuScreen = MsgAbout;
    display->textxy(20, 10, F("SSB85 Transceiver"), ST7735_WHITE, ST7735_BLACK);
    display->textxy(20, 20, F("Version: 1.0.0"), COLOR_MEDIUM_RED, ST7735_BLACK);
    display->textxy(74, 30, F("2019-05-07"), COLOR_MEDIUM_RED, ST7735_BLACK);
    display->textxy(1, 50, F("Details & updates online:"), COLOR_GRAY_MEDIUM, ST7735_BLACK);
    display->textxy(1, 65, F("zoonman.com/projects/ssb85"), COLOR_BRIGHT_BLUE, ST7735_BLACK);
    display->textxy(0, 110, F("Built by Philipp Tkachev"), COLOR_DARK_GREEN, ST7735_BLACK);
}

void displayAllEeprom() {
    display->clear();
    EEPROM.get(0, state);
    subMenuScreen = MsgExit;
    render();
}

void eraseEeprom() {
    display->clear();
    display->drawRoundTextBox(0, 0, TFT_WIDTH, TFT_HEIGHT, MsgErasing, ST7735_WHITE, ST7735_BLACK);
    for (int i = 0 ; i < EEPROM.length() ; i++) {
        EEPROM.write(i, 255);
    }
    display->clear();
    render();
}

void displayIntermediateFrequencySettings() {
    display->clear();
    subMenuScreen = MsgIF;
    display->drawRoundTextBox(0, 0, TFT_WIDTH, 22, MsgSSB85, ST7735_WHITE, ST7735_BLACK);
    spinner->setLeft(5);
    spinner->setLabel(MsgIF);
    spinner->setTop(40);
    spinner->setWidth(110);
    spinner->setHeight(32);
    spinner->setFocus(true);
    spinner->setVisibility(true);
    spinner->setValue(state.iFrequency);
    spinner->draw();
}

void displaySSBOffsetSettings() {
    display->clear();
    subMenuScreen = MsgSSBOffset;
    display->drawRoundTextBox(0, 0, TFT_WIDTH, 22, MsgSSB85, ST7735_WHITE, ST7735_BLACK);
    ssbSpinner->setLeft(5);
    ssbSpinner->setLabel(MsgSSBOffset);
    ssbSpinner->setTop(40);
    ssbSpinner->setWidth(110);
    ssbSpinner->setHeight(32);
    ssbSpinner->setFocus(true);
    ssbSpinner->setVisibility(true);
    ssbSpinner->setValue(state.ssbOffset);
    ssbSpinner->draw();
}


class App {
    private:
    Task **tasks;
    size_t length = 0;
    size_t nextTask = 0;

    void AddWidget(Task *task) {
        size_t nl = ++this->length;
        this->tasks = (Task * *)realloc(this->tasks, nl * sizeof(*task));
        this->tasks[this->length-1] = task;
    }

    void loop() {
        if (--this->nextTask > -1) {
            this->tasks[this->nextTask]->loop();
        } else {
            this->nextTask = this->length;
        }
    }
};


void callMenuFunc(Message m) {
    switch (m) {
        case MsgAbout:
            currentMenu->exit();
            displayAbout();
            break;
        case MsgMemory:
            currentMenu = &memoryMenu;
            currentMenu->setActive(true);
            currentMenu->render();
            break;
        case MsgSettings:
            currentMenu = &settingsMenu;
            currentMenu->setActive(true);
            currentMenu->render();
            break;
        case MsgIF:
            currentMenu->exit();
            displayIntermediateFrequencySettings();
            break;
        case MsgLoadFromTheCell:
            currentMenu->exit();
            displayAllEeprom();
            break;
        case MsgErase:
            currentMenu->exit();
            eraseEeprom();
            break;
        case MsgTogglePano:
            state.isPanoEnabled = ! state.isPanoEnabled;
            currentMenu->exit();
            break;
        case MsgSSBOffset:
            currentMenu->exit();
            displaySSBOffsetSettings();
            break;
        case MsgExit:
        case MsgCW:
        case MsgDDSCalibration:
        case MsgSWRCalibration:
            currentMenu->exit();
            if (currentMenu == &settingsMenu) {
                currentMenu = &mainMenu;
            }
            display->clear();
            render();
            break;
        default:
            ;
    }
}

void encoderClickHandler() {
    if (currentMenu->isActive()) {
        callMenuFunc(currentMenu->getActiveMessage());
    } else if (subMenuScreen != MsgExit) {
        //
        switch (subMenuScreen) {
            case MsgIF:
                state.iFrequency = spinner->getValue();
                display->clear();
                saveStateToACell();
                subMenuScreen = MsgExit;
                break;
            case MsgSSBOffset:
                state.ssbOffset = ssbSpinner->getValue();
                display->clear();
                saveStateToACell();
                subMenuScreen = MsgExit;
                break;
            case MsgAbout:
                display->clear();
                render();
                subMenuScreen = MsgExit;
                break;
            default:
                ;
        }
    } else {
        state.isRIT = !state.isRIT;
        displayRIT();
        setFrequency();
    }
}

void encoderCW() {
    switch (subMenuScreen) {
        case MsgIF:
            spinner->inc();
            spinner->loop();
            state.iFrequency = spinner->getValue();
            setIntermediateFrequency();
            break;
        case MsgSSBOffset:
            ssbSpinner->inc();
            ssbSpinner->loop();
            state.ssbOffset = ssbSpinner->getValue();
            setIntermediateFrequency();
            break;
        default:
            ;
    }
}

void encoderCCW() {
    switch (subMenuScreen) {
        case MsgIF:
            spinner->dec();
            spinner->loop();
            state.iFrequency = spinner->getValue();
            setIntermediateFrequency();
            break;
        case MsgSSBOffset:
            ssbSpinner->dec();
            ssbSpinner->loop();
            state.ssbOffset = ssbSpinner->getValue();
            setIntermediateFrequency();
            break;
        default:
            ;
    }
}

/****************************************************************
 *
 *   SETUP
 *
 ****************************************************************/

void setup() {
    setupCalls++;
    // delay(1000);

    pinMode(28, INPUT);
    analogReference(INTERNAL2V56);

    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);


    pinMode(TFT_BACKLIGHT_PIN, OUTPUT);


    // analogWrite(BACKLIGHT_PIN, 250);
    si5351->init(SI5351_CRYSTAL_LOAD_0PF, 0, 0);


    tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab

    // put your setup code here, to run once:
    tft.fillScreen(ST77XX_BLACK);
    tft.setRotation(1);

    loopMS = millis();

    band->setVisibility(true);


    //
    //
    //Serial.begin(9600);
    //Serial.println("TWBR");
    //Serial.println(TWBR);
    //Serial.println(TWSR);
    // TWBR
    // Wire.

//    Wire.setClock(400000);
    //Serial.println("--");
    //Serial.println(TWBR);
    //Serial.println(TWSR);
    si5351->output_enable(SI5351_CLK0, 1);
    si5351->output_enable(SI5351_CLK1, 1);
    si5351->output_enable(SI5351_CLK1, 1);
    si5351->drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
    si5351->drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);
    si5351->drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);

    setIntermediateFrequency();

/**/
    state.frequency = START_FREQUENCY;
    state.altFrequency = START_FREQUENCY + 1000;

    setFrequency();
    freqEncoder.write(encoderPosition);

    setFrequency();


    render();
    band->loop();

    txButton->setup();
    freqEncButton->setup();
    modeButton->setup();
    vfoButton->setup();
    stepButton->setup();
    menuButton->setup();
    bandButton->setup();

    sMeter.setup();
    swrMeter.setup();

    freqEncButton->registerShortPressCallback(&encoderClickHandler);
    freqEncButton->registerLongPressCallback(&switchBand);

    stepButton->registerShortPressCallback(&stepButtonShortClickCb);
    bandButton->registerShortPressCallback(&switchBand);

    modeButton->registerShortPressCallback(&switchModulation);
    modeButton->registerLongPressCallback(&switchBacklight);

    vfoButton->registerShortPressCallback(&switchVFO);
    // stepButton->registerShortPressCallback(&stepButtonShortClickCb);


    menuButton->registerShortPressCallback(&menuClick);
    //delay(100);

    delay(10);
    //storage->loadState(&state, 0);
    delay(10);

    menuPreviousState = currentMenu->isActive();
    // turn on led
    digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

    band->draw();
}



void renderTXUI() {
    tft.fillRect(0, FREQUENCY_Y, TFT_WIDTH, TFT_HEIGHT - FREQUENCY_Y, ST77XX_BLACK);
    displayMode();
    setFrequency();
    swrMeter.render();
}

void renderRXUI() {
    tft.fillRect(0, FREQUENCY_Y, TFT_WIDTH, TFT_HEIGHT - FREQUENCY_Y, ST77XX_BLACK);
    displayMode();
    displaySMeter();
    setFrequency();
    // displayScale(true);
    sMeter.drawLevel(1);
    sMeter.drawLevel(12);
    band->loop();
}


// MAIN LOOP ===================================================================
void loop() {
    // return;
    txButton->loop();

    if (txButton->isPressed() && state.tx == 0) {
        state.tx = true;
        if (!currentMenu->isActive() && subMenuScreen == MsgExit) {
            renderTXUI();
        }
    } else if (txButton->isReleased() && state.tx != 0 && subMenuScreen == MsgExit) {
        state.tx = false;
        // getting back in rx
        if (!currentMenu->isActive()) {
            renderRXUI();
        }
    }
    yield();

    if (currentMenu->isActive() != menuPreviousState) {
        // we should render UI=
        menuPreviousState = currentMenu->isActive();
    }

    if (state.tx) {
        swrMeter.loop();
    } else {
        auto currentMS = millis();

        // save cycles, we don't have to evaluate all that stuff with 16MHz frequency
        if (currentMS - loopMS > 10) {

            sMeter.loop();
            freqEncButton->loop();
            modeButton->loop();
            vfoButton->loop();
            stepButton->loop();
            menuButton->loop();
            bandButton->loop();

            long int lastEncoderPosition = freqEncoder.read();
            if (lastEncoderPosition != encoderPosition && freqEncButton->isPressed()) {
                freqEncButton->disable();
            }
            // turning the knob counter-clockwise
            if (lastEncoderPosition > encoderPosition + 2) {
                if (currentMenu->isActive()) {
                    currentMenu->down();
                } else if (freqEncButton->isPressed()) {
                    changeFrequencyStep(-1);
                } else if (state.isRIT) {
                    if (state.RITFrequency > -9999) {
                        state.RITFrequency--;
                    }
                    if (!currentMenu->isActive()) {
                        displayRIT();
                    }
                } else if (subMenuScreen != MsgExit) {
                    encoderCCW();
                } else {
                    state.frequency -= state.step;
                    band->loop();
                }
                encoderPosition = lastEncoderPosition;
            } else if (lastEncoderPosition < encoderPosition - 2) {
                // turning the knob clockwise
                if (currentMenu->isActive()) {
                    currentMenu->up();
                } else if (freqEncButton->isPressed()) {
                    changeFrequencyStep(1);
                } else if (state.isRIT) {
                    if (state.RITFrequency < 9999) {
                        state.RITFrequency++;
                    }
                    if (!currentMenu->isActive()) {
                        displayRIT();
                    }
                } else if (subMenuScreen != MsgExit) {
                    encoderCW();
                } else {
                    state.frequency += state.step;
                    band->loop();
                }
                encoderPosition = lastEncoderPosition;
            }

            if (state.frequency > UPPER_RX_BOUND) {
                state.frequency = LOWER_RX_BOUND;
            } else if (state.frequency < LOWER_RX_BOUND) {
                state.frequency = UPPER_RX_BOUND;
            }

            if (oFrequency != state.frequency) {
                setFrequency();
            }

            if (!currentMenu->isActive()) {
                yield();
                // A4 = PA3 = ADC3
                // set  ADC Multiplexer Selection Register
                ADMUX = (_BV(MUX0) | _BV(MUX1) // select ADC3 input
                 | _BV(REFS0) | _BV(REFS1)); // pull 2.65 V REF
                cbi(ADMUX, ADLAR); // turn of left adjust
                cbi(ADMUX, MUX3); // turn off gain
                cbi(ADMUX, MUX4); // turn off differential input

                cbi(ADCSRA, ADATE);
                cbi(ADCSRA, ADIE);

                sbi(ADCSRA, ADEN); // enable ADC
                cbi(PRR0, PRADC); // enable power reduction ADC
                sbi(ADCSRA, ADSC); // turn on ADC
                while (bit_is_set(ADCSRA, ADSC));
                uint8_t low  = ADCL;
                uint8_t high = ADCH;
                cbi(ADCSRA, ADEN);
                uint16_t voltage = (((high << 8) | low) & 0x03FF) + 1;
                auto av = (float)voltage / 37.5;
                auto str1 = String(av, 2);
                str1.concat("V");
                if (subMenuScreen == MsgExit) {
                    display->textxy(50, RIT_Y, &str1, COLOR_DARK_GREEN, ST7735_BLACK);
                }
            }

            loopMS = currentMS;
            // settle
            yield();
        }
        if (!currentMenu->isActive() && subMenuScreen == MsgExit) {
            pano->loop();
            band->loop();
        }
        yield();
    }
/*
 * Render pool
 *
 * registers rendering elements
 * has flag to re-render
 * cycles through loop
 * renders one component per cycle
 *
 * **/
    yield();
}

