#include "LCD.h"

#define RnW_PIN         9
#define RS_PIN          10
#define BACKLIGHT_PIN   11
#define E_PIN           12

#define CMD_CLEAR       0x01
#define CMD_HOME        0x02
#define CMD_ENTRY_MODE  0x04
#define CMD_CONTROL     0x08
#define CMD_SHIFT       0x10
#define CMD_FUNCTION    0x20
#define CMD_SET_DDRAM   0x80

LCD::LCD(uint8_t mcpResetPin, uint8_t mcpAddress){
    this->_mcpAddress = mcpAddress;
    this->_mcpResetPin = mcpResetPin;
}

LCD::~LCD()
{
}

//public
void LCD::begin(){
    if(this->_mcpResetPin == 255){
        Serial.println(F("Display error: No valid reset pin specified."));
        while(1);
    }

    // display setup
    pinMode(this->_mcpResetPin, OUTPUT);
    digitalWrite(this->_mcpResetPin, LOW);
    delay(100);
    
    // start I2C com. with IO expander
    if (!this->_mcp.begin_I2C(this->_mcpAddress)) {
        Serial.println(F("Display error: No I2C device found."));
        while (1);
    }

    for(uint8_t i=0; i<16; i++){
        this->_mcp.pinMode(i, OUTPUT);
    }

    this->functionSet();
    this->cursorOrDisplayShift();
    this->displayControl();

    this->clear();
}

void LCD::write(const char * text){
    Serial.println(strlen(text));
    for(uint8_t i=0; i<strlen(text); i++){
        this->data(text[i]);
    }
}

void LCD::clear(){
    this->command(CMD_CLEAR);
}

void LCD::returnHome(){
    this->command(CMD_HOME);
}

void LCD::entryModeSet(bool cursorIncrement, bool displayShift){
    uint8_t cmd = CMD_ENTRY_MODE;
    if(cursorIncrement) cmd |= 0x02;
    if(displayShift) cmd |= 0x01;
    this->command(cmd);
}

void LCD::displayControl(bool displayOn, bool cursorOn, bool cursorBlink){
    uint8_t cmd = CMD_CONTROL;
    if(displayOn) cmd |= 0x04;
    if(cursorOn) cmd |= 0x02;
    if(cursorBlink) cmd |= 0x01;
    this->command(cmd);
}

void LCD::cursorOrDisplayShift(bool shiftDisplay, bool shiftRight){
    uint8_t cmd = CMD_SHIFT;
    if(shiftDisplay) cmd |= 0x08;
    if(shiftRight) cmd |= 0x04;
    this->command(cmd);
}


void LCD::functionSet(bool set8Bit, bool set2Line, bool set5x10Font){
    uint8_t cmd = CMD_FUNCTION;
    if(set8Bit) cmd |= 0x10;
    if(set2Line) cmd |= 0x08;
    if(set5x10Font) cmd |= 0x04;
    this->command(cmd);
}

void LCD::setPosition(uint8_t line, uint8_t index){
    uint8_t cmd = CMD_SET_DDRAM | index;
    if(line == 1) cmd |= 0x40;
    this->command(cmd);
    delayMicroseconds(100);
}

void LCD::setBacklight(bool enable){
    if(enable) this->_mcp.digitalWrite(BACKLIGHT_PIN, HIGH);
    else this->_mcp.digitalWrite(BACKLIGHT_PIN, LOW);
}

bool LCD::getBacklight(){
    return (this->_mcp.digitalRead(BACKLIGHT_PIN) == HIGH);
}

// private
void LCD::E(bool enable){
    if(enable) this->_mcp.digitalWrite(E_PIN, HIGH);
    else this->_mcp.digitalWrite(E_PIN, LOW);
}

void LCD::RnW(bool read){
    if(read) this->_mcp.digitalWrite(RnW_PIN, HIGH);
    else this->_mcp.digitalWrite(RnW_PIN, LOW);
}

void LCD::RS(bool data){
    if(data) this->_mcp.digitalWrite(RS_PIN, HIGH);
    else this->_mcp.digitalWrite(RS_PIN, LOW);
}

void LCD::command(uint8_t cmd){
    this->RnW();
    this->RS();
    this->_mcp.writeGPIOA(cmd);
    this->E();
    this->E(false);
    if((cmd | 0x01) == (CMD_HOME | 0x01)) delay(1);
}

void LCD::data(uint8_t c){
    this->RnW();
    this->RS(true);
    this->_mcp.writeGPIOA(c);
    this->E();
    this->E(false);
}