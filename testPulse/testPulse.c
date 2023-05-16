#define DEBUG
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "testPulse.pio.h"
#include "pico/rand.h"

//global constants
const uint PULSE_PIN = 18;
const uint OUT_PIN = 19;
const PIO PIONUM = pio0;

typedef struct {
    uint sensorPin;
    volatile uint32_t countsArray;
    absolute_time_t startTimeArray;
    absolute_time_t endTimeArray;

    bool countStarted;

    float currentHertz;
    float currentRPM;

    PIO PIO_NUM;
    uint SM_NUM;
    uint OFFSET;
} PULSE_COUNTER;

PULSE_COUNTER pulse_1;


void initPulseCounter(PULSE_COUNTER *pc, uint pin) {
    pc->sensorPin = pin;
    pc->countsArray = 0;
    pc->startTimeArray = nil_time;
    pc->endTimeArray = nil_time;
    pc->countStarted = false;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);  // set the pin to be an input pullup
    gpio_pull_down(pin);
}

//grab a state machine, load, and initialize it
void initPulsePIO(PULSE_COUNTER *pc) {
    // enum pio_interrupt_source intSource;
    // uint intNum;
    pc->PIO_NUM = PIONUM;
    pc->SM_NUM = pio_claim_unused_sm(pc->PIO_NUM, true);// Get first free state machine in selected PIO
    pc->OFFSET = pio_add_program(pc->PIO_NUM, &pulse_count_program);// Add PIO program to PIO instruction memory, return memory offset
    pulse_count_program_init(pc->PIO_NUM, pc->SM_NUM, pc->OFFSET, pc->sensorPin);//initialize the SM
    pio_sm_set_enabled(pc->PIO_NUM, pc->SM_NUM, true);//start the SM
    printf("Loaded PIO program at %d and started SM: %d\n", pc->OFFSET, pc->SM_NUM);
}

void startCount(PULSE_COUNTER *pc) {
    printf("sending start instruction\n");
    pio_sm_put_blocking(pc->PIO_NUM, pc->SM_NUM, true);//send a word to the TX FIFO, blocking if full
    pc->startTimeArray = get_absolute_time();
    pio_interrupt_clear(PIONUM, 0);
    pc->countStarted = true;
}

void stopCount(PULSE_COUNTER *pc) {
    if (pc->countStarted) {
        printf("sending stop instruction\n");
        pio_sm_put_blocking(pc->PIO_NUM, pc->SM_NUM, (uint32_t)1);//send a word to the TX FIFO, blocking if full
        pc->endTimeArray= get_absolute_time();
        //get the count from RX FIFO, blocking if empty
        printf("waiting for count\n");  
        // pc->countsArray = pio_sm_get_blocking(pc->PIO_NUM, pc->SM_NUM)+1;
        pc->countsArray = pio_sm_get(pc->PIO_NUM, pc->SM_NUM)+1;
        sleep_ms(100);

        printf("received: %d\n", pc->countsArray);
        pc->countStarted = false;
    }
    else printf("stopCount w/o a startCount. Skipping...\n");
}

void calcSpeed(PULSE_COUNTER *pc) {
    int64_t timeDiff_ms = absolute_time_diff_us(pc->startTimeArray, pc->endTimeArray);
    if (timeDiff_ms) {
        pc->currentHertz = ((float)pc->countsArray) / (timeDiff_ms);
        pc->currentRPM = pc->currentHertz * 60;
        printf("time diff ms: %lld, counts: %d\n", timeDiff_ms, pc->countsArray);
    }
    else printf("no time difference. Skipping...\n");
}

//run our initializations
void init() {
    stdio_init_all();
    gpio_init(OUT_PIN);
    gpio_set_dir(OUT_PIN, GPIO_OUT);
    gpio_put(OUT_PIN, false);
    sleep_ms(2000);//give us some time to breathe
    initPulseCounter(&pulse_1, PULSE_PIN);
    initPulsePIO(&pulse_1);
}



//loop forever
void loop() {
    uint speed = 10;
    while(true) {
        uint32_t randPulseNum = get_rand_32();
        randPulseNum = (randPulseNum % 10) + 1;
        printf("using %d for number of pulses\n",randPulseNum);
        printf("starting counts\n");
        startCount(&pulse_1);
        for (int i =0; i<randPulseNum; i++) {
            gpio_put(OUT_PIN, true);
            sleep_ms(100);
            gpio_put(OUT_PIN, false);
            sleep_ms(100);
        }
        sleep_ms(5000);
        stopCount(&pulse_1);
        calcSpeed(&pulse_1);
        printf("Speed: %d, HZ: %f, RPM: %f\n", speed, pulse_1.currentHertz, pulse_1.currentRPM);
        sleep_ms(2000);
    }
}

int main() {
    init();
    loop();
}