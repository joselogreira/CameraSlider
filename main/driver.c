
/******************************************************************************
*******************	I N C L U D E   D E P E N D E N C I E S	*******************
******************************************************************************/

#include "driver.h"

#include <avr/io.h>
#include <util/delay.h>

/******************************************************************************
*******************	C O N S T A N T S  D E F I N I T I O N S ******************
******************************************************************************/

// DRIVER ports
#define DRV_EN_PORT 	PORTD
#define DRV_MS1_PORT 	PORTC
#define DRV_MS2_PORT	PORTD
#define DRV_MS3_PORT	PORTD
#define DRV_RST_PORT	PORTD
#define DRV_SLEEP_PORT	PORTD
#define DRV_STEP_PORT	PORTB
#define DRV_DIR_PORT	PORTB

#define DRV_EN_PIN 		PORTD7
#define DRV_MS1_PIN 	PORTC5
#define DRV_MS2_PIN		PORTD4
#define DRV_MS3_PIN		PORTD5
#define DRV_RST_PIN		PORTD6
#define DRV_SLEEP_PIN	PORTD7
#define DRV_STEP_PIN	PORTB0
#define DRV_DIR_PIN		PORTB1

/******************************************************************************
******************** F U N C T I O N   P R O T O T Y P E S ********************
******************************************************************************/

void drv_step_mode(uint8_t mode)
{
	switch(mode){

		case MODE_FULL_STEP:
			DRV_MS3_PORT &= ~(1<<DRV_MS3_PIN);
			DRV_MS2_PORT &= ~(1<<DRV_MS2_PIN);
			DRV_MS1_PORT &= ~(1<<DRV_MS1_PIN);
			break;

		case MODE_HALF_STEP:
			DRV_MS3_PORT &= ~(1<<DRV_MS3_PIN);
			DRV_MS2_PORT &= ~(1<<DRV_MS2_PIN);
			DRV_MS1_PORT |= (1<<DRV_MS1_PIN);
			break;

		case MODE_QUARTER_STEP:
			DRV_MS3_PORT &= ~(1<<DRV_MS3_PIN);
			DRV_MS2_PORT |= (1<<DRV_MS2_PIN);
			DRV_MS1_PORT &= ~(1<<DRV_MS1_PIN);
			break;

		case MODE_EIGHTH_STEP:
			DRV_MS3_PORT &= ~(1<<DRV_MS3_PIN);
			DRV_MS2_PORT |= (1<<DRV_MS2_PIN);
			DRV_MS1_PORT |= (1<<DRV_MS1_PIN);
			break;

		case MODE_SIXTEENTH_STEP:
			DRV_MS3_PORT |= (1<<DRV_MS3_PIN);
			DRV_MS2_PORT |= (1<<DRV_MS2_PIN);
			DRV_MS1_PORT |= (1<<DRV_MS1_PIN);
			break;

		default:
			DRV_MS3_PORT &= ~(1<<DRV_MS3_PIN);
			DRV_MS2_PORT &= ~(1<<DRV_MS2_PIN);
			DRV_MS1_PORT &= ~(1<<DRV_MS1_PIN);
			break;
	}
}

void drv_spin_direction(uint8_t dir)
{
	if(dir) {
		slider.spin = CW;
		DRV_DIR_PORT |= (1<<DRV_DIR_PIN);
	} else {
		slider.spin = CCW;
		DRV_DIR_PORT &= ~(1<<DRV_DIR_PIN);
	}
}

void drv_set(uint8_t state)
{
	if (state) {
		DRV_EN_PORT &= ~(1<<DRV_EN_PIN);
	} else {
		DRV_EN_PORT |= (1<<DRV_EN_PIN);
	}
	_delay_us(100);

}

void drv_reset(void)
{
	DRV_RST_PORT &= ~(1<<DRV_RST_PIN);
	_delay_us(5);
	DRV_RST_PORT |= (1<<DRV_RST_PIN);
	_delay_us(95);
}