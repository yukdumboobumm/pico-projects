//includes
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "motorPWM.pio.h"

//DEFINES
#define DEBUG
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...) (void)0
#endif

//global constants
const bool CW = false; 
const bool CCW = true;
const bool HIGH = true;
const bool LOW = false;
const uint FREQ_HZ = 20000;
const uint DUTY_CYCLE = 30;

//forward declaration of struct
typedef struct DC_MOTOR DC_MOTOR;

//global struct, we'll pass this around with a pointer
DC_MOTOR motor_1;

//Function Declarations
void setupGPIO(DC_MOTOR *, uint, uint, uint, uint);//&MOTOR, R-ENABLE, L-ENABLE, R-PWM, L-PWM
void initMotor(DC_MOTOR *, uint, uint, PIO);//&MOTOR, FREQUENCY, DUTY CYCLE, PIO NUM
void setupPIO(DC_MOTOR *);//&MOTOR
void stopMotor(DC_MOTOR *);//MOTOR
void runMotor(DC_MOTOR *, bool);//&MOTOR, DIRECTION


//container for our motor(s)
struct DC_MOTOR
{
    //motor variables
    uint16_t freq; //in hz
    uint16_t dutyCycle; //percentage 0-100
    bool dir; //cirrent rotation direction
    bool runFlag; //are we running
    
    //pins
    uint R_ENABLE_PIN;
    uint L_ENABLE_PIN;
    uint R_PWM_PIN;
    uint L_PWM_PIN;
    uint LOWEST_PWM; //PIO state machine will take the lowest # pin and initialize up.
    
    //PIO number and state machine 
    PIO PIO_NUM;
    uint SM_NUM;

    //SM values 
    float clockDiv;
    uint32_t period;
    uint32_t pwmOnCycles;
    uint32_t pwmOffCycles;
};

//initialize our pins
//&MOTOR, R-ENABLE, L-ENABLE, R-PWM, L-PWM
void setupGPIO(DC_MOTOR *motor, uint p1, uint p2, uint p3, uint p4)
{
    motor->R_ENABLE_PIN = p1;
    motor->L_ENABLE_PIN = p2;
    motor->R_PWM_PIN = p3;
    motor->L_PWM_PIN = p4;

    motor->LOWEST_PWM = motor->R_PWM_PIN < motor->L_PWM_PIN ? motor->R_PWM_PIN:motor->L_PWM_PIN;

    //initialize gpio for read/write
    gpio_init(motor->R_ENABLE_PIN);
    gpio_init(motor->L_ENABLE_PIN);
    gpio_init(motor->R_PWM_PIN);
    gpio_init(motor->L_PWM_PIN);
    //set gpios to outputs
    gpio_set_dir(motor->R_ENABLE_PIN, GPIO_OUT);
    gpio_set_dir(motor->L_ENABLE_PIN, GPIO_OUT);
    gpio_set_dir(motor->R_PWM_PIN, GPIO_OUT);
    gpio_set_dir(motor->L_PWM_PIN, GPIO_OUT);
    //write initial values to low
    gpio_put(motor->R_ENABLE_PIN, LOW);
    gpio_put(motor->L_ENABLE_PIN, LOW);
    gpio_put(motor->R_PWM_PIN, LOW);
    gpio_put(motor->L_PWM_PIN, LOW);
}

//initialize the motor with PWM frequency and duty cycle
//&MOTOR, FREQUENCY, DUTY CYCLE, PIO NUM
void initMotor(DC_MOTOR *motor, uint freq, uint duty, PIO pio) {
    motor->freq = freq;
    motor->dutyCycle = duty;
    motor->PIO_NUM = pio;
    //determine pwm period and on/off cycles
    motor->period = (clock_get_hz(clk_sys) / motor->freq) - 11;//PIO program has 11 instructions
    motor->pwmOnCycles = motor->period * motor->dutyCycle / 100U;
    motor->pwmOffCycles = motor->period - motor->pwmOnCycles;

    DEBUG_PRINT("sys clock: %d\n", clock_get_hz(clk_sys));
    DEBUG_PRINT("requested frequency: %d\n", motor->freq);
    DEBUG_PRINT("clock div: %f\n", motor->clockDiv);
    DEBUG_PRINT("PWM period: %d\n", motor->period);
    DEBUG_PRINT("on cycles: %d, off cycles: %d\n", motor->pwmOnCycles, motor->pwmOffCycles);
}

//grab a state machine, load, and initialize it
void setupPIO(DC_MOTOR *motor) {
    motor->SM_NUM = pio_claim_unused_sm(motor->PIO_NUM, true);// Get first free state machine in selected PIO
    uint offset = pio_add_program(motor->PIO_NUM, &motorPWM_program);// Add PIO program to PIO instruction memory, return memory offset
    motorPWM_program_init(motor->PIO_NUM, motor->SM_NUM, offset, motor->LOWEST_PWM);//initialize the SM
    DEBUG_PRINT("Loaded PIO program at %d\n", offset);
}

//stop the motor with physical pins and disables the motor's state machine
void stopMotor(DC_MOTOR *motor) {
    DEBUG_PRINT("STOPPING MOTOR\n");
    gpio_put(motor->R_ENABLE_PIN, LOW);
    gpio_put(motor->L_ENABLE_PIN, LOW);
    pio_sm_drain_tx_fifo(motor->PIO_NUM, motor->SM_NUM); //safe
    pio_sm_put_blocking(motor->PIO_NUM, motor->SM_NUM, 0);//send our word to the TX FIFO
    motor->runFlag = false;
    //pio_sm_set_enabled(motor.PIO_NUM, motor.SM_NUM, false);//disable the state machine, not actually necessary
    sleep_ms(2000);
}

//run a motor in the given direction by setting enable pins and starting the staate machine
//&MOTOR, DIRECTION
void runMotor(DC_MOTOR *motor, bool dir) {
    //check if we actually need to do anything
    if (dir != motor->dir || !motor->runFlag) {
        stopMotor(motor);
        uint32_t SM_word = (motor->pwmOffCycles & 0x7fff) << 17; //shift the 15 bit off-cycles to the MSBs of 32 bit word
        SM_word |= (motor->pwmOnCycles & 0x7fff) << 2; //shift 15 bits of on-cycles next, leaving two bits for dir and run
        SM_word |= (dir + 1); //dir is either 0b10 or 0b01 
        motor->runFlag = true;
        motor->dir = dir;
        DEBUG_PRINT("RUNNING MOTOR\n");
        gpio_put(motor->R_ENABLE_PIN, HIGH);
        gpio_put(motor->L_ENABLE_PIN, HIGH);
        pio_sm_drain_tx_fifo(motor->PIO_NUM, motor->SM_NUM); //safe
        pio_sm_put_blocking(motor->PIO_NUM, motor->SM_NUM, SM_word);//send our word to the TX FIFO
        pio_sm_set_enabled(motor->PIO_NUM, motor->SM_NUM, true);//start the SM
    }
}

//run our initializations
void init() {
    sleep_ms(2000);//give us some time to breathe
    stdio_init_all();
    setupGPIO(&motor_1, 21, 20, 19, 18);//DCMOTOR, R-Enable, L-Enable, R-PWM, L-PWM
    initMotor(&motor_1, FREQ_HZ, DUTY_CYCLE, pio0);//would be nice to get first available PIO
    setupPIO(&motor_1);
    DEBUG_PRINT("DC MOTOR 1 PINS \n RE: %d, LE: %d, RP: %d, LP: %d\n", motor_1.R_ENABLE_PIN, motor_1.L_ENABLE_PIN, motor_1.R_PWM_PIN, motor_1.L_PWM_PIN);
    DEBUG_PRINT("motor 1 set with freq: %d, duty: %d, on pin: %d\n", motor_1.freq, motor_1.dutyCycle, motor_1.LOWEST_PWM);
    // runMotor(&motor_1, CW);
}

//loop forever
void loop() {
    while(true) {
        runMotor(&motor_1, CW);
        sleep_ms(10000);
        runMotor(&motor_1, CCW);
        sleep_ms(10000);
    }
}

int main()
{
    init();
    loop();
}
