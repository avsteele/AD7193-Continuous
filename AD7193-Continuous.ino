#include <SPI.h>
#include "Streaming.h"

#define AD7193_CS_PIN 10

const uint8_t AD7193_REG_COMM = 0x00;
const uint8_t AD7193_REG_STAT = 0x00;
const uint8_t AD7193_REG_MODE = 0x01;
const uint8_t AD7193_REG_CONF = 0x02;
const uint8_t AD7193_REG_DATA = 0x03;

SPISettings settings(F_CPU/2, MSBFIRST, SPI_MODE3);

void setup()
{
    Serial.begin(9600);

    // SPI setup
    SPI.begin();
    pinMode(AD7193_CS_PIN, OUTPUT);
    
    // this does not appear to matter. the device properly controls the line up/down when CS is low
    //pinMode(MISO, INPUT_PULLUP);

    delay(100);    
    resetDevice();
    delay(100);

    uint32_t modeIni = 0x080060;
    uint32_t mode = getRegister(AD7193_REG_MODE, 3);
    // expect 0x080060
    Serial << (mode == modeIni ? "   OK" : "ERROR") << ": Mode1 = 0x" << _WIDTHZ(_HEX(mode),6) << endl;

    // WORKS
    setupContinuousConversion();
}

uint32_t tlast = 0;
uint16_t samples = 0;
uint32_t tick = 5000;
void loop()
{
    //delay(1000);
    uint32_t t = millis();
    waitForReady2();
    uint32_t data = readMISO4bytes();
    samples++;

    if(millis()-tlast > tick)
    {
        // print sample rate (Hz)
        Serial << "Sample rate: " << (float)samples / (float)(millis()-tlast) * 1000 << " Hz" << endl;
        samples = 0;
        tlast = millis();
    }
}

void resetDevice()
{
    digitalWrite(AD7193_CS_PIN, LOW);
    delay(10);

    char i = 0;
    for(i = 0; i < 6; i++)
    {
        SPI.transfer(0xFF);
    }
    delay(10);
    digitalWrite(AD7193_CS_PIN, HIGH);
}

float toVolts(uint32_t rawData)
{
    // right for unipolar mode, 2.5V reference, in mode where status bits appended

    uint32_t data = rawData >>8; // discard status bits
    float voltage = ((double)data / 0xFFFFFF ) * 2.50f; 
    return voltage;
}

bool waitForReady2()
{
    uint32_t t0 = millis();
    uint32_t loops = 0;
    while(1){
        if (digitalRead(MISO) == 0) 
        {
            // ouput wait time
            //Serial << (millis()-t0) << " ms" << endl;
            return true;
        }
        loops++;
        if (loops > 1000000) {
            Serial << "Timeout waiting for ready" << endl;
            return false;
        }
    }
    return false;
}

uint32_t readMISO4bytes()
{
    // CS-is not altered in this transaction

    SPI.beginTransaction(settings);
    //digitalWrite(AD7193_CS_PIN, LOW);
    //delay(1);

    // 32b buffer to hold the read data.(No register is longer than 32b)
    uint32_t buffer = 0;
    // holds the most recently received byte
    uint8_t receiveBuffer = 0;
    uint8_t byteIndex = 0;
    while(byteIndex < 4)
    {
        // we are getting data here, we send 0's
        receiveBuffer = SPI.transfer(0);
        buffer = (buffer << 8) + receiveBuffer;
        byteIndex++;
    }

    //digitalWrite(AD7193_CS_PIN, HIGH);
    SPI.endTransaction();
    return buffer;
}

void setupContinuousConversion()
{
    // nominal mode value: 0x180060
    // 0001 1000 0000 0000 0110 0000
    // 0001 => continuous convert (000), append status (1)
    // 1000 => int. clock (01), no averaging/fast-settling (00)
    // 0000 => sinc4 (0), NOP (0),no parity check (0), clock nonsense (0)
    // 00__ => NOT single conv cycle (0), no 60hz notch (0)
    // __00 0110 0000 => filter /data rate (default = 96 == 50hz output rate)
    //uint32_t modeSet = 0x180060;
    // 005 => 960 Hz
    // 060 => 50 Hz
    // 1E0 => 10 Hz
    // CFF => 4.7 Hz
    uint32_t modeSet = 0x180060; // same but w/ diff filter word
    setRegister(AD7193_REG_MODE, modeSet, 3);

    uint32_t configSet = 0x040118;
    // 0 => 0000 => chop disable (0), NOP (00), Ext ref 1 (0)
    // 4 => 0100 => NOP (0), Pseudo-diff (1), no short (0), no temp monitor (0)
    // 01 => 0000 0001  => channel 0 enabled
    // 1 => 0001 => burn? (0), refdet? (0), NOP (0), buffer input (1)
    // 8 => 1000 => unipolar (1), gain =0 (000)
    setRegister(AD7193_REG_CONF, configSet, 3);

    // see: https://ez.analog.com/data_converters/precision_adcs/f/q-a/24479/ad7193-continuous-read
    uint8_t commSet = 0b01011100; // also tried: 0b11011100
    // 0 => write enable bit
    // 1 => read operation follows
    // 011 => the data register
    // 1 => continuous read
    // 00 => NOP
    setRegister(AD7193_REG_COMM, commSet, 1);

    // continuous low CS means this device has the line
    digitalWrite(AD7193_CS_PIN, LOW);
}

uint32_t getRegister(uint8_t which, uint8_t bytesNumber)
{
    SPI.beginTransaction(settings);
    digitalWrite(AD7193_CS_PIN, LOW);

    // 1<<6 is for 'read' operation
    uint8_t request = (1<<6) | (which<<3);
    // send request, discard any data received
    uint8_t _ = SPI.transfer(request);
    // 32b buffer to hold the read data.(No register is longer than 32b)
    uint32_t buffer = 0;
    // holds the most recently received byte
    uint8_t receiveBuffer = 0;
    uint8_t byteIndex = 0;
    while(byteIndex < bytesNumber)
    {
        // we are geting data here, we send 0's
        receiveBuffer = SPI.transfer(0);
        buffer = (buffer << 8) + receiveBuffer;
        byteIndex++;
    }

    digitalWrite(AD7193_CS_PIN, HIGH);
    SPI.endTransaction();
    return buffer;
}

void setRegister(uint8_t which, uint32_t value, uint8_t bytesNumber)
{
    SPI.beginTransaction(settings);
    digitalWrite(AD7193_CS_PIN, LOW);
    //delay(1);

    // 0<<6 is for 'write' operation
    uint8_t request = (0<<6) | (which<<3);
    // send request, discard any data received
    uint8_t _ = SPI.transfer(request);
    uint8_t byteIndex = 0;
    for(byteIndex = 0; byteIndex < bytesNumber; byteIndex++)
    {
        uint8_t nextByte = (value >> (8*(bytesNumber - byteIndex - 1))) & 0xFF;
        //Serial << "Sending byte: 0x" << _HEX(nextByte) << endl;
        SPI.transfer(nextByte);
    }    

    //delay(1);
    digitalWrite(AD7193_CS_PIN, HIGH);
    SPI.endTransaction();
}