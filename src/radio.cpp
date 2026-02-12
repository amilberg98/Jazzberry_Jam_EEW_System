#include "radio.h"

Radio::Radio(int csPin, int irqPin, int rstPin) {
    _cs = csPin;
    _irq = irqPin;
    _rst = rstPin;

    _radio = new SX1276(new Module(_cs, _irq, _rst));
    _receivedFlag = false;
}

void Radio::setFlag(void* radioPtr) {
    Radio* r = (Radio*)radioPtr;
    r->_receivedFlag = true;
}

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
    _radio->setDio0Action(setFlag, this);

    return RADIOLIB_ERR_NONE;
}

int Radio::send(uint8_t* data, size_t len) {
    int state = _radio->transmit(data, len);

    if (state == RADIOLIB_ERR_NONE) {
        _radio->startReceive();
    }

    return state;
}

int Radio::startListen() {
    _receivedFlag = false;
    return _radio->startReceive();
}

int Radio::msgAvailable() {
    return _receivedFlag;
}

String Radio::recv() {
    if (!_receivedFlag) return "";

    _receivedFlag = false;

    String str;
    int state = _radio->readData(str);

    _radio->startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        return str;
    }

    return "";
}

void Radio::sleep() {
    _radio->sleep();
}

void Radio::standby() {
    _radio->standby();
}
