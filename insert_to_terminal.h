/* 
 * File: insert_to_terminal   
 * Author: Filip Fosen
 * Comments: Mainly used for communication to the terminal. 
 * Revision history: 
 */

// This is a guard condition so that contents of this file are not included
// more than once.  
#ifndef INSERT_TO_TERMINAL_H
#define	INSERT_TO_TERMINAL_H

#include <xc.h> // include processor files - each processor file is guarded.  
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <avr/interrupt.h>


#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */

#define BUFFER_SIZE 10
    char inputBuffer[BUFFER_SIZE]; // Buffer for input
    volatile uint8_t bufferIndex = 0;

    volatile int terminalcase = 6;

    volatile uint32_t d_k = 0;
    volatile uint32_t dd_k = 0;

    volatile int T_SWEEP = 2;
    volatile int F_1 = 2000;
    volatile int F_0 = 1000;
    volatile int randomFreqProduced = 0;
    volatile uint16_t dac_val = 0;


    static int USART3_printChar(char c, FILE *stream);

#define USART3_BAUD_RATE(BAUD_RATE) ((float)(F_CPU * 64 / (16 *(float)BAUD_RATE)) + 0.5)
    static FILE USART_stream = FDEV_SETUP_STREAM(USART3_printChar, NULL, _FDEV_SETUP_WRITE);

    void USART3_init(void) {
        PORTB.DIRSET = PIN0_bm;
        PORTB.DIRCLR = PIN1_bm;
        USART3.BAUD = (uint16_t) USART3_BAUD_RATE(9600);
        USART3.CTRLB |= USART_TXEN_bm | USART_RXEN_bm;

        stdout = &USART_stream;
    }

    void USART3_sendChar(char c) {
        while (!(USART3.STATUS & USART_DREIF_bm)); // Wait for data register to empty
        USART3.TXDATAL = c; // Send char
    }

    static int USART3_printChar(char c, FILE *stream) {
        USART3_sendChar(c);
        return 0;
    }

    void USART3_sendString(char *str) {
        for (size_t i = 0; i < strlen(str); i++) {
            USART3_sendChar(str[i]);
        }
    }

    void USART3_flush() {
        while (USART3.STATUS & USART_RXCIF_bm) {
            volatile char dummy = USART3.RXDATAL; // Read and discard
        }
    }

    /**
     * @brief Setter Ã¸nsket verdi pÃ¥ DACen.
     *
     * @Param val 10 bits verdi som skal konverters.
     */
    void DAC0_set_val(uint16_t val) {
        /* Lagrer de to LSbene i DAC0.DATAL */
        DAC0.DATAL = (val & (0x03)) << 6;
        /* Lagrer de Ã¥tte MSbene i DAC0.DATAH */
        DAC0.DATAH = val >> 2;
    }

    void USART3_receiveString() {
        USART3_flush();

        char receivedChar;
        bufferIndex = 0;

        while (1) {
            while (!(USART3.STATUS & USART_RXCIF_bm)); // Wait for received character
            receivedChar = USART3.RXDATAL;

            if (receivedChar == '\r' || receivedChar == '\n') { // Enter key pressed
                inputBuffer[bufferIndex] = '\0'; // Null-terminate string
                break;
            }

            if (bufferIndex < BUFFER_SIZE - 1) { // Prevent overflow
                inputBuffer[bufferIndex++] = receivedChar;
            }
        }
    }

    void t1case() {
        dac_val = 0;
        DAC0_set_val(dac_val);
        printf("Bestem varigheten T (s): ");
        USART3_receiveString();
        T_SWEEP = atoi(inputBuffer); // Konverter string til int
        printf("\nDu skrev: %d\n\r", T_SWEEP);
        terminalcase = 1;
        _delay_ms(1000);
    }

    void highestFrequency() {
        printf("Bestem høyeste frekvens: ");
        USART3_receiveString();
        F_1 = atoi(inputBuffer);
        printf("\nDu skrev: %d\n\r", F_1);
        terminalcase = 3;
        _delay_ms(1000);
    }

    void lowestFrequency() {
        printf("Bestem laveste frekvens: ");
        USART3_receiveString();
        F_0 = atoi(inputBuffer);
        printf("\nDu skrev: %d\n\r", F_0);
        terminalcase = 2;
        _delay_ms(1000);
    }

    void startProgram() {
        printf("Høyeste frekvens: %d \n\r", F_1);
        printf("Laveste frekvens: %d \n\r", F_0);
        printf("Varigheten T: %d s \n\r", T_SWEEP);
        printf("Skriv OK for å gå videre, RESTART for å skrive inn nye verdier.\n\r");

        USART3_receiveString();

        if (strcmp(inputBuffer, "OK") == 0) {
            terminalcase = 4;
            //Deklarer d_k og dd_k
            d_k = (uint32_t) ((float) M_SAMPLES * F_0 / (float) F_SAMPLE * ((uint32_t) 1 << 16));
            dd_k = (uint32_t) ((float) M_SAMPLES * (F_1 - F_0) / ((float) T_SWEEP * F_SAMPLE * F_SAMPLE) * ((uint32_t) 1 << 16));

        } else if (strcmp(inputBuffer, "RESTART") == 0) {
            terminalcase = 0;
            printf("Restarting... \r\n");
        }
        _delay_ms(1000);
    }



#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif	/* XC_HEADER_TEMPLATE_H */

