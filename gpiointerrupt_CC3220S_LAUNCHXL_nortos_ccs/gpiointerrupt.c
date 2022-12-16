/*
 * Copyright (c) 2015-2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== gpiointerrupt.c ========
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/Timer.h>
#include <ti/drivers/I2C.h>

/* Driver configuration */
#include "ti_drivers_config.h"


#include <ti/drivers/Timer.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

// Global shared variables
int16_t setTempCelsius = 22;
int16_t currentTempCelsius;
unsigned long timerPeriod = 100;
unsigned long totalTimeElapsed = 0;
char heaterOn = 0;          // bit
char upBtnPressed = 0;      // bit
char downBtnPressed = 0;    // bit
char timerFlag = 0;         // bit

/*
 *  ======== UART Driver Stuff ========
 */
#define DISPLAY(x) UART_write(uart, &output, x);

// UART Global Variables
char output[64];
int bytesToSend;

// Driver Handles - Global variables
UART_Handle uart;

void initUART(void)
{
    UART_Params uartParams;
    // Init the driver
    UART_init();
    // Configure the driver
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.baudRate = 115200;

    // Open the driver
    uart = UART_open(CONFIG_UART_0, &uartParams);

    if (uart == NULL) {
        /* UART_open() failed */
        while (1);
    }
}
// ------------------------------- UART END -----------------------------


// Set up a task type to track each tasks's relevant information
// Code adapted from Emerging Systems Architectures and Technologies, ZyBooks ISBN: 979-8-203-05560-6
const unsigned char numTasks = 5;

typedef struct task {
    int state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);        // Pointer to the tasks processing function
} task;

task tasks[5];

enum BTN_States { BTN_Off, BTN_On };
enum HTR_States { HTR_Off, HTR_On };

// State machine tick function declarations
int TickFct_CheckUpBtn(int state);
int TickFct_CheckDownBtn(int state);
int TickFct_CheckTemp(int state);
int TickFct_Output(int state);

/*
 *  ======== I2C Driver Stuff ========
 */
// I2C Global Variables
static const struct {
    uint8_t address;
    uint8_t resultReg;
    char *id;
}

sensors[3] = {
    { 0x48, 0x0000, "11X" },
    { 0x49, 0x0000, "116" },
    { 0x41, 0x0001, "006" }
};

uint8_t txBuffer[1];
uint8_t rxBuffer[2];
I2C_Transaction i2cTransaction;

// Driver Handles - Global variables
I2C_Handle i2c;

// Make sure you call initUART() before calling this function.
void initI2C(void)
{
    int8_t i, found;
    I2C_Params i2cParams;
    DISPLAY(snprintf(output, 64, "Initializing I2C Driver - "));

    // Init the driver
    I2C_init();
    // Configure the driver
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    // Open the driver
    i2c = I2C_open(CONFIG_I2C_0, &i2cParams);

    if (i2c == NULL)
    {
        DISPLAY(snprintf(output, 64, "Failed\n\r"));
        while (1);
    }

    DISPLAY(snprintf(output, 32, "Passed\n\r"));

    // Boards were shipped with different sensors.
    // Welcome to the world of embedded systems.
    // Try to determine which sensor we have.
    // Scan through the possible sensor addresses
    /* Common I2C transaction setup */
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 0;

    found = false;
    for (i=0; i<3; ++i)
    {
        i2cTransaction.slaveAddress = sensors[i].address;
        txBuffer[0] = sensors[i].resultReg;
        DISPLAY(snprintf(output, 64, "Is this %s? ", sensors[i].id));
        if (I2C_transfer(i2c, &i2cTransaction))
        {
            DISPLAY(snprintf(output, 64, "Found\n\r"));
            found = true;
            break;
        }
        DISPLAY(snprintf(output, 64, "No\n\r"));
    }

    if(found)
    {
        DISPLAY(snprintf(output, 64, "Detected TMP%s I2C address: %x\n\r", sensors[i].id, i2cTransaction.slaveAddress));
    }
    else
    {
        DISPLAY(snprintf(output, 64, "Temperature sensor not found, contact professor\n\r"));
    }
}

int16_t readTemp(void)
{
    int16_t temperature = 0;
    i2cTransaction.readCount = 2;
    if (I2C_transfer(i2c, &i2cTransaction))
    {
        /*
         * Extract degrees C from the received data;
         * see TMP sensor datasheet
         */
        temperature = (rxBuffer[0] << 8) | (rxBuffer[1]);
        temperature *= 0.0078125;
        /*
         * If the MSB is set '1', then we have a 2's complement
         * negative value which needs to be sign extended
         */
        if (rxBuffer[0] & 0x80)
        {
            temperature |= 0xF000;
        }
    }
    else
    {
        DISPLAY(snprintf(output, 64, "Error reading temperature sensor %d\n\r", i2cTransaction.status));
        DISPLAY(snprintf(output, 64, "Please power cycle your board by unplugging USB and plugging back in.\n\r"));
    }

    return temperature;
}
// ---------------------------------- I2C END ------------------------------------------


/*
 *  ======== gpioButtonFxn0 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_0.
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */
void gpioButtonFxn0(uint_least8_t index)
{
    upBtnPressed = 1;
}

void gpioButtonFxn1(uint_least8_t index)
{
    downBtnPressed = 1;
}

// State machine tick functions
int TickFct_CheckUpBtn(int state) {
    switch(state) {
        case BTN_On:
            if (upBtnPressed) {
                state = BTN_On;
            } else
            {
                state = BTN_Off;
            }
            break;
        case BTN_Off:
            if (upBtnPressed) {
                state = BTN_On;
            } else
            {
                state = BTN_Off;
            }
            break;
        default:
            state = BTN_Off;
            break;
    }

    switch(state) {
        case BTN_On:
            setTempCelsius++;
            upBtnPressed = 0;
            break;
        case BTN_Off:
            break;
    }

    return state;
}

int TickFct_CheckDownBtn(int state) {
    switch(state) {
        case BTN_On:
            if (downBtnPressed) {
                state = BTN_On;
            } else
            {
                state = BTN_Off;
            }
            break;
        case BTN_Off:
            if (downBtnPressed) {
                state = BTN_On;
            } else
            {
                state = BTN_Off;
            }
            break;
        default:
            state = BTN_Off;
            break;
    }

    switch(state) {
        case BTN_On:
            setTempCelsius--;
            downBtnPressed = 0;
            break;
        case BTN_Off:
            break;
    }

    return state;
}

int TickFct_CheckTemp(int state) {
    switch(state) {
        case HTR_Off:
            if (currentTempCelsius < setTempCelsius) {
                state = HTR_On;
            } else {
                state = HTR_Off;
            }
            break;
        case HTR_On:
            if (currentTempCelsius >= setTempCelsius) {
                state = HTR_Off;
            } else {
                state = HTR_On;
            }
            break;
    }

    switch(state) {
        case HTR_Off:
            heaterOn = 0;
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
            break;
        case HTR_On:
            heaterOn = 1;
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
            break;
    }

    return state;
}

// This tick function is really just a state machine with a single state, so state code for it is eliminated
int TickFct_Output(int state) {
    totalTimeElapsed++;

    // Turn on the heater LED if necessary

    // Output the report to the console
    DISPLAY(snprintf(output, 64, "<%02d,%02d,%d,%04d>\r\n", currentTempCelsius, setTempCelsius, heaterOn, totalTimeElapsed))

    return 0;
}

// This tick function is really just a state machine with a single state, so state code for it is eliminated
int TickFct_SetTemp(int state) {
    currentTempCelsius = readTemp();

    return 0;
}

/*
 *  ======== Timer Driver Stuff ========
 */
// Driver Handles - Global variables
Timer_Handle timer0;
volatile unsigned char TimerFlag = 0;

void timerCallback(Timer_Handle myHandle, int_fast16_t status)
{
    timerFlag = 1;
}

void initTimer(void)
{
    Timer_Params params;
    // Init the driver
    Timer_init();
    // Configure the driver
    Timer_Params_init(&params);
    //Every 100 milliseconds
    params.period = 1000000;
    params.periodUnits = Timer_PERIOD_US;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;

    // Open the driver
    timer0 = Timer_open(CONFIG_TIMER_0, &params);
    if (timer0 == NULL) {
        /* Failed to initialized timer */
        while (1) {}
    }
    if (Timer_start(timer0) == Timer_STATUS_ERROR) {
        /* Failed to start timer */
        while (1) {}
    }
}
// ---------------------------------- Timer End ------------------------------------------

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED and button pins */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* Install Button callbacks */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn0);
    GPIO_setCallback(CONFIG_GPIO_BUTTON_1, gpioButtonFxn1);

    /* Enable interrupts */
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);
    GPIO_enableInt(CONFIG_GPIO_BUTTON_1);

    // Initialize the board components. Timer inits with a default 100ms period to accommodate both 200ms and 500ms intervals
    initUART();
    initI2C();
    initTimer();

    // Set the current temp to start to make sure that the check temp state machine doesn't inadvertently
    // turn on the heater before we've accurately captured the current temp
    DISPLAY(snprintf(output, 64, "Reading temperature\n\r"));
    currentTempCelsius = readTemp();
    DISPLAY(snprintf(output, 64, "Current temperature %02d\n\r", currentTempCelsius));

    // Set up the tasks to be handled
    unsigned char i = 0;
    tasks[i].state = 0;                     // The first task has no states. It just always sets the current temperature
    tasks[i].period = 50;
    tasks[i].elapsedTime = tasks[i].period; // All elapsed times will be set to the period so that the init state will run at start-up
    tasks[i].TickFct = &TickFct_SetTemp;
    ++i;
    tasks[i].state = BTN_Off;
    tasks[i].period = 20;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_CheckUpBtn;
    ++i;
    tasks[i].state = BTN_Off;
    tasks[i].period = 20;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_CheckDownBtn;
    ++i;
    tasks[i].state = HTR_Off;
    tasks[i].period = 50;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_CheckTemp;
    ++i;
    tasks[i].state = 0;
    tasks[i].period = 100;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &TickFct_Output;

    while(1) {
        if (timerFlag) {
            // For each tasks, if the amount of time that it's been waiting is at least as long as the period then
            // we need to go ahead and run that task
            for (i = 0; i < numTasks; ++i) {
                if (tasks[i].elapsedTime >= tasks[i].period) {
                    //tasks[i].state = tasks[i].TickFct(tasks[i].state);
                    switch (i) {
                        case 0:
                            tasks[i].state = TickFct_SetTemp(tasks[i].state);
                            break;
                        case 1:
                            tasks[i].state = TickFct_CheckUpBtn(tasks[i].state);
                            break;
                        case 2:
                            tasks[i].state = TickFct_CheckDownBtn(tasks[i].state);
                            break;
                        case 3:
                            tasks[i].state = TickFct_CheckTemp(tasks[i].state);
                            break;
                        case 4:
                            tasks[i].state = TickFct_Output(tasks[i].state);
                            break;
                        default:
                            break;
                    }   // end switch
                    tasks[i].elapsedTime = 0;
                }   // end if elapsed time
                tasks[i].elapsedTime += timerPeriod;
            }   // end for loop

            timerFlag = 0;
        }   // end if (timerFlag)
    }   // end while(1)

    return (NULL);
}
