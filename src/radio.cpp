#include "radio.h"

Radio* Radio::instance = nullptr;

// Constructor
// Initializes SPI pins and radio module
Radio::Radio(int csPin, int irqPin, int rstPin) {
    _cs = csPin;
    _irq = irqPin;
    _rst = rstPin;

    _radio = new SX1276(new Module(_cs, _irq, _rst));
    _receivedFlag = false;

    instance = this;
}

// setFlag
// Setting receivedFlag high to start receiving message
void Radio::setFlag() {
    if (instance != nullptr) {
        instance->_receivedFlag = 1;
    }
}

// begin
// Initialize LoRA to specified frequency and configure radio
int Radio::begin(float freq) {
    int state = _radio->begin(freq);

    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    // TODO: Configure these settings appropriately
    _radio->setSpreadingFactor(7);
    _radio->setBandwidth(125.0);
    _radio->setCodingRate(5);
    _radio->setOutputPower(17);
    _radio->setPreambleLength(16);

    // Make radio sending interrupt
    _radio->setDio0Action(setFlag, RISING);

    return RADIOLIB_ERR_NONE;
}

// send
// Sends data of length len over initialized LoRA
int Radio::send(uint8_t* data, size_t len) {
    int state = _radio->transmit(data, len);

    if (state == RADIOLIB_ERR_NONE) {
        _radio->startReceive();
    }

    return state;
}

// startListen
// Reset received flag and start receiving 
int Radio::startListen() {
    _receivedFlag = false;
    return _radio->startReceive();
}

// msgAvailable
// Check if message is available or not via received flag
int Radio::msgAvailable() {
    return _receivedFlag;
}

// recv
// Reset receivedFlag, and read in message if received flag high and no errors
String Radio::recv() {
    if (!_receivedFlag) return "";

    _receivedFlag = 0;

    String str;
    int state = _radio->readData(str);

    _radio->startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        return str;
    }

    return "";
}

// sleep
// Put LoRA module in SLEEP mode (to save power)
void Radio::sleep() {
    _radio->sleep();
}

// standby
// Put LoRA module in STANDBY mode--radio powered,
// but cannot transmit or receive. For switching from
// transmission to receiving
void Radio::standby() {
    _radio->standby();
}
