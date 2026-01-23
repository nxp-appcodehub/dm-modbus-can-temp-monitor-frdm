/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zephyr/console/console.h>


#include <zephyr/device.h>
#include "fsl_clock.h"

// Definitions ------------------------------
#define INPUT_THRESHOLD_THREAD_STACK_SIZE 256
#define INPUT_THRESHOLD_THREAD_PRIORITY 2

#define STATE_RELAY_MSG_ID 0x10

#define MODBUS_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_modbus_serial)
#define TEMP_SENSOR_ID		0x03
#define DISPLAY_ID			0x01

#define DEFAULT_THRESHOLD 25.0
// ------------------------------------------

// Threads ----------------------------------
K_THREAD_STACK_DEFINE(input_threshold_stack, INPUT_THRESHOLD_THREAD_STACK_SIZE);

struct k_thread input_threshold_thread_data;

void input_threshold_thread(void *arg1, void *arg2, void *arg3);
// ------------------------------------------

// Modbus -----------------------------------
LOG_MODULE_REGISTER(mbc_sample, LOG_LEVEL_INF);

static int client_iface;
const static struct modbus_iface_param client_param = {
	.mode = MODBUS_MODE_RTU,
	.rx_timeout = 50000,
	.serial = {
		.baud = 9600,
		.parity = UART_CFG_PARITY_NONE,
	},
};

static int init_modbus(void);
// ------------------------------------------

// Peripherals ------------------------------
const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
// ------------------------------------------

// App variables ----------------------------
static float g_threshold = DEFAULT_THRESHOLD;
// ------------------------------------------



int main(void)
{
	k_tid_t get_state_tid;
	int32_t stat;
	uint16_t buff[2];
	float temp;

	struct can_frame state_relay_frame = {
		.flags = CAN_FRAME_IDE,
		.id = STATE_RELAY_MSG_ID,
		.dlc = 2
	};

	printk("-------- Modbus CAN FRDM-MCXE31B --------\n\r");

	// Modbus init ---------------------------------------------
	if (init_modbus()) {
		LOG_ERR("Modbus RTU client initialization failed");
		return 0;
	}
	printk("Modbus started, waiting devices ...\n\r");
	k_msleep(2000);
	// ---------------------------------------------------------

	// Set decimal point in LDC --------------------------------
	printk("Setting decimal point of display\n\r");
	modbus_write_holding_reg(client_iface, DISPLAY_ID, 0x0001, 0x02);
	k_msleep(200);
	// ---------------------------------------------------------

	// Attach clocks and reinit FlexCAN ------------------------
    CLOCK_SetClkDiv(kCLOCK_DivFlexcan012PeClk, 1U);
    CLOCK_AttachClk(kAIPS_PLAT_CLK_to_FLEXCAN012_PE);
	can_dev->state->initialized = false;
	can_dev->state->init_res = 0;
	z_impl_device_init(can_dev);
	// ---------------------------------------------------------

	// Check if device is initialized --------------------------
	if (!device_is_ready(can_dev)) {
		printf("CAN: Device %s not ready.\n", can_dev->name);
		return 0;
	}
	// ---------------------------------------------------------

	// Set CAN mode --------------------------------------------
	stat = can_set_mode(can_dev, CAN_MODE_NORMAL);
	if (stat != 0) {
		printf("Error setting CAN mode [%d]", stat);
		return 0;
	}
	// ---------------------------------------------------------

	// Start CAN -----------------------------------------------
	stat = can_start(can_dev);
	if (stat != 0) {
		printf("Error starting CAN controller [%d]", stat);
		return 0;
	}
	// ---------------------------------------------------------

	// For threshold input -------------------------------------
	console_init(); 
	// ---------------------------------------------------------

	// Input threshold thread ----------------------------------
	get_state_tid = k_thread_create(&input_threshold_thread_data,
					input_threshold_stack,
					K_THREAD_STACK_SIZEOF(input_threshold_stack),
					input_threshold_thread, NULL, NULL, NULL,
					INPUT_THRESHOLD_THREAD_PRIORITY, 0,
					K_NO_WAIT);
	if (!get_state_tid) {
		printf("ERROR spawning poll_state_thread\n");
	}
	// ---------------------------------------------------------

	printf("Finished init.\n");

	while (1)
	{
		stat = modbus_read_holding_regs(client_iface, TEMP_SENSOR_ID, 0x0000, buff, 2);
		k_msleep(200);
		if(!stat)
		{
			temp = ((float)buff[0])/((float)10);
			printf("Temperature: %.1f\n\r", (double)temp);
			modbus_write_holding_reg(client_iface, DISPLAY_ID, 0x0000, (uint16_t)(temp*100));
			*((uint16_t*)(state_relay_frame.data)) = (temp > g_threshold)? 1:0;
			can_send(can_dev, &state_relay_frame, K_MSEC(100), NULL, NULL);
		}
		else
		{
			printf("Error reading temperature sensor\n\r");
			modbus_write_holding_reg(client_iface, DISPLAY_ID, 0x0000, 0000);
			*((uint16_t*)(state_relay_frame.data)) = 0;
			can_send(can_dev, &state_relay_frame, K_MSEC(100), NULL, NULL);
		}
		k_msleep(2000);
	}
}

void input_threshold_thread(void *arg1, void *arg2, void *arg3)
{
	uint8_t index;
	char character;
	char buff[5];

	index = 0;
	while(1)
	{
		character = console_getchar();

		if(index >= 5)
		{
			printf("\n\rInvalid data\n\r");
			index = 0;
		}
		else
		{
			if(character == '\n' || character == '\r')
			{
				if(sscanf(buff, "%f", &g_threshold) > 0)
				{
					printf("New Threshold: %.1f\n\r", (double)g_threshold);
				}
				else
				{
					printf("Threshold: %.1f\n\r", (double)g_threshold);
				}
				index = 0;
			}
			else
			{
				buff[index++] = character;
				buff[index] = 0;
				printf("Set threshold: %s\n\r", buff);
			}
		}
		k_msleep(100);
	}
}

static int init_modbus(void)
{
	const char iface_name[] = {DEVICE_DT_NAME(MODBUS_NODE)};

	client_iface = modbus_iface_get_by_name(iface_name);

	return modbus_init_client(client_iface, client_param);
}