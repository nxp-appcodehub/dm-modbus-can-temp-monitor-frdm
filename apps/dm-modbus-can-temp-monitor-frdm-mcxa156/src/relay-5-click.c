/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <relay-5-click.h>

// Map register ----------------------------
#define INPUT_PORT_REG			0x00
#define OUTPUT_PORT_REG			0x01
#define POLARITY_INVERSION_REG	0x02
#define CONFIGURATION_REG		0x03
// -----------------------------------------

static relay_i2c_transfer 	g_i2c_transfer_function = 0;
static uint8_t				g_buff					= 0;

void relay5click_init(relay_i2c_transfer transfer_func)
{
	g_i2c_transfer_function = transfer_func;
	if(g_i2c_transfer_function)
	{
		g_buff = 0x00;
		g_i2c_transfer_function(k_joystick_i2c_Write, RELAY5CLICK_SELECTED_ADDRESS,
								CONFIGURATION_REG, &g_buff, 1);
		g_buff = 0x00;
		g_i2c_transfer_function(k_joystick_i2c_Write, RELAY5CLICK_SELECTED_ADDRESS,
								OUTPUT_PORT_REG, &g_buff, 1);
	}
}

relay_states_t relay5click_get_states()
{
	relay_states_t retval;
	if(g_i2c_transfer_function)
	{
		g_i2c_transfer_function(k_joystick_i2c_Read, RELAY5CLICK_SELECTED_ADDRESS,
							INPUT_PORT_REG, &g_buff, 1);
		retval.relay_0 = ((0x01 & g_buff)!=0);
		retval.relay_1 = ((0x02 & g_buff)!=0);
		retval.relay_2 = ((0x04 & g_buff)!=0);
	}
	return retval;
}

int32_t relay5click_set_states(relay_states_t states)
{
	int32_t retval = -1;
	if(g_i2c_transfer_function)
	{
		g_buff = 0x00 | (states.relay_2 << 2) | (states.relay_1 << 1) | states.relay_0;
		retval = g_i2c_transfer_function(k_joystick_i2c_Write, RELAY5CLICK_SELECTED_ADDRESS,
							OUTPUT_PORT_REG, &g_buff, 1);
	}
	return retval;

}

int32_t relay5click_set_state(relay_t relay,relay_state_t state)
{
	int32_t retval = -1;
	if(g_i2c_transfer_function)
	{
		retval = g_i2c_transfer_function(k_joystick_i2c_Read, RELAY5CLICK_SELECTED_ADDRESS,
							INPUT_PORT_REG, &g_buff, 1);
		if(!retval)
		{
			if(state)
			{
				g_buff |= 1<<relay;
			}
			else
			{
				g_buff &= ~(1<<relay);
			}
			retval = g_i2c_transfer_function(k_joystick_i2c_Write, RELAY5CLICK_SELECTED_ADDRESS,
									OUTPUT_PORT_REG, &g_buff, 1);
		}
	}
	return retval;
}



