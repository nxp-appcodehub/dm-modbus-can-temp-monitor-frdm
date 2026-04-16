/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/dt-bindings/gpio/nxp-kinetis-gpio.h>

#include "relay-5-click.h"

// Definitions ------------------------------
#define RX_THREAD_STACK_SIZE 512
#define RX_THREAD_PRIORITY 2

#define RELAY_STATE_MSG_ID 0x10


typedef struct _can_relay_states_t
{
	uint8_t relay_1;
	uint8_t relay_2;
	uint8_t relay_3;
} can_relay_states_t;
// ------------------------------------------

// Threads ----------------------------------
K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);
struct k_thread rx_thread_data;

void rx_thread(void *arg1, void *arg2, void *arg3);
void poll_state_thread(void *arg1, void *arg2, void *arg3);
// ------------------------------------------

// Peripherals ------------------------------
const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
const struct device *const i2c_dev = DEVICE_DT_GET(DT_NODELABEL(lpi2c3));
const struct gpio_dt_spec rst_gpio_dev = GPIO_DT_SPEC_GET(DT_ALIAS(rstgpio), gpios);
// ------------------------------------------

// Application ------------------------------
static can_relay_states_t *g_can_relay_states = 0;
static uint8_t g_relay_state_update = 0;
// ------------------------------------------

// CAN Reception queue ----------------------
CAN_MSGQ_DEFINE(relay_state_msgq, 2);
// ------------------------------------------



int32_t relay_i2c_transfer_function(relay_i2c_option_t option, uint8_t address, uint8_t subaddress,uint8_t* buff, uint32_t len)
{
	uint32_t retval;
	uint8_t send[2] = {subaddress,buff[0]};
	if(option == k_joystick_i2c_Read)
	{
		retval = i2c_write_read(i2c_dev, address, &subaddress, 1, &buff, len);
		if(retval) 
		{
			printk("I2C read error\r\n");
		}
	}
	else
	{
		retval = i2c_write(i2c_dev, send, (len+1), address);
		if(retval) 
		{
			printk("Error write I2C\r\n");
		}
	}
	return retval;
}

int main(void)
{
	k_tid_t rx_th;
	relay_states_t states;
	int ret;

	printk("-------- Modbus CAN FRDM-MCXA156 --------\n\r");
	// Check if device is initialized --------------------------
	if(!device_is_ready(can_dev)) 
	{
		printf("CAN: Device %s not ready.\n", can_dev->name);
		return -1;
	}
	// ---------------------------------------------------------

	// Set CAN mode --------------------------------------------
	ret = can_set_mode(can_dev, CAN_MODE_NORMAL);
	if(ret != 0)
	{
		printf("Error setting CAN mode [%d]", ret);
		return -1;
	}
	// ---------------------------------------------------------

	// Start CAN -----------------------------------------------
	ret = can_start(can_dev);
	if(ret != 0)
	{
		printf("Error starting CAN controller [%d]", ret);
		return -1;
	}
	// ---------------------------------------------------------
	
	// LPI2C ---------------------------------------------------
    if(!device_is_ready(i2c_dev)) 
	{
        printk("I2C device not ready\r\n");
        return -1;
    }
	// ---------------------------------------------------------

	// GPIO ----------------------------------------------------
	if(!device_is_ready(rst_gpio_dev.port)) 
	{
        printk("Reset GPIO is not ready\r\n");
        return -1;
    }
	gpio_pin_configure_dt(&rst_gpio_dev, GPIO_OUTPUT_INACTIVE | KINETIS_GPIO_DS_ALT);
	// Do reset -------------------------
	gpio_pin_set_dt(&rst_gpio_dev, 1);
	k_msleep(500);
	gpio_pin_set_dt(&rst_gpio_dev, 0);
	k_msleep(500);
	// ----------------------------------
	// ---------------------------------------------------------

	// Reception thread ----------------------------------------
	rx_th = k_thread_create(&rx_thread_data, rx_thread_stack,
				 K_THREAD_STACK_SIZEOF(rx_thread_stack),
				 rx_thread, NULL, NULL, NULL,
				 RX_THREAD_PRIORITY, 0, K_NO_WAIT);
	if(!rx_th)
	{
		printf("Error openning reception thread\r\n");
		return -1;
	}
	// ---------------------------------------------------------

	// Relay 5 click init --------------------------------------
    relay5click_init(relay_i2c_transfer_function);
	
	states.relay_0 = k_relay_off;
	states.relay_1 = k_relay_off;
	states.relay_2 = k_relay_off;
	relay5click_set_states(states);
	// ---------------------------------------------------------

	printf("Init finished\r\n");

	while (1)
	{
		if(g_relay_state_update)
		{
			g_relay_state_update = 0;
			printf("States received: %d,%d,%d\r\n", g_can_relay_states->relay_1, g_can_relay_states->relay_2, g_can_relay_states->relay_3);
			states.relay_0 = (g_can_relay_states->relay_1)? k_relay_on:k_relay_off;
			states.relay_1 = (g_can_relay_states->relay_2)? k_relay_on:k_relay_off;
			states.relay_2 = (g_can_relay_states->relay_3)? k_relay_on:k_relay_off;
			relay5click_set_states(states);
		}
		k_msleep(500);
	}
}


void rx_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	
	struct can_frame frame;
	int filter_id;
	const struct can_filter filter = {
		.flags = CAN_FILTER_IDE,
		.id = RELAY_STATE_MSG_ID,
		.mask = CAN_EXT_ID_MASK
	};

	g_can_relay_states = (can_relay_states_t*)frame.data;
	filter_id = can_add_rx_filter_msgq(can_dev, &relay_state_msgq, &filter);
	printf("Relay state filter id: %d\r\n", filter_id);

	while (1) {
		k_msgq_get(&relay_state_msgq, &frame, K_FOREVER);

		if (IS_ENABLED(CONFIG_CAN_ACCEPT_RTR) && (frame.flags & CAN_FRAME_RTR) != 0U) {
			continue;
		}

		if (frame.dlc != 2U) {
			continue;
		}

		g_relay_state_update = 1;
	}
}