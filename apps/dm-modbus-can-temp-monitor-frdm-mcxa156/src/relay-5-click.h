/*
 * Copyright 2024, 2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RELAY_5_CLICK_H_
#define RELAY_5_CLICK_H_

#include <stdint.h>
#include <stdbool.h>

#define RELAY5CLICK_ADDR0	0x70
#define RELAY5CLICK_ADDR1	0x71
#define RELAY5CLICK_ADDR2	0x72
#define RELAY5CLICK_ADDR3	0x73

#define RELAY5CLICK_SELECTED_ADDRESS RELAY5CLICK_ADDR0

typedef enum _relay_i2c_option_t
{
	k_joystick_i2c_Read,
	k_joystick_i2c_Write
}relay_i2c_option_t;

typedef enum _relay_t
{
	k_relay_2,
	k_relay_1,
	k_relay_0
} relay_t;


typedef enum _relay_state_t
{
	k_relay_off,
	k_relay_on
} relay_state_t;

typedef struct _relay_states_t
{
	relay_state_t relay_0;
	relay_state_t relay_1;
	relay_state_t relay_2;
}relay_states_t;

typedef int32_t(*relay_i2c_transfer)(relay_i2c_option_t option, uint8_t address, uint8_t subaddress,uint8_t* buff, uint32_t len);

void relay5click_init(relay_i2c_transfer transfer_func);

relay_states_t relay5click_get_states();

int32_t relay5click_set_states(relay_states_t states);

int32_t relay5click_set_state(relay_t relay,relay_state_t state);

#endif /* RELAY_5_CLICK_H_ */
