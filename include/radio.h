#ifndef RADIO_H
#define RADIO_H

#include <Arduino.h>
#include <RadioLib.h>

class Radio {
        public:
        Radio(int cs, int irq, int rst);

        int begin(float freq);

        // Sending
        int send(uint8_t* data, size_t len);


        // Receiving
        int startListen();
        int msgAvailable();
        String recv();

        void sleep();
        void standby();

        bool hasError();


        private:
        SX1276* _radio;
        int _cs;
        int _irq;
        int _rst;
        volatile int _receivedFlag;
        bool errorFlag;


        static Radio* instance;
        static void setFlag();
};

#endif