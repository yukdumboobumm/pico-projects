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

//interrupt flag
volatile bool int0Flag = false;
volatile bool int1Flag = false;
volatile bool int2Flag = false;
volatile bool int3Flag = false;

typedef struct {
    uint sensorPin;
    
    uint32_t count;
    absolute_time_t countStartTime;
    absolute_time_t countEndTime;

    bool countStarted;
    bool countReady;

    float currentHertz;
    float currentRPM;

    uint interruptNum;
    PIO PIO_NUM;
    uint SM_NUM;
    uint OFFSET;
} PULSE_COUNTER;

enum INT_NUM{
    INT0,
    INT1,
    INT2,
    INT3,
    INT_NUM_SIZE
};

PULSE_COUNTER pulse_0;
bool intnum_used[INT_NUM_SIZE] = {false};


void initPulseCounter(PULSE_COUNTER *pc, uint pin,  enum INT_NUM intNum) {
    pc->sensorPin = pin;
    pc->count = 0;
    pc->countStartTime = nil_time;
    pc->countEndTime = nil_time;
    pc->countStarted = false;
    pc->countReady = false;
    if (intnum_used[intNum]==false) {
        pc->interruptNum = intNum;
        switch(intNum) {
            case INT0:
                pc->PIO_NUM = pio0;
                pc->SM_NUM = 0;
                break;
            case INT1:
                pc->PIO_NUM = pio0;
                pc->SM_NUM = 1;
                break;
            case INT2:
                pc->PIO_NUM = pio1;
                pc->SM_NUM = 0;
                break;
            case INT3:
                pc->PIO_NUM = pio1;
                pc->SM_NUM = 1;
                break;
            default:
                printf("bad interrupt value. program terminated\n");
                while(1){};
        }
        pc->interruptNum = intNum; //how can write this so i know if a value has been used already?
    }
    else { 
        printf("interrupt alread used. program terminated\n");
        while(1){};
    }
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);  // set the pin to be an input pullup
    gpio_pull_down(pin);
}

void isr0() {
    int0Flag = true;
}
void isr1() {
    int1Flag = true;
}
void isr2() {
    int2Flag = true;
}
void isr3() {
    int3Flag = true;
}

//grab a state machine, load, and initialize it
void initPulsePIO(PULSE_COUNTER *pc) {
    // enum pio_interrupt_source intSource;
    // uint intNum;
    pc->PIO_NUM = PIONUM;
    //pc->SM_NUM = pio_claim_unused_sm(pc->PIO_NUM, true);// Get first free state machine in selected PIO
    pc->OFFSET = pio_add_program(pc->PIO_NUM, &pulse_count_program);// Add PIO program to PIO instruction memory, return memory offset
    pulse_count_program_init(pc->PIO_NUM, pc->SM_NUM, pc->OFFSET, pc->sensorPin);//initialize the SM
    pio_sm_set_enabled(pc->PIO_NUM, pc->SM_NUM, true);//start the SM
    printf("Loaded PIO program at %d and started SM: %d\n", pc->OFFSET, pc->SM_NUM);
	irq_set_exclusive_handler(PIO0_IRQ_0, isr0);
	irq_set_enabled(PIO0_IRQ_0, true);
    pio_set_irq0_source_enabled(pio0, pis_interrupt0, true);
}

void startCount(PULSE_COUNTER *pc, uint numPulses) {
    //printf("sending start instruction\n");
    pc->countStartTime = get_absolute_time();
    pio_sm_put_blocking(pc->PIO_NUM, pc->SM_NUM, numPulses);//send a word to the TX FIFO, blocking if full
    pio_interrupt_clear(PIONUM, 0);
    pc->countStarted = true;
    pc->count = numPulses;
}

void stopCount(PULSE_COUNTER *pc) {
    uint32_t randNum = get_rand_32();
    randNum = (randNum % 10) + 1;
    if (pc->countStarted) {
        printf("sending stop instruction\n");
        pio_sm_put(pc->PIO_NUM, pc->SM_NUM, randNum);//send a word to the TX FIFO, blocking if full
        pc->countEndTime= get_absolute_time();
        //get the count from RX FIFO, blocking if empty
        //printf("waiting for count\n");  
        pc->count = pio_sm_get_blocking(pc->PIO_NUM, pc->SM_NUM);
        //pc->count = pio_sm_get(pc->PIO_NUM, pc->SM_NUM)+1;
        sleep_ms(100);

        printf("**received: %u\n", pc->count);
        pc->countStarted = false;
    }
    else printf("stopCount w/o a startCount. Skipping...\n");
}

void getCount(PULSE_COUNTER *pc) {
    pc->countEndTime = get_absolute_time();
    //pc->count = pio_sm_get_blocking(pc->PIO_NUM, pc->SM_NUM);
    printf("interrupt triggered\n");
    pc->countStarted = false;
}

void calcSpeed(PULSE_COUNTER *pc) {
    int64_t timeDiff = absolute_time_diff_us(pc->countStartTime, pc->countEndTime);
    int32_t timeDiff_ms = timeDiff * 1000;
    int32_t timeDiff_s = timeDiff_ms * 1000;
    if (timeDiff) {
        printf("time diff ms: %lld, counts: %d\n", timeDiff_ms, pc->count);
        pc->currentHertz = ((float)pc->count) / (timeDiff_s);
        pc->currentRPM = pc->currentHertz * 60;
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
    initPulseCounter(&pulse_0, PULSE_PIN, INT0);
    initPulsePIO(&pulse_0);
}

//loop forever
void loop() {
    uint speed = 10;
    while(true) {
        int0Flag = false;
        uint32_t randPulseNum = get_rand_32();
        randPulseNum = (randPulseNum % 1000) + 100;
        printf("**using %d for number of pulses\n",randPulseNum);
        startCount(&pulse_0, randPulseNum);
        while(int0Flag == false) {
            gpio_put(OUT_PIN, true);
            sleep_us(25);
            gpio_put(OUT_PIN, false);
            sleep_us(25);
        }
        getCount(&pulse_0);
        calcSpeed(&pulse_0);
        sleep_ms(2000);
    }
}

int main() {
    init();
    loop();
}