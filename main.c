/*
 * File:   main.c
 * Author: Filip Fosen
 * 
 * Dette programmet gjennomfÃ¸rer et frekvenssveip ved hjelp av direkte digital syntese (DDS) pÃ¥ DACen til AVR128DB48. Sveipet gjennomfÃ¸res ved hjelp av telleren TCA0 og er avbruddsstyrt for Ã¥ fÃ¥ en jevn
 * punktprÃ¸vingsfrekvens. Prinsippene for DDS og frekvenssveip, samt noen praktiske detaljer, er beskrevet det det tekniske notatet [1]. Sveipet bruker en cosinus-oppslagstabell pÃ¥ 2^13 punkter, lagret i "cosine_table.h".
 * Noen detaljer:
 * - Utgangspinne for DAC: pinne PD6.
 * - Prossessorfrekvens: 24MHz.
 * - PunktprÃ¸vingsfrekvens: 16384 Hz.
 *
 * Kilder:
 * [1] - L. Lundheim: Generering av frekvenssveip, internt notat, NTNU, 2025
 * Created on 6. februar 2025, 14:42
 */

/* Faste konstanter - disse trenger man i utgangspunktet ikke Ã¥ justere*/
#define F_CPU 24000000UL
#define F_SAMPLE 16384 /* Hz - Samplingsfrekvens */
#define M_SAMPLES 8192 /* Antall punktprÃ¸ver i oppslagstabellen */

#include <avr/io.h>
#include <avr/cpufunc.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "cosine_table.h"
#include "insert_to_terminal.h"



volatile int outOfInterrupt = 0;

/**
 * @brief Initialiserer hovedklokkka (F_CPU) til maksimal hastighet: 24MHz. 
 * Det er mulig Ã¥ endre til andre klokkefrekvenser i funksjonen.
 */
void CLK_configuration(void) {
    /* Setter OSCHF-klokka til 24 MHz */
    ccp_write_io((uint8_t *) & CLKCTRL.OSCHFCTRLA, CLKCTRL_FRQSEL_24M_gc
            | CLKCTRL_AUTOTUNE_bm);
}

/**
 * @brief Initialiserer DACen. Den bruker pinne PD6.
 */
void DAC0_init(void) {
    /* Konfigurerer pinne PD6*/
    /* Deaktiverer digital input buffer */
    PORTD.PIN6CTRL &= ~PORT_ISC_gm;
    PORTD.PIN6CTRL |= PORT_ISC_INPUT_DISABLE_gc;

    /* Deaktiverer opptrekksmotstand */
    PORTD.PIN6CTRL &= ~PORT_PULLUPEN_bm;

    VREF.DAC0REF |= VREF_REFSEL_VDD_gc;

    /* Aktiverer DACen, utgangsbuffer */
    DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm;
}

/**
 * @brief Initialiserer telleren TCA0. Telleren aktiveres ikke.
 */
void TCA0_init() {
    /* Konfigrerer tellerperioden (hvor mange klokkesykluser den skal telle).
     *  Formel: F_CPU/(F_CNT*SKALERING)
     */
    TCA0.SINGLE.PER = (uint16_t) (F_CPU / (F_SAMPLE * 1));

    /* Aktiverer avbrudd fra telleroverflyt */
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;

    /* Setter telleren i 16-bits modus med en klokkefrekvens pÃ¥ F_FPU/1. */
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc;

}

/*
 * @brief Skriver ut en frekvens
 */

/**
 * @brief Setter i gang et sveip ved Ã¥ aktivere telleren.
 */
void run_sweep() {
    /* Tenner LED0 for Ã¥ markere start pÃ¥ sveip */
    PORTB.OUT &= ~PIN3_bm;

    /* Aktiverer telleren for Ã¥ starte sveip*/
    TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm;
}

/**
 * @brief Avbruddsrutine for telleren. Denne hÃ¥ndterer frekvenssveipet.
 */
ISR(TCA0_OVF_vect) {
    static int16_t curr_sample = 0;
    static uint16_t k = 0;

    if (terminalcase == 4) {
        d_k = (d_k + dd_k) % ((uint32_t) F_1 << 15);
    }
    k = (k + (d_k >> 16)) % M_SAMPLES;

    curr_sample = sineLookupTable[k];
    dac_val = (curr_sample >> 6) + 512;
    DAC0_set_val(dac_val);

    if (d_k == 0 || outOfInterrupt == 1) {
        terminalcase = 7;
        PORTB.OUT = PIN3_bm;
        TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;
        d_k = (uint32_t) ((float) M_SAMPLES * F_0 / (float) F_SAMPLE * ((uint32_t) 1 << 16));
        outOfInterrupt = 0;
    }
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

ISR(PORTB_PORT_vect) {
    if (PORTB.INTFLAGS & PIN2_bm) {
        if (terminalcase == 5) {
            outOfInterrupt = 1;
            terminalcase = 0;
            dac_val = 0;
            DAC0_set_val(dac_val);
        }
        else if (terminalcase == 4) {

            terminalcase = 7;
            dac_val = 0;
            DAC0_set_val(dac_val);
            outOfInterrupt = 1;
        } else if (terminalcase == 8) {
            terminalcase = 5;
        }
        PORTB.INTFLAGS = PIN2_bm;
    }
}

void didYouHit() {
    uint32_t frequencySample = (uint32_t) ((d_k >> 16) * F_SAMPLE) / M_SAMPLES;

    int16_t frequencyDifference = abs(frequencySample - randomFreqProduced);

    printf("Frekvensforskjell: %d \n\r", frequencyDifference);
    printf("Du traff frekvensen: %d \n\r", frequencySample);
    printf("Frekvensen du skulle treffe: %d \n\r", randomFreqProduced);
    printf("Trykk på knappen for å spille på nytt. \n\r");
    terminalcase = 8;
}

void randomFrequencyMaker() {
    for (int i = 0; i < 10; i++) {
        F_0 = rand() % (2000); //Fra 0-2000hz
    }
    randomFreqProduced = F_0;
    d_k = (uint32_t) ((float) M_SAMPLES * F_0 / (float) F_SAMPLE * ((uint32_t) 1 << 16));
    terminalcase = 5;
}

void gameCase() {
    switch (terminalcase) {
        case 0:
            t1case();
            break;
        case 1:
            lowestFrequency();
            break;
        case 2:
            highestFrequency();
            break;
        case 3:
            startProgram();
            break;
        case 4:
            run_sweep();
            break;
        case 5:
            run_sweep(); //ddk er 0
            break;
        case 6:
            randomFrequencyMaker();
            break;
        case 7:
            didYouHit();
            break;
        case 8:
            ;
            break;
    }
}

int main(void) {
    srand(time(NULL));
    USART3_init();
    /* Setter hovedklokka til 24MHz for Ã¥ fÃ¥ hÃ¸y samplingsfrekvens. */
    CLK_configuration();

    /* Initialiserer DACen*/
    DAC0_init();

    /* Intitaliserer telleren. Merk: den aktiveres ikke.*/
    TCA0_init();


    /* Setter pinne PB2, BTN0 pÃ¥ kortet, som inngang. */
    PORTB.DIR &= ~PIN2_bm;
    PORTB.PIN2CTRL |= PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc;

    /* Setter pinne PB3, LED0 pÃ¥ kortet, som utgang. */
    PORTB.DIRSET = PIN3_bm;
    PORTB.OUT = PIN3_bm;

    sei();

    srand(time(NULL));

    /* Kjører et frekvenssveip med de definerte verdiene */

    while (1) {
        gameCase();
    }
}
