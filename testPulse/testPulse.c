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
    
    uint32_t count;
    absolute_time_t countStartTime;
    absolute_time_t countEndTime;

    bool countStarted;
    bool countReady;

    float currentHertz;
    float currentRPM;

    uint interruptNum;
    uint PIO_IRQ;
    uint NVIC_IRQ;
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
//interrupt flags
volatile bool intFlags[INT_NUM_SIZE] = {false};


void initPulseCounter(PULSE_COUNTER *pc, uint pin,  enum INT_NUM intNum) {
    pc->sensorPin = pin;
    pc->count = 0;
    pc->countStartTime = nil_time;
    pc->countEndTime = nil_time;
    pc->countStarted = false;
    pc->countReady = false;
    if (intnum_used[intNum]==false) {
        if (intNum < INT3) {
            pc->PIO_IRQ = (uint)(intNum + 8);
            pc->interruptNum = intNum;
        }
        else {
            printf("bad interrupt value. program terminated\n");
            while(1){};
        }
    }
    else { 
        printf("interrupt alread used. program terminated\n");
        while(1){};
    }
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);  // set the pin to be an input pullup
    gpio_pull_down(pin);
}

void generalISRhandler() {
    printf("!!in handler!!\n");
    if(pio_interrupt_get(PIONUM, INT0))  {
        intFlags[INT0] = true;
        pio_interrupt_clear(PIONUM, INT0);
    }
    else if(pio_interrupt_get(PIONUM, INT1)) {
        intFlags[INT1] = true;
        pio_interrupt_clear(PIONUM, INT1);
    }
    else if(pio_interrupt_get(PIONUM, INT2)) {
        intFlags[INT2] = true;
        pio_interrupt_clear(PIONUM, INT2);
    }
    else if(pio_interrupt_get(PIONUM, INT3)) {
        intFlags[INT3] = true;
        pio_interrupt_clear(PIONUM, INT3);
    }
    else printf("no interrupt found\n");
}

//grab a state machine, load, and initialize it
//need a better mechanism (and understanding) for how to grab an interrupt number
//like wtf is irq0 vs irq1. yeesh.
void initPulsePIO(PULSE_COUNTER *pc) {
    pc->PIO_NUM = PIONUM;
    pc->SM_NUM = pio_claim_unused_sm(pc->PIO_NUM, true);// Get first free state machine in selected PIO
    pc->OFFSET = pio_add_program(pc->PIO_NUM, &pulse_count_program);// Add PIO program to PIO instruction memory, return memory offset
    pulse_count_program_init(pc->PIO_NUM, pc->SM_NUM, pc->OFFSET, pc->sensorPin);//initialize the SM
    pio_sm_set_enabled(pc->PIO_NUM, pc->SM_NUM, true);//start the SM
    printf("Loaded PIO program at %d and started SM: %d\n", pc->OFFSET, pc->SM_NUM);
	irq_set_exclusive_handler(PIO0_IRQ_0, generalISRhandler);
	irq_set_enabled(PIO0_IRQ_0, true); //enable interrupt for NVIC
    // pio_set_irq0_source_enabled(pc->PIO_NUM, pc->IRQ, true); //enable interrupt in PIO
    pio_set_irq0_source_enabled(PIONUM, pis_interrupt0, true);
    printf("finished setting interrupt\n");
    // printf("PIS number: %d, IRQ number: %d\n", pis_interrupt0, pc->IRQ);
}

void startCount(PULSE_COUNTER *pc, uint numPulses) {
    //printf("sending start instruction\n");
    pc->countStartTime = get_absolute_time();
    pio_sm_put_blocking(pc->PIO_NUM, pc->SM_NUM, numPulses);//send a word to the TX FIFO, blocking if full
    pc->countStarted = true;
    pc->count = numPulses;
}

void getCount(PULSE_COUNTER *pc) {
    pc->countEndTime = get_absolute_time();
    pc->countStarted = false;
}

void calcSpeed(PULSE_COUNTER *pc) {
    int64_t timeDiff = absolute_time_diff_us(pc->countStartTime, pc->countEndTime);
    int32_t timeDiff_ms = timeDiff / 1000;
    int32_t timeDiff_s = timeDiff_ms / 1000;
    if (timeDiff) {
        printf("time diff ms: %d\n", timeDiff_ms);
        pc->currentHertz = ((float)pc->count) / (timeDiff_s);
        pc->currentRPM = pc->currentHertz * 60;
    }
    else printf("no time difference. Skipping...\n");
}

inline void clearFlag(PULSE_COUNTER *pc) {
    intFlags[pc->interruptNum] = false;
}

inline bool checkFlag(PULSE_COUNTER *pc) {
    return intFlags[pc->interruptNum];
}

//run our initializations
void init() {
    stdio_init_all();
    gpio_init(OUT_PIN);
    gpio_set_dir(OUT_PIN, GPIO_OUT);
    gpio_put(OUT_PIN, false);
    sleep_ms(2000);//give us some time to breathe
    initPulseCounter(&pulse_0, PULSE_PIN, INT0);//PulseCounter instance, input pin, pio interrupt num (0-3)
    initPulsePIO(&pulse_0); //pionum and sm_num are set by initpulsecounter, determined by the interrupt num used
}

//loop forever
void loop() {
    while(true) {
        uint counter = 0;
        //uint32_t message;
        clearFlag(&pulse_0);
        uint32_t randPulseNum = get_rand_32();
        randPulseNum = (randPulseNum % 1000) + 100;
        startCount(&pulse_0, randPulseNum);
        while(checkFlag(&pulse_0) == false) {
            gpio_put(OUT_PIN, true);
            sleep_us(25);
            gpio_put(OUT_PIN, false);
            sleep_us(500);
            counter++;
        }
        printf("asked for %d, and sent: %d\n",randPulseNum, counter);
        getCount(&pulse_0);
        calcSpeed(&pulse_0);
        sleep_ms(1000);
    }
}

int main() {
    init();
    loop();
}