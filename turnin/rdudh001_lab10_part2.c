/*	Author: Rishab Dudhia
 *  Partner(s) Name: 
 *	Lab Section: 022
 *	Assignment: Lab #10  Exercise #2
 *	Exercise Description: [optional - include for your own benefit]
 *	Locking mechanism: led B0: on = unlocked, off = locked; button B7: press = lock
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 *	
 *	Youtube link: https://www.youtube.com/watch?v=RE33sBm-4oo
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

unsigned char locked = 0;
unsigned char lockB = 0;
//unsigned char outB = 0;

enum LockStates { lstart, lwait, pressed } lstate;
int Tick_Lock (int lstate) {
	unsigned char temp = ~PINB & 0x80;//FIXME
	switch (lstate) {
		case lstart:
			lstate = lwait;
			break;
		case lwait:
			if (temp == 0x00)
				lstate = lwait;
			else
				lstate = pressed;
			break;
		case pressed:
			if (temp == 0x00)
				lstate = lwait;
			else
				lstate = pressed;
			break;
		default:
			lstate = lstart;
			//outB = 1;
			break;
	}

	switch (lstate) {
		case lstart:
			break;
		case lwait:
			lockB = 0;
			break;
		case pressed:
			locked = 0x00;
			lockB = 1;
			//outB = 0x02;
			break;
		default:
			break;
	}
	//PORTB = locked;
	return lstate;
}


enum KeypadStates { kstart, wait, wait_hash, next_wait, next_press, next_release, done } kstate;
int Tick_Keypad (int kstate) {
	static unsigned char i;
	unsigned char val;
	val = GetKeypadKey();
	switch (kstate) {
		case kstart:
			kstate = wait;
			break;
		case wait:
			if (val == '#' && lockB == 0 && locked == 0x00)
				kstate = wait_hash;
			//else if (lockB == 1)
			//	kstate = lock;
			else
				kstate = wait;
			break;
		case wait_hash:
			if (val == '#' && lockB == 0)
				kstate = wait_hash;
			//else if (lockB == 1)
			//	kstate = lock;
			else
				kstate = next_wait;
			break;
		case next_wait:
			if (val == i && lockB == 0)
				kstate = next_press;
			//else if (lockB == 1)
                         //       kstate = lock;
			else if (val == '\0' && lockB == 0)
				kstate = next_wait;
			else if (val == '#' && lockB == 0)
				kstate = wait_hash;
			else
				kstate = wait;
			break;
		case next_press:
			if (val == i && lockB == 0)
				kstate = next_press;
			//else if (lockB == 1)
                        //        kstate = lock;
			else if (val == '\0' && lockB == 0)
				kstate = next_release;
			else if (val == '#' && lockB == 0)
				kstate = wait_hash;
			else
				kstate = wait;
			break;
		case next_release:
			if (val == '\0' && i != '5' && lockB == 0) {
				kstate = next_wait;
				++i;
			}
			//else if (lockB == 1)
                        //        kstate = lock;
			else if (val == '\0' && i == '5' && lockB == 0) 
			        kstate = done;	       
			else if (val == '#' && lockB == 0)
				kstate = wait_hash;
			else
				kstate = wait;
			break;
		case done:
			kstate = wait;
			break;
		//case lock:
			//kstate = wait;
		default:
			kstate = kstart;
			break;
	}

	switch (kstate) {
		case kstart:
		case next_wait:
		case next_press:
		case next_release:
			break;
		case wait:
			i = '0';
			break;
		case wait_hash:
			i = '1';
			break;
		case done:
			locked = 0x01;
		//case lock:
		//	locked = 0x00;
		default:
			break;
	}

	return kstate;
}

enum Together_States { tstart, run } tstate;
int Tick_Together(int tstate) {
	switch (tstate) {
		case tstart:
			tstate = run;
			break;
		case run:
			tstate = run;
			break;
		default:
			tstate = tstart;
			break;
	}

	switch (tstate) {
		case tstart:
			break;
		case run:
			break;
		default:
			break;
	}

	PORTB = locked;

	return tstate;
}

int main(void) {
    /* Insert DDR and PORT initializations */
    DDRB = 0x7F; PORTB = 0x80;
    DDRC = 0xF0; PORTC = 0x0F;

    static task task1, task2, task3;
    task *tasks[] = { &task1, &task2, &task3 };
    const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

    const char start = -1;
    //Task 1 
    task1.state = start;
    task1.period = 50;
    task1.elapsedTime = task1.period;
    task1.TickFct = &Tick_Lock;
    //Task 2
    task2.state = start;
    task2.period = 50;
    task2.elapsedTime = task2.period;
    task2.TickFct = &Tick_Keypad;
    //Task 3
    task3.state = start;
    task3.period = 50;
    task3.elapsedTime = task3.period;
    task3.TickFct = &Tick_Together;

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
