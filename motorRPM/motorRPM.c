#define DEBUG
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "dc_motor.h"
#include "motorRPM.pio.h"

//global constants
const bool CW = true; 
const bool CCW = false;
const bool HIGH = true;
const bool LOW = false;
const uint FREQ_HZ = 20000;
const uint DUTY_CYCLE = 30;

const uint PULSE_PIN = 22;
const uint R_ENABLE_PIN = 21;
const uint L_ENABLE_PIN = 20;
const uint R_PWM_PIN = 19;
const uint L_PWM_PIN = 18;

const PIO PIONUM = pio0;

typedef struct {
    uint sensorPin;
    uint32_t countsArray[10];
    absolute_time_t startTimeArray[10];
    absolute_time_t endTimeArray[10];
    uint arraySize;
    uint arrayIndex;

    bool countStarted;

    float currentHertz;
    float currentRPM;

    PIO PIO_NUM;
    uint SM_NUM;
    uint OFFSET;
} PULSE_COUNTER;

//global structs, we'll pass these around with pointers
DC_MOTOR motor_1;
PULSE_COUNTER pulse_1;


void initPulseCounter(PULSE_COUNTER *pc, uint pin) {
    pc->sensorPin = pin;
    pc->arraySize = sizeof(pc->countsArray) / sizeof(pc->countsArray[0]);
    for (int i = 0; i < pc->arraySize; i++) {
        pc->countsArray[i] = 0;
        pc->startTimeArray[i]= nil_time;
        pc->endTimeArray[i] = nil_time;
    }
    pc->arrayIndex = 0;
    pc->countStarted = false;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);  // set the pin to be an input
    gpio_pull_up(pin);
}

//grab a state machine, load, and initialize it
void setupPulsePIO(PULSE_COUNTER *pc) {
    enum pio_interrupt_source intSource;
    uint intNum;
    pc->PIO_NUM = PIONUM;
    pc->SM_NUM = pio_claim_unused_sm(pc->PIO_NUM, true);// Get first free state machine in selected PIO
    pc->OFFSET = pio_add_program(pc->PIO_NUM, &pulse_count_program);// Add PIO program to PIO instruction memory, return memory offset
    pulse_count_program_init(pc->PIO_NUM, pc->SM_NUM, pc->OFFSET, pc->sensorPin);//initialize the SM
    pio_sm_set_enabled(pc->PIO_NUM, pc->SM_NUM, true);//start the SM
    DEBUG_PRINT("Loaded PIO program at %d and started SM: %d\n", pc->OFFSET, pc->SM_NUM);
    // Make SM 0 exclusive source for PIO0_IRQ_0
    switch (pc->SM_NUM) {
    case 0:
        intSource = pis_interrupt0;
        intNum = PIO0_IRQ_0;
        break;
    case 1:
        intSource = pis_interrupt1;
        break;
    case 2:
        intSource = pis_interrupt2;
        break;
    case 3:
        intSource = pis_interrupt0;
        break;
}
	// pio_set_irq0_source_enabled(pio0, intSource, true);
	// irq_set_exclusive_handler(PIO0_IRQ_0, isr0);
	// irq_set_enabled(PIO0_IRQ_0, true);
    // pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS;
}

//todo start a count by getting a timestamp and sending some fluff to the TX FIFO
//the SM will see that (with pull noblock) and will hmmm 
//it'll already be running. i need to start it don't i?
void startCount(PULSE_COUNTER *pc) {
    // if (pio_interrupt_get(PIONUM, 0)) {//} && pc->countStarted==false) {
    // if (pio0_hw->irq & 1) {
    if (true) {
        pio_sm_put_blocking(pc->PIO_NUM, pc->SM_NUM, true);//send a word to the TX FIFO, blocking if full
        pc->startTimeArray[pc->arrayIndex] = get_absolute_time();
        pio_interrupt_clear(PIONUM, 0);
        pc->countStarted = true;
    }
    else DEBUG_PRINT("PIO interrupt not set. Skipping...\n");
}

void stopCount(PULSE_COUNTER *pc) {
    if (pc->countStarted) {
        DEBUG_PRINT("sending stop instruction\n");
        pio_sm_put_blocking(pc->PIO_NUM, pc->SM_NUM, true);//send a word to the TX FIFO, blocking if full
        pc->endTimeArray[pc->arrayIndex] = get_absolute_time();
        //get the count from RX FIFO, blocking if empty
        DEBUG_PRINT("waiting for count\n");  
        pc->countsArray[pc->arrayIndex] = pio_sm_get_blocking(pc->PIO_NUM, pc->SM_NUM);
        DEBUG_PRINT("received: %d\n", pc->countsArray[pc->arrayIndex]);
        pc->countStarted = false;
        pc->arrayIndex = (pc->arrayIndex + 1) % pc->arraySize;
    }
    else DEBUG_PRINT("stopCount w/o a startCount. Skipping...\n");
}

void calcSpeed(PULSE_COUNTER *pc) {
    uint latestIndex = (pc->arrayIndex-1) % pc->arraySize;
    DEBUG_PRINT("lastest index: %d\n", latestIndex);
    uint64_t timeDiff = absolute_time_diff_us(pc->startTimeArray[latestIndex], pc->endTimeArray[latestIndex]);
    if (timeDiff) {
        pc->currentHertz = ((double)pc->countsArray[latestIndex] * 1000 * 1000) / (timeDiff);
        pc->currentRPM = pc->currentHertz * 60;
        DEBUG_PRINT("time diff: %d, counts: %d\n", timeDiff, pc->countsArray[0]);
    }
    else DEBUG_PRINT("no time difference. Skipping...\n");
}

//run our initializations
void init() {
    stdio_init_all();
    sleep_ms(2000);//give us some time to breathe
    initPulseCounter(&pulse_1, PULSE_PIN);
    setupPulsePIO(&pulse_1);
    initMotor(&motor_1, R_ENABLE_PIN, L_ENABLE_PIN, R_PWM_PIN, L_PWM_PIN);//DCMOTOR, R-Enable, L-Enable, R-PWM, L-PWM
    initMotorPWM(&motor_1, FREQ_HZ);
    initMotorPIO(&motor_1, pio0);//would be nice to get first available PIO
    DEBUG_PRINT("DC MOTOR 1 PINS \n RE: %d, LE: %d, RP: %d, LP: %d\n", motor_1.R_ENABLE_PIN, motor_1.L_ENABLE_PIN, motor_1.R_PWM_PIN, motor_1.L_PWM_PIN);
    DEBUG_PRINT("motor 1 PWM set with freq: %d\n", motor_1.freq);
    setMotorSpeed(&motor_1, 10);
    //runMotor(&motor_1, CW);
    //sleep_ms(2000); //allow the motor to reach speed
    // while(true) {
    //     int pin_val = gpio_get(PULSE_PIN);
    //     printf("pin val: %d\n", pin_val);
    //     sleep_ms(5000);
    // }

}

//loop forever
void loop() {
    uint speed = 10;
    bool dir = CW;
    while(true) {
        runMotor(&motor_1, dir);
        sleep_ms(1000);
        startCount(&pulse_1);
        sleep_ms(5000);
        stopCount(&pulse_1);
        stopMotor(&motor_1);
        calcSpeed(&pulse_1);
        printf("Speed: %d, HZ: %f, RPM: %f\n", speed, pulse_1.currentHertz, pulse_1.currentRPM);
        speed = ((speed + 5) % 50); //keep us in the range of 10-45
        speed = !speed ? 10 : speed;
        setMotorSpeed(&motor_1, speed);
    }
}

int main() {
    // sleep_ms(2000);//give us some time to breathe
    // stdio_init_all();
    // printf("hello world\n");
    init();
    loop();
}