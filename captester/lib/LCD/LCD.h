#ifndef __LCD_H__
#define __LCD_H__

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

class LCD{
    public:
        LCD(uint8_t mcpResetPin=-1, uint8_t mcpAddress=0x20);
        ~LCD();

        void begin();
        void clear();
        void returnHome();
        void entryModeSet(bool cursorIncrement=true, bool displayShift=false);
        void displayControl(bool displayOn=true, bool cursorOn=false, bool cursorBlink=false);
        void cursorOrDisplayShift(bool shiftDisplay=false, bool shiftRight=false);
        void functionSet(bool set8Bit=true, bool set2Line=true, bool set5x10Font=false);
        void setBacklight(bool enable=true);
        bool getBacklight();

        // line = 0|1, index = 0...16
        void setPosition(uint8_t line, uint8_t index);
        void write(const char * text);
        void write(const char * text, uint8_t line, uint8_t index){
            this->setPosition(line, index);
            this->write(text);
        }

    private:
        uint8_t _mcpResetPin;
        uint8_t _mcpAddress;

        void E(bool enable=true);
        void RnW(bool read=false);
        void RS(bool data=false);
        void command(uint8_t cmd);
        void data(uint8_t c);

        Adafruit_MCP23X17 _mcp;

};


#endif /*__LCD_H__*/
