/** 
 * Auxillary functions
 *
 * Copyright (C) 2016 Marian Hrinko.
 * Written by Marian Hrinko (mato.hrinko@gmail.com)
 *
 * @author      Marian Hrinko
 * @datum       04.02.2017
 * @file        st7735.c
 * @tested      AVR Atmega16
 * @inspiration 
 *
 */
#ifndef F_CPU
  #define F_CPU 16000000
#endif

#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <util/delay.h>
#include "st7735.h"
#include "auxillary.h"


// pocitadlo
volatile int8_t _freq;
// pocitadlo
volatile int8_t _temp = -1;
// pocitadlo
volatile uint8_t _count = 0;
// pocitadlo
volatile uint8_t _index = 0;
// pole hodnot buffra
volatile uint8_t _buffer[WIDTH];

/**
 * @description Show loading
 *
 * @param void
 * @return void
 */
void ShowLoading(void)
{
  // declaration & definition
  uint8_t x;
  uint8_t y;
 
  // calc y
  y = POSITION_YE - POSITION_YS;
  // start x
  x = POSITION_XS;

  // init lcd driver
  St7735Init();

  // clear screen
  ClearScreen(0x0000);
  // set text on position
  SetPosition(50, 20);
  // draw string
  DrawString("OSCOPE Ver.3", 0xffff, X2);

  // check if reach the end
  while (x++ < POSITION_XE) {
    // set window
    SetWindow(POSITION_XS, x, POSITION_YS, POSITION_YE);
    // send color
    SendColor565(0xffff, (x-POSITION_XS+1)*(y+1));
    // update screen
    UpdateScreen();
    // delay
    _delay_ms(10);
  }
}

/**
 * @description Init settings
 *
 * @param void
 * @return void
 */
void StartScope(void)
{
  // init timer 1A
  Timer1AInit();
  // init timer 0
  Timer0Init();
  // init adc
  AdcInit();
  // vselect channel
  ADC_CHANNEL(1);
  // start timer 0
  TIMER0_START(PRESCALER_8);
  // start timer 1A
  TIMER1A_START(PRESCALER_1);  
  // globa interrupts enabled
  sei();
  // loop
  while(1) {
    // show after buffer full
    if (_index > WIDTH) {
      // show buffer
      BufferShow();
      // zero index
      _index = 0;
      // zero counter
      _count = 0;
      // zero counter
      _freq = 0;
    }
  }
}

/***
 * @description Init Timer0
 *
 * @param uint8_t - number of seconds
 * @return void
 */
void Timer0Init(void)
{
  // zero counter
  TCNT0  = 0;
  //    foc0 = 10kHz (20kHz):
  // ---------------------------------------------
  //    OCR0 = [fclk/(N.focnX)] - 1
  //    fclk = 16 Mhz
  //       N = 8
  //    foc0 = 1/Toc0
  // Pozn. 
  // Pri frekvencii 10kHz je pocitadlo dvakrat spustane
  //   40kHz ( 25us) -> OCR0 =  49; N =  8; (ADC PRESCALER 16)
  //   20kHz ( 50us) -> OCR0 =  99; N =  8; (ADC PRESCALER 32)
  //   10kHz (100us) -> OCR0 = 199; N =  8; (ADC PRESCALER 32)
  //    5kHz (0.2ms) -> OCR0 =  49; N = 64; (ADC PRESCALER 32)
  //  2.5kHz (0.4ms) -> OCR0 =  99; N = 64; (ADC PRESCALER 32)
  OCR0 = 49;
  // PIN PB3 - OC0 ako vystupny 
  // DDRB  |= (1 << PB3);
  // Waveform generation - toggle
  TCCR0 |= (1 << COM00);
  // Mod 2 - CTC -> TOP = OCRnX
  TCCR0 |= (1 << WGM01);
  // nulovanie priznaku zhody OCR0 a TCNT0
  // priznak sa nastavuje aj bez povolenia globalnych preruseni
  // a povoleni preruseni od zhody TCNT0 a OCR0
  TIFR  |= (1 << OCF0);
}

/***
 * Inicializacia casovaca Timer1A
 * nastavenie frekvencie snimaneho impulzu
 *
 * @param void - number of seconds
 * @return void
 */
void Timer1AInit(void)
{
  // nuluje pocitadlo
  TCNT1  = 0;
  // foc1A = 1kHz:
  // ---------------------------------------------
  //   OCR1A = [fclk/(2.N.foc1A)] - 1
  //    fclk = 16 Mhz
  //       N = 1
  //   foc1A = 1/Toc1A
  OCR1A = 7999;
  // PIN PD5 - OC1A ako vystupny 
  DDRD  |= (1 << PD5);
  // Waveform generation - toggle
  TCCR1A |= (1 << COM1A0);
  // Mod CTC -> TOP = OCR1A
  TCCR1B |= (1 << WGM12);
}

/***
 * Analogovo digitalny prevodnik 
 * - vstup PC0 (ADC0)
 * - referencne napatie AVcc s externym kondenzatorom na AREF pine
 * - preddelicka 128 bity ADPS2:0 = 8 v ADCSRA, kvoli podmienke, ze 
 *   frekvencia prevodu ma byt v rozmedzi 50-200 kHz. Pri 16Mhz a preddelicke
 *   frekvancia prevodu je 125kHz
 *
 * @param Void
 * @return Void
 */
void AdcInit(void)
{ 
  // referencne napatie AVcc s externym kondenzatorom na AREF pine 
  ADMUX |= (1 << REFS0);
  // zarovnanie do lava -> ADCH
  ADMUX |= (1 << ADLAR);
  // nastavenie prevodu
  ADCSRA |= (1 << ADIE)  | // povolenie prerusenia 
            (1 << ADEN)  | // povolenie prevodu 12 cyklov
            (1 << ADATE);
  // povolenie auto spustania
  // nastavenie preddelicky na 32
  // f = 16Mhz / 32 = 500 kHz  = 2 us
  // Prevod => 2us x 13.5 cykla = 27 us
  ADC_PRESCALER(ADC_PRESCALER_16); 
  // Timer/Counter0 Compare Match
  SFIOR |= (1 << ADTS1) | (1 << ADTS0);
}

/**
 * @description Axis show
 *
 * @param void
 * @return void
 */
void AxisShow()
{
  uint8_t i = OFFSET_X;
  uint16_t color = 0x5C4B;

  // draw axis x
  DrawLineHorizontal(OFFSET_X, OFFSET_X+WIDTH, OFFSET_Y+HEIGHT, color);
  //  draw axis x
  DrawLineHorizontal(OFFSET_X, OFFSET_X+WIDTH, OFFSET_Y, color);
  //  draw axis y
  DrawLineVertical(OFFSET_X, OFFSET_Y, OFFSET_Y+HEIGHT, color);
  //  draw axis y
  DrawLineVertical(OFFSET_X+WIDTH, OFFSET_Y, OFFSET_Y+HEIGHT, color);
  // draw auxillary axis x
  while (i <= WIDTH+OFFSET_X) {
    // draw auxillary signs up
    DrawLineVertical(i, OFFSET_Y, OFFSET_Y+HEIGHT, color);
    // move to right
    i += STEP_X;
  }

  i = OFFSET_Y;
  // draw auxillary axis y
  while (i <= HEIGHT+OFFSET_Y) {
    // draw line
    DrawLineHorizontal(OFFSET_X, OFFSET_X+WIDTH, i, color);
    // move to right
    i += STEP_Y;
  }
}

/**
 * @description Show values on lcd
 *
 * @param void
 * @return void
 */
void BufferShow()
{
  char str[3];
  uint8_t i;
  uint16_t color = 0xffff;

  // zakazanie preruseni
  cli();
  // vymazanie obrazovky
  ClearScreen(0x0000);
  // set text position
  SetPosition(0, OFFSET_Y+HEIGHT - 8);
  // draw text
  DrawString("P=", 0xffff, X1);
  // zapis do retazca
  sprintf(str,"%d", _freq);
  // draw text
  DrawString(str, 0xffff, X1);
  // vykreslenie osi
  AxisShow();
  // zobrazenie nabuffrovanych hodnot
  for (i=0; i<WIDTH; i++) {
    // zapis do retazca
    DrawLine(i+OFFSET_X, i+OFFSET_X+1, HEIGHT+OFFSET_Y-HEIGHT*_buffer[i]/255, HEIGHT+OFFSET_Y-HEIGHT*_buffer[i+1]/255, color);
  }
  // zobrazenie */
  UpdateScreen();
  // vyckanie 1 s
  _delay_ms(1500);
  // povolenie preruseni
  sei();
}