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

//global variables:
absolute_time_t intTime;

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
        pc->interruptNum = intNum;
        switch(intNum) {
            case INT0:
                pc->PIO_IRQ = pis_interrupt0;
                pc->NVIC_IRQ = PIO0_IRQ_0;
                break;
            case INT1:
                pc->PIO_IRQ = pis_interrupt1;
                pc->NVIC_IRQ = PIO0_IRQ_1;
                break;
            case INT2:
                pc->PIO_IRQ = pis_interrupt2;
                pc->NVIC_IRQ = PIO1_IRQ_0;
                break;
            case INT3:
                pc->PIO_IRQ = pis_interrupt3;
                pc->NVIC_IRQ = PIO1_IRQ_1;
                break;
            default:
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
    intTime = get_absolute_time();
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
    irq_set_exclusive_handler(pc->NVIC_IRQ, generalISRhandler);
	irq_set_enabled(pc->NVIC_IRQ, true); //enable interrupt for NVIC
    pio_set_irq0_source_enabled(PIONUM, pc->PIO_IRQ, true);
    printf("finished setting interrupt\n");
}

void startCount(PULSE_COUNTER *pc, uint numPulses) {
    //printf("sending start instruction\n");
    pc->countStartTime = get_absolute_time();
    pio_sm_put_blocking(pc->PIO_NUM, pc->SM_NUM, numPulses);//send a word to the TX FIFO, blocking if full
    pc->countStarted = true;
    pc->count = numPulses;
}

void getCount(PULSE_COUNTER *pc) {
    pc->countEndTime = intTime;
    pc->countStarted = false;
}

void calcSpeed(PULSE_COUNTER *pc) {
    int64_t timeDiff = absolute_time_diff_us(pc->countStartTime, pc->countEndTime);
    int32_t timeDiff_ms = timeDiff / 1000;
    int32_t timeDiff_s = timeDiff_ms / 1000;
    if (timeDiff) {
        printf("count: %d time diff us: %llu ms: %d secs: %d\n", pc->count, timeDiff, timeDiff_ms, timeDiff_s);
        pc->currentHertz = ((double)pc->count * 1000 * 1000) / (timeDiff);
        pc->currentRPM = pc->currentHertz * 60;
        printf("%.2f Hz, %d RPM\n", pc->currentHertz, (uint)pc->currentRPM);
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
        randPulseNum = (randPulseNum % 100) + 10;
        // startCount(&pulse_0, randPulseNum - 1);
        startCount(&pulse_0, 123);
        // test pulse of 20Hz
        while(checkFlag(&pulse_0) == false) {
            gpio_put(OUT_PIN, true);
            sleep_us(25);
            gpio_put(OUT_PIN, false);
            sleep_us(50000-25);
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