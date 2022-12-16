/*
 * Copyright (c) 2020, Texas Instruments Incorporated
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
 *  ======== uart2echo.c ========
 */
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>

/* Driver configuration */
#include "ti_drivers_config.h"

// Shared variables to track entered key stroke and LED status
volatile char input;
volatile char ledOn;  // bit

/* Handle the LED state machine */
enum LED_States { LED_ON, LED_OFF } LED_State;
void TickFunction_SetLED() {
    // Handle transitions of states
    switch(LED_State) {
        case LED_OFF:
            if (ledOn) {
                LED_State = LED_ON;
            }
            else {
                LED_State = LED_OFF;
            }
            break;

        case LED_ON:
            if (ledOn) {
                LED_State = LED_ON;
            }
            else {
                LED_State = LED_OFF;
            }
            break;

        default:
            LED_State = LED_OFF;
            break;
    }

    // Handle the activities for each state
    switch (LED_State) {
        case LED_ON:
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
            break;
        case LED_OFF:
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
            break;
        default:
            break;
    }
}

/* Handle the data entry state machine */
enum ENTRY_States { ENTRY_Init, ENTRY_O, ENTRY_ON, ENTRY_OF, ENTRY_OFF } ENTRY_State;
void TickFunction_TrackEntry() {
    // Handle transitions of states
    switch(ENTRY_State) {
        case ENTRY_Init:
            if (input == 'O') {
                ENTRY_State = ENTRY_O;
            } else {
                ENTRY_State = ENTRY_Init;
            }
            break;

        case ENTRY_O:
            if (input == 'F') {
                ENTRY_State = ENTRY_OF;
            } else if (input == 'N') {
                ENTRY_State = ENTRY_ON;
            } else if ((input != 'F') && (input != 'N')) {
                ENTRY_State = ENTRY_Init;
            }
            break;

        case ENTRY_OF:
            if (input == 'F') {
                ENTRY_State = ENTRY_OFF;
            } else if ((input != 'F') && (ledOn)) {
                ENTRY_State = ENTRY_ON;
            } else if ((input != 'F') && (!ledOn)) {
                ENTRY_State = ENTRY_Init;
            }
            break;

        case ENTRY_ON:
            if (input == 'O') {
                ENTRY_State = ENTRY_O;
            } else if (input != 'O') {
                ENTRY_State = ENTRY_ON;
            }
            break;

        case ENTRY_OFF:
            ENTRY_State = ENTRY_Init;
            break;

        default:
            ENTRY_State = ENTRY_Init;
            break;
    }

    // Handle the activities for each state
    switch (ENTRY_State) {
        case ENTRY_Init:
            ledOn = 0;
            break;
        case ENTRY_O:
            break;
        case ENTRY_OF:
            break;
        case ENTRY_ON:
            ledOn = 1;
            break;
        case ENTRY_OFF:
            ledOn = 0;
            break;
        default:
            break;
    }
}

/*
 *  ======== mainThread ========
 */
void* mainThread(void *arg0)
{
    UART2_Handle uart;
    UART2_Params uartParams;
    size_t bytesRead;
    size_t bytesWritten = 0;
    uint32_t status = UART2_STATUS_SUCCESS;
    char input_local;

    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED pin */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Create a UART where the default read and write mode is BLOCKING */
    UART2_Params_init(&uartParams);
    uartParams.baudRate = 115200;

    uart = UART2_open(CONFIG_UART2_0, &uartParams);

    if (uart == NULL)
    {
        /* UART2_open() failed */
        while (1)
            ;
    }

    /* Turn on user LED to indicate successful initialization */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    /* Loop forever echoing */
    while (1)
    {
        status = UART2_read(uart, &input_local, 1, &bytesRead);

        if (status != UART2_STATUS_SUCCESS)
        {
            /* UART2_read() failed */
            while (1);
        } else {
            input = input_local;
        }

        status = UART2_write(uart, &input_local, 1, &bytesWritten);

        if (status != UART2_STATUS_SUCCESS)
        {
            /* UART2_write() failed */
            while (1);
        }

        TickFunction_TrackEntry();
        TickFunction_SetLED();

    }
}
