/*	Author: Rishab Dudhia
 *  Partner(s) Name: 
 *	Lab Section: 022
 *	Assignment: Lab #10  Exercise #1
 *	Exercise Description: [optional - include for your own benefit]
 *	Light on PB7 turns on when any button on keypad pressed
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#include "../header/keypad.h"
#endif

volatile unsigned char TimerFlag = 0; // TimerISR() sets to 1

unsigned long _avr_timer_M = 1; //start count from here, down to 0. default 1 ms
unsigned long _avr_timer_cntcurr = 0; //current internal count of 1ms ticks

void TimerOn () {
	//AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B; //bit3 = 0: CTC mode (clear timer on compare)
		       //bit2bit1bit0 = 011: pre-scaler / 64
		       //00001011: 0x0B
		       //so, 8 MHz clock or 8,000,000 / 64 = 125,000 ticks/s
		       //thus, TCNT1 register will count at 125,000 ticks/s
	//AVR output compare register OCR1A
	OCR1A = 125; //timer interrupt will be generated when TCNT1 == OCR1A
	             //we want a 1 ms tick. 0.001s * 125,000 ticks/s = 125
		     //so when TCNT1 register equals 125,
		     //1 ms has passed. thus, we compare 125.

	//AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1 = 0;

	_avr_timer_cntcurr = _avr_timer_M;
	//TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 10000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0 = 000: timer off
}

void TimerISR() {
	TimerFlag = 1;
}

//In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect){
	//CPU automatically calls when TCNT1 == OCR1 (every 1ms per TimerOn settings)
	_avr_timer_cntcurr--; // count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

//Set TimerISR() to tick every M ms
void TimerSet(unsigned long M){
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

typedef struct _task {
	signed char state;
	unsigned long int period;
	unsigned long int elapsedTime;
	int (*TickFct)(int);
} task;

unsigned char pressed = 0;
unsigned char temp = 0;

enum States0 {smstart0, off, on} state0;
int Tick_PressLight(int state0) {
	switch (state0) {
		case smstart0:
			state0 = off;
			break;
		case off:
			if (pressed)
				state0 = on;
			else
				state0 = off;
			break;
		case on:
			if (pressed)
				state0 = on;
			else
				state0 = off;
			break;
		default:
			state0 = smstart0;
			break;
	}

	switch (state0) {
		case smstart0:
			break;
		case off:
			temp = 0x7F & temp;
			break;
		case on:
			temp = 0x80 | temp;
			break;
		default:
			break;
	}
	
	PORTB = temp;
	return state0;
}

enum States1 {smstart1, go} state1;
int Tick_KeyLights(int state1) {
	switch (state1) {
		case smstart1:
			state1 = go;
			break;
		case go:
			state1 = go;
			break;
		default:
			state1 = smstart1;
			break;
	}

	switch (state1) {
		unsigned char x;

		case smstart1:
			break;
		case go:
			x = GetKeypadKey();		
        		switch (x) {
                		case '\0':
                        		temp = 0x1F;// All 5 LEDs on
					pressed = 0;
					break;
                		case '1':
                        		temp = 0x01;
					pressed = 1;       
					break; // hex
                		case '2':
                        		temp = 0x02;
					pressed = 1;       
					break;
                		case '3':
                        		temp = 0x03; 
					pressed = 1;
					break;
                		case '4':
                        		temp =  0x04; 
					pressed = 1;
					break;
                		case '5':
                        		temp = 0x05; 
					pressed = 1;
					break;
                		case '6':
                        		temp = 0x06; 
					pressed = 1;
					break;
                		case '7':
                        		temp = 0x07; 
					pressed = 1;
					break;
                		case '8':
                        		temp = 0x08; 
					pressed = 1;
					break;
                		case '9':
                        		temp = 0x09; 
					pressed = 1;
					break;
                		case 'A':
                        		temp = 0x0A; 
					pressed = 1;
					break;
				case 'B':
                        		temp = 0x0B; 
					pressed = 1;
					break;
                		case 'C':
                        		temp = 0x0C; 
					pressed = 1;
					break;
                		case 'D':
                        		temp = 0x0D; 
					pressed = 1;
					break;
                		case '0':
                        		temp = 0x00; 
					pressed = 1;
					break;
                		case '*':
                        		temp = 0x0E; 
					pressed = 1;
					break;
                		case '#':
                        		temp = 0x0F; 
					pressed = 1;
					break;
                		default:
                        		temp = 0x1B; 
					pressed = 0;
					break; //should never occur. Middle led off
			}
			break;
		default:
			break;
	}
	return state1;
}

	



int main(void) {
    /* Insert DDR and PORT initializations */
    DDRB = 0xFF; PORTB = 0x00;
    DDRC = 0xF0; PORTC = 0x0F;

    static task task1, task2;
    task *tasks[] = { &task1, &task2 };
    const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

    const char start = -1;
    //Task 1 
    task1.state = start;
    task1.period = 50;
    task1.elapsedTime = task1.period;
    task1.TickFct = &Tick_KeyLights;
    //Task 2
    task2.state = start;
    task2.period = 50;
    task2.elapsedTime = task2.period;
    task2.TickFct = &Tick_PressLight;

    //Set the timer and turn it on
    TimerSet(50);
    TimerOn();

    unsigned short i;
    /* Insert your solution below */
    while (1) {
	for (i = 0; i < numTasks; ++i) {
		if (tasks[i]->elapsedTime == tasks[i]->period) {
			tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
			tasks[i]->elapsedTime = 0;
		}
		tasks[i]->elapsedTime += 50;
	}

	while (!TimerFlag);
	TimerFlag = 0;
    }
    return 1;
}
