/*	Author: Rishab Dudhia
 *  Partner(s) Name: 
 *	Lab Section: 022
 *	Assignment: Lab #10  Exercise #4
 *	Exercise Description: [optional - include for your own benefit]
 *	Locking mechanism: led B0: on = unlocked, off = locked; button B7: press = lock; 
 *	button A7: doorbell tone played on speaker; B7 and '*' allow reset
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 *
 *	Youtube link: https://www.youtube.com/watch?v=wGW0XkKZIpc
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#include "../header/keypad.h"
#endif

//0.954 hz is lowest frequency possible with this function
//based on settings in PWM_on()
//Passing in 0 as the frequency will stop the speaker from generating sound
void set_PWM(double frequency) {
	static double current_frequency; //keeps track of the currently set frequency
	//Will only update the registers when the frequency changes, otherwise allows
	//music to play uninterrupted
	if (frequency != current_frequency) {
		if (!frequency) { TCCR3B &= 0x08; } // stops timer/counter
		else { TCCR3B |= 0x03; } // resumes/continues timer/counter

		//prevents OCR3A from overflowing, using prescaler 64
		//0.954 is smallest frequency that will not result in overflow
		if (frequency < 0.954) { OCR3A = 0XFFFF; }

		//prevents OCR3A from underflowing, using prescaler 64
		//31250 is largest frequency that will not result in underflow
		else if (frequency > 31250) { OCR3A = 0x0000; }

		//set OCR3A based on desired frequency
		else { OCR3A = (short)(8000000 / (128 * frequency)) -1; }

		TCNT3 = 0; //resets counter
		current_frequency = frequency; // updates the current frequency
	}
}

void PWM_on() {
	TCCR3A = (1 << COM3A0);
		// COM3A0: toggle PB3 on compare match between counter and OCR3A
	TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);
		//WGM32: when counter (TCNT3) matches OCR3A, reset counter
		//CS31 & CS30: set a prescaler of 64
	set_PWM(0);
}

void PWM_off() {
	TCCR3A = 0x00;
	TCCR3B = 0x00;
}


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

unsigned char seqNums = 5;
unsigned char sequence[5] = {'1','2','3','4','5'};

unsigned char test = 0;

enum KeypadStates { kstart, wait, wait_hash, next_wait, next_press, next_release, done } kstate;
int Tick_Keypad (int kstate) {
	static unsigned char i = 0;
	unsigned char val;
	val = GetKeypadKey();
	unsigned char curr;
	curr = sequence[i];
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
			if (val == curr && lockB == 0)
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
			if (val == curr && lockB == 0)
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
			if (val == '\0' && i < (seqNums - 1) && lockB == 0) {
				kstate = next_wait;
				++i;
			}
			//else if (lockB == 1)
                        //        kstate = lock;
			else if (val == '\0' && i == (seqNums - 1) && lockB == 0) 
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
			test = 1;
			break;
	}

	switch (kstate) {
		case kstart:
		case next_wait:
		case next_press:
		case next_release:
			break;
		case wait:
			i = 0x00;
			break;
		case wait_hash:
			i = 0x00;
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

enum Bell_States {bstart, bwait, play, bdone } bstate;
int Tick_Bell (int bstate) {
	unsigned char check = ~PINA & 0x80;
	double freqs[12] = {523.25,523.25,440,440,440,440,0,261.63, 293.66,392,0,392};
	static unsigned char i;
	static double res;

	switch (bstate) {
		case bstart:
			bstate = bwait;
			break;
		case bwait:
			if (check == 0x00)
				bstate = bwait;
			else {
				bstate = play;
				i = 0;
			}
			break;
		case play:
			if (i < 12)
				bstate = play;
			else if (i == 12 && check == 0x80)
				bstate = bdone;
			else
				bstate = bwait;
			break;
		default:
			bstate = bstart;
			break;
	}

	switch (bstate) {
		case bstart:
			break;
		case bwait:
		case bdone:
			set_PWM(0);
			break;
		case play:
			res = freqs[i];
			set_PWM(res);
			++i;
			break;
		default:
			break;
	}

	return bstate;
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

unsigned char newSeq[4];
enum Set_States { sstart, wait_p, wait_r, set_p, set_r, done1, done_wait, waiting, final_set } sstate;
int Tick_Set (int sstate) {
	static unsigned char i = 0;
	static unsigned char k = 0;
	static unsigned char j = 0;
	unsigned char temp;
	temp = GetKeypadKey();

	switch (sstate) {
		case sstart:
			sstate = wait_p;
			break;
		case wait_p:
			if (lockB != 0 && temp == '*')
				sstate = wait_r;
			else
				sstate = wait_p;
			break;
		case wait_r:
			if (lockB == 0 && temp == '\0') {
				sstate = set_p;
				i = 0;
			}
			else
				 sstate = wait_r;
			break;
		case set_p:
			if (temp == '\0')
				sstate = set_p;
			else
				sstate = set_r;
			break;
		case set_r:
			if (temp == '\0' && i < 3) {
				sstate = set_p;
				++i;
			}
			else if (temp == '\0' && i == 3) {
				sstate = done1;
				k = 0;
			}
			else
				sstate = set_r;
			break;
		case done1:
			if (temp == '\0' && k < 10) {
				sstate = done1;
				++k;
			}
			else if (temp == 'D')
				sstate = done_wait;
			else
				sstate = wait_p;
			break;
		case done_wait:
			if (temp == 'D')
				sstate = done_wait;
			else {
				sstate = final_set;
				j = 0;
			}
			break;
		case waiting:  
			if (temp == '\0' && j < 10) {
				sstate = waiting;
				++j;
			}
			else if (temp == newSeq[0] && j < 10)
				sstate = final_set;
			else
				sstate = wait_p;
			break;
		case final_set:
			sstate = wait_p;
			break;
		default:
			sstate = sstart;
			break;
	}

	switch (sstate) {
		case sstart:
		case wait_p:
		case wait_r:
		case set_p:
		case done1:
		case done_wait:
		case waiting:
			break;
		case set_r:
			newSeq[i] = temp;
			break;
		case final_set:
			for (unsigned char h = 0; h < 4; ++h) {
				sequence[h] = newSeq[h];
			}
			seqNums = 4;
			break;
		default:
			break;
	}

	return sstate;
}

int main(void) {
    /* Insert DDR and PORT initializations */
    DDRA = 0x00; PORTA = 0xFF;
    DDRB = 0x7F; PORTB = 0x80;
    DDRC = 0xF0; PORTC = 0x0F;

    static task task1, task2, task3, task4, task5;
    task *tasks[] = { &task1, &task2, &task3, &task4, &task5 };
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
    //Task 4
    task4.state = start;
    task4.period = 200;
    task4.elapsedTime = task4.period;
    task4.TickFct = &Tick_Bell;
    //Task 5
    task5.state = start;
    task5.period = 200;
    task5.elapsedTime = task5.period;
    task5.TickFct = &Tick_Set;

    //Set the timer and turn it on
    TimerSet(50);
    TimerOn();
    PWM_on();

    unsigned short k;
    /* Insert your solution below */
    while (1) {
	for (k = 0; k < numTasks; ++k) {
		if (tasks[k]->elapsedTime == tasks[k]->period) {
			tasks[k]->state = tasks[k]->TickFct(tasks[k]->state);
			tasks[k]->elapsedTime = 0;
		}
		tasks[k]->elapsedTime += 50;
	}

	while (!TimerFlag);
	TimerFlag = 0;
    }
    return 1;
}
