

#include "stepper.h"
#include "config.h"
#include "driver.h"
#include "timers.h"
#include "uart.h"

#include <stdlib.h>
#include <math.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define SPEED_UP	0xF1
#define SPEED_FLAT	0xF2
#define SPEED_DOWN	0xF3
#define SPEED_HALT 	0xF0

// Stepper structure instance
stepper_s stepper;

float cn;
float c0;
float cmin;
uint16_t n;
volatile uint8_t spd;		// state variable

volatile int32_t current_pos;
volatile int32_t target_pos;
uint8_t dir;

int32_t queue_pos;
uint8_t queue_full;

// timer frequency: CPU_clk / prescaler
static const float f = 16000000 / 8;

static void compute_c(void);

static void pulse(void){

	DRV_STEP_PORT |= (1<<DRV_STEP_PIN);
	_delay_us(2);
	DRV_STEP_PORT &= ~(1<<DRV_STEP_PIN);
	if (dir == CW) current_pos++;
	else current_pos--;
}

static void queue_motion(int32_t p) {

	queue_pos = p;
	queue_full = TRUE;
}

void stepper_init(void) {

	drv_reset();
	drv_set(ENABLE);
	drv_step_mode(MODE_EIGHTH_STEP);
	drv_dir(CW, &dir);
	
	current_pos = 0;
	target_pos = 0;

	// minimum counter value to get max speed
	cmin = 249.0;		// Valid for MODE_EIGHTH_STEPPING and 2MHz clk
	stepper_set_accel(8000.0);	// Based on the MODE_EIGHTH_STEPPING parameter
	cn = c0;
	n = 0;
	spd = SPEED_HALT;
	queue_full = FALSE;
}

void stepper_set_accel(float a){
	
	c0 = 0.676 * f * sqrt(2.0 / a);		// Correction based on David Austin paper

	char str[6];
	ltoa((uint32_t)c0, str, 10);
	uart_send_string("\n\rc0: ");
	uart_send_string(str);
}

void stepper_move_to_pos(int32_t p, uint8_t mode){

	if (mode == ABS) target_pos = p;
	else if (mode == REL) target_pos += p;

	//discard if position is the same as target
	if (target_pos == current_pos) return;

	// Determine how's the motor moving:
	if (spd == SPEED_HALT) {
		
		if (target_pos > current_pos) drv_dir(CW, &dir);
		else drv_dir(CCW, &dir);
			
		drv_set(ENABLE);
		speed_timer_set(ENABLE, (uint16_t)c0);
		pulse();
		compute_c();	// computes the next cn for the next cycle
		spd = SPEED_UP;
		queue_full = FALSE;

	} else {
		// All possible cases of target position vs current position & movement direction:
		//	- target_pos >= current_pos: moving CW - towards the final position - check closeness to target
		//								moving CCW - away from the final position
		//	- target_pos < current_pos: moving CW - away from the final position
		//								moving CCW - towards the final position - check closeness to target
		// 
		if (target_pos >= current_pos) {
			if (dir == CW) {
				if ((target_pos - current_pos) < (int32_t)n) {	// stepper too close to target to stop
					queue_motion(target_pos);
					target_pos += (int32_t)n;
					stepper_stop();
				}
			} else {
				stepper_stop();
				queue_motion(target_pos);
			}
		} else {
			if (dir == CW) {
				stepper_stop();
				queue_motion(target_pos);
			} else {
				if (abs(target_pos - current_pos) < (int32_t)n) {	// stepper too close to target to stop
					queue_motion(target_pos);
					target_pos -= (int32_t)n;
					stepper_stop();
				}
			}
		}
	}
}

void stepper_move_to_pos_block(int32_t p, uint8_t mode) {

	stepper_move_to_pos(p, mode);
	while(spd != SPEED_HALT);
}

void stepper_stop(void) {

	if (spd != SPEED_HALT) {
		if (dir == CW) target_pos = current_pos + (int32_t)n;
		else target_pos = current_pos - (int32_t)n;
	}

	char str[8];
	int32_t a = current_pos;
	int32_t b = (int32_t)n;
	int32_t c = target_pos;

	ltoa(a, str, 10);
	uart_send_string("\n\rpos: ");
	uart_send_string(str);
	ltoa(b, str, 10);
	uart_send_string("\n\rn: ");
	uart_send_string(str);
	ltoa(c, str, 10);
	uart_send_string("\n\rtarget: ");
	uart_send_string(str);
}

//char str[8];
//uint16_t nv[100];
//uint16_t cv[100];
//uint16_t i = 0;

static void compute_c(void){

	int32_t steps_ahead = labs(target_pos - current_pos);

	if (steps_ahead > (int32_t)n) {
		if (cn <= cmin) spd = SPEED_FLAT;
		else spd = SPEED_UP;
	} else {
		spd = SPEED_DOWN;
	}
	
	switch (spd) {

		case SPEED_UP:
			n++;
			cn = cn - (2.0 * cn) / (4.0 * (float)n + 1.0);
			if (cn <= cmin) {
				cn = cmin;
				spd = SPEED_FLAT;
			}
			break;

		case SPEED_FLAT:
			cn = cmin;
			if (steps_ahead <= n) {
				spd = SPEED_DOWN;
				cn = cn - (2.0 * cn) / (4.0 * (float)n * (-1.0) + 1.0);
			}
			break;

		case SPEED_DOWN:
			n = (uint16_t)steps_ahead;
			if (n > 0) {
				cn = cn - (2.0 * cn) / (4.0 * (float)n * (-1.0) + 1.0);
			} else {
				cn = c0;
				speed_timer_set(DISABLE, (uint16_t)c0);	
				spd = SPEED_HALT;
				
				drv_set(DISABLE);

				// debug message
				char str[8];
				ltoa(current_pos, str, 10);
				uart_send_string("\n\rFinal pos: ");
				uart_send_string(str);
				
				// check queue:
				if (queue_full)
					aux_timer_set(ENABLE, 10);	// software ISR to execute queued movement
				
			}
			break;

		default:
			break;
	}

	/*
	if (steps_ahead > (uint32_t)n) {
		if (!maxspeed)
			n++;
		x = (4 * n) + 1;
		y = 2 * cn;
		z = y / x;
		tmp = cn - z;
		if (steps_ahead != n) {
			if (tmp > cmin) {
				cn = tmp;
			} else {
				cn = cmin;
				maxspeed = TRUE;
			}
		}
		//////////
		if ((n > 100) && (i < 100)){
			nv[i] = n;
			cv[i] = cn;
			i++;
		}
	} else {
		maxspeed = FALSE;
		n = (uint16_t)steps_ahead;
		if (n > 0) {
			x = (4 * n) + 1;
			y = 2 * cn;
			z = y / x;
			cn = cn - z;
		} else {
			cn = c0;
			(*timer_set)(DISABLE, (uint16_t)c0);	
			char str[8];
			ltoa(current_pos, str, 10);
			uart_send_string("\n\rFinal pos: ");
			uart_send_string(str);
			moving = FALSE;
			///////////
			for (i = 0; i < 100; i++) {
				itoa(nv[i], str, 10);
				uart_send_string("\n\rn: ");
				uart_send_string(str);
				itoa(cv[i], str, 10);
				uart_send_string(" cn: ");
				uart_send_string(str);
			}
		}
	}*/
}

//****************************************************************


ISR(TIMER1_COMPA_vect) {
/*
* Speed Timer.
*/
	pulse();
	// set the new timing delay (based on computation of cn)
	speed_timer_set_raw((uint16_t)cn);
	// compute the timing delay for the next cycle
	compute_c();

}

ISR(TIMER0_COMPA_vect) {
/*
* Miscelaneous Timer
* Used to generate "software-like" interrupts
*/
	aux_timer_set(DISABLE, 10);
	stepper_move_to_pos(queue_pos, ABS);
	queue_full = FALSE;
	//PORTB |= 1<<PORTB5;
}