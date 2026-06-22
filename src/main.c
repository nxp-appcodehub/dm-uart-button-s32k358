/*==================================================================================================
 * Project : RTD AUTOSAR 4.9
 * Platform : CORTEXM
 * Peripheral : S32K3XX
 * Dependencies : none
 *
 * Autosar Version : 4.9.0
 * Autosar Revision : ASR_REL_4_9_REV_0000
 * Autosar Conf.Variant :
 * SW Version : 7.0.0
 * Build Version : S32K3_RTD_7_0_0_QLP03_D2512_ASR_REL_4_9_REV_0000_20251210
 *
 * Copyright 2020 - 2026 NXP
 *
 * 	 NXP Proprietary. This software is owned or controlled by NXP and may only be
 *   used strictly in accordance with the applicable license terms. By expressly
 *   accepting such terms or by downloading, installing, activating and/or otherwise
 *   using the software, you are agreeing that you have read, and that you agree to
 *   comply with and are bound by, such license terms. If you do not agree to be
 *   bound by the applicable license terms, then you may not retain, install,
 *   activate or otherwise use the software.
 ==================================================================================================*/

/**
 *   @file main.c
 *
 *   @addtogroup main_module main module documentation
 *   @{
 */

#ifdef __cplusplus
extern "C"{
#endif

/* Including necessary configuration files. */
#include "Mcal.h"
#include "Clock_Ip.h"
#include "OsIf.h"
#include "OsIf_Timer_System.h"
#include "Siul2_Dio_Ip.h"
#include "Siul2_Port_Ip.h"
#include "Lpuart_Uart_Ip.h"
#include <string.h>
#include <stdio.h>

#define LPUART_INSTANCE 6
#define BTN_DELAY_TIME 20
#define RX_BUFFER_SIZE 64

char HelloMsg[] =
		"Hello from NXP FRDM-A-S32K358!\r\n\r\n"
		"Available Commands:\r\n"
		"  help      - Show this help message\r\n"
		"  led on    - Turn on green LED\r\n"
		"  led off   - Turn off green LED\r\n"
		"  status    - Show current LED state\r\n"
		"\r\nHardware Controls:\r\n"
		"  SW2/SW3   - Toggle green LED on/off\r\n"
		"  Blue LED  - Indicates UART activity\r\n"
		"> ";

static char FormattedMessage[150];
static uint8 RxBuffer[RX_BUFFER_SIZE];
static char CommandBuffer[RX_BUFFER_SIZE];
static uint8 CommandIndex = 0;
static boolean CommandReady = false;
static boolean GreenLedState = false;

void Delay(uint32 delayMs) {
	uint32 cur = OsIf_GetCounter(OSIF_COUNTER_SYSTEM);
	uint32 elapsed = 0U;
	uint32 timeout = OsIf_MicrosToTicks(delayMs, OSIF_COUNTER_SYSTEM);
	while (elapsed < timeout) {
		elapsed += OsIf_GetElapsed(&cur, OSIF_COUNTER_SYSTEM);
	}
}

void SendMessage(char Message[]) {
    uint32 msgLen = (uint32) strlen(Message);
    uint32 bytesRemaining = 0U;
    uint8 TxBuff;

    for (uint32 index = 0U; index < msgLen; index++) {
        TxBuff = (uint8) Message[index];
        /*
         * Wait for driver to be ready
         */
        while (Lpuart_Uart_Ip_GetTransmitStatus(LPUART_INSTANCE, &bytesRemaining)
                == LPUART_UART_IP_STATUS_BUSY)
            ;
        /*
         * Send data to console
         */
        Lpuart_Uart_Ip_SyncSend(LPUART_INSTANCE, &TxBuff, 1, 100000);
        /*
         * Wait for transmission to complete
         */
        while (Lpuart_Uart_Ip_GetTransmitStatus(LPUART_INSTANCE, &bytesRemaining)
                == LPUART_UART_IP_STATUS_BUSY)
            ;
    }
}

void BlinkBlueLed(void) {
    // Turn on blue LED to indicate UART activity
    Siul2_Dio_Ip_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, 1);

    // Short delay (50ms)
    Delay(50);

    // Turn off blue LED
    Siul2_Dio_Ip_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, 0);
}

void ReceiveCommand(void) {
    uint32 bytesRemaining = 0;
    Lpuart_Uart_Ip_StatusType status;

    // Check if data is actually waiting before trying to receive
    status = Lpuart_Uart_Ip_GetReceiveStatus(LPUART_INSTANCE, &bytesRemaining);

    // Only attempt receive if there's data or receiver is idle/ready
    if (status == LPUART_UART_IP_STATUS_SUCCESS ||
        status == LPUART_UART_IP_STATUS_TIMEOUT) {

        // Use a longer timeout, or loop until a byte arrives
        status = Lpuart_Uart_Ip_SyncReceive(LPUART_INSTANCE, RxBuffer, 1, 100000); // 100ms

        if (status == LPUART_UART_IP_STATUS_SUCCESS) {
            uint8 receivedChar = RxBuffer[0];

            if (receivedChar == '\b' || receivedChar == 127) {
                if (CommandIndex > 0) {
                    CommandIndex--;
                    SendMessage("\b \b");
                }
            }
            else if (receivedChar == '\r' || receivedChar == '\n') {
                CommandBuffer[CommandIndex] = '\0';
                CommandReady = true;
                SendMessage("\r\n");
            }
            else if (CommandIndex < (RX_BUFFER_SIZE - 1) &&
                     receivedChar >= 32 && receivedChar <= 126) {
                CommandBuffer[CommandIndex++] = (char)receivedChar;
                Lpuart_Uart_Ip_SyncSend(LPUART_INSTANCE, &receivedChar, 1, 100000);
            }
        }
    }
}

void ProcessCommand(void) {
    // Only process AND clear if Enter was pressed
    if (!CommandReady) {
        return;
    }

    // Blink blue LED to show UART activity
    BlinkBlueLed();

    // Convert command to lowercase for easier comparison
    for (uint8 i = 0; i < CommandIndex; i++) {
        if (CommandBuffer[i] >= 'A' && CommandBuffer[i] <= 'Z') {
            CommandBuffer[i] = CommandBuffer[i] + 32;
        }
    }

    // Parse and execute commands
    if (strcmp(CommandBuffer, "help") == 0) {
        SendMessage(HelloMsg);
    }
    else if (strcmp(CommandBuffer, "led on") == 0) {
        Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, 1);
        GreenLedState = true;
        SendMessage("Green LED turned ON\r\n> ");
    }
    else if (strcmp(CommandBuffer, "led off") == 0) {
        Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, 0);
        GreenLedState = false;
        SendMessage("Green LED turned OFF\r\n> ");
    }
    else if (strcmp(CommandBuffer, "status") == 0) {
        if (GreenLedState) {
            SendMessage("Green LED is ON\r\n> ");
        } else {
            SendMessage("Green LED is OFF\r\n> ");
        }
    }
    else if (CommandIndex > 0) {
        snprintf(FormattedMessage, sizeof(FormattedMessage),
                 "Unknown command: '%s'. Type 'help' for available commands.\r\n> ",
                 CommandBuffer);
        SendMessage(FormattedMessage);
    }
    else {
        SendMessage("> ");
    }

    /* These must stay INSIDE the execution block, otherwise
     *  they clear the buffer on loops where no data was received.
     */
    CommandIndex = 0;
    CommandReady = false;
    memset(CommandBuffer, 0, RX_BUFFER_SIZE);
}


/*!
 \brief The main function for the project.
 \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
 */
int main(void) {
    boolean sw2_pressed = false;
    boolean sw3_pressed = false;

    /* MCU Initialization */
    Clock_Ip_Init(&Clock_Ip_aClockConfig[0]);
    OsIf_Init(NULL_PTR);
    OsIf_Timer_System_Init();
    Siul2_Port_Ip_Init(
    NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
            g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals);
    Lpuart_Uart_Ip_Init(LPUART_INSTANCE, &Lpuart_Uart_Ip_xHwConfigPB_6);

    /* ------------------------------------------------------------------
     *  SAFE INITIAL STATE
     * ------------------------------------------------------------------ */

    Siul2_Dio_Ip_ClearPins(LED_BLUE_PORT, LED_BLUE_PIN);
    Siul2_Dio_Ip_ClearPins(LED_RED_PORT, LED_RED_PIN);
    Siul2_Dio_Ip_ClearPins(LED_GREEN_PORT, LED_GREEN_PIN);

    /* ------------------------------------------------------------------
     *  HELLO MESSAGE
     * ------------------------------------------------------------------ */

    SendMessage(HelloMsg);

    while (true) {
        /* Check for UART commands */
        ReceiveCommand();
        ProcessCommand();

        /* SW2: Toggle Green LED on/off with debounce */
        if(Siul2_Dio_Ip_ReadPin(SW2_PORT, SW2_PIN)){
            if(!sw2_pressed){
                sw2_pressed = true;
                if(GreenLedState){
                    GreenLedState = false;
                    Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, 0);
                    SendMessage("\r\nGreen LED turned OFF (SW2 Button)\r\n> ");
                } else {
                    GreenLedState = true;
                    Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, 1);
                    SendMessage("\r\nGreen LED turned ON (SW2 Button)\r\n> ");
                }
                Delay(BTN_DELAY_TIME);
            }
        } else {
            sw2_pressed = false;
        }

        /* SW3: Toggle Green LED on/off with debounce */
        if(Siul2_Dio_Ip_ReadPin(SW3_PORT, SW3_PIN)){
            if(!sw3_pressed){
                sw3_pressed = true;
                if(GreenLedState){
                    GreenLedState = false;
                    Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, 0);
                    SendMessage("\r\nGreen LED turned OFF (SW3 Button)\r\n> ");
                } else {
                    GreenLedState = true;
                    Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, 1);
                    SendMessage("\r\nGreen LED turned ON (SW3 Button)\r\n> ");
                }
                Delay(BTN_DELAY_TIME);
            }
        } else {
            sw3_pressed = false;
        }
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

/** @} */
