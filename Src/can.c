#include "stm32f0xx_hal.h"
#include "can.h"
#include "led.h"

CAN_HandleTypeDef hcan;
CAN_FilterTypeDef filter;
uint32_t prescaler;
enum can_bus_state bus_state;

void can_init(void) {
    // default to 125 kbit/s
    prescaler = 48;
    hcan.Instance = CAN;
    bus_state = OFF_BUS;
}

void can_set_filter(uint32_t id, uint32_t mask)
{
    // see page 825 of RM0091 for details on filters
    // set the standard ID part
    filter.FilterIdHigh = (id & 0x7FF) << 5;
    // add the top 5 bits of the extended ID
    filter.FilterIdHigh += (id >> 24) & 0xFFFF;
    // set the low part to the remaining extended ID bits
    filter.FilterIdLow += ((id & 0x1FFFF800) << 3);

    // set the standard ID part
    filter.FilterMaskIdHigh = (mask & 0x7FF) << 5;
    // add the top 5 bits of the extended ID
    filter.FilterMaskIdHigh += (mask >> 24) & 0xFFFF;
    // set the low part to the remaining extended ID bits
    filter.FilterMaskIdLow += ((mask & 0x1FFFF800) << 3);

    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;

    if (bus_state == ON_BUS) {
    	HAL_CAN_ConfigFilter(&hcan, &filter);
    }
}

void can_enable(void)
{
	if (bus_state == OFF_BUS) {
		hcan.Init.Prescaler = prescaler;
		hcan.Init.Mode = CAN_MODE_NORMAL;
		hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
		hcan.Init.TimeSeg1 = CAN_BS1_4TQ;
		hcan.Init.TimeSeg2 = CAN_BS2_3TQ;
		hcan.Init.TimeTriggeredMode = DISABLE;
		hcan.Init.AutoBusOff = DISABLE;
		hcan.Init.AutoWakeUp = DISABLE;
		hcan.Init.AutoRetransmission = DISABLE;
		hcan.Init.ReceiveFifoLocked = DISABLE;
		hcan.Init.TransmitFifoPriority = DISABLE;
		HAL_CAN_Init(&hcan);
		bus_state = ON_BUS;
		can_set_filter(0, 0);
	}
}

void can_disable(void)
{
    if (bus_state == ON_BUS) {
        // do a bxCAN reset (set RESET bit to 1)
        hcan.Instance->MCR |= CAN_MCR_RESET;
        bus_state = OFF_BUS;
    }
    set_green_led(false);
}

void can_set_bitrate(enum can_bitrate bitrate) {
    if (bus_state == ON_BUS) {
        // cannot set bitrate while on bus
        return;
    }

    switch (bitrate) {
    case CAN_BITRATE_10K:
	prescaler = 600;
        break;
    case CAN_BITRATE_20K:
	prescaler = 300;
        break;
    case CAN_BITRATE_50K:
	prescaler = 120;
        break;
    case CAN_BITRATE_100K:
        prescaler = 60;
        break;
    case CAN_BITRATE_125K:
        prescaler = 48;
        break;
    case CAN_BITRATE_250K:
        prescaler = 24;
        break;
    case CAN_BITRATE_500K:
        prescaler = 12;
        break;
    case CAN_BITRATE_750K:
        prescaler = 8;
        break;
    case CAN_BITRATE_1000K:
        prescaler = 6;
        break;
    }
}

void can_set_silent(uint8_t silent) {
    if (bus_state == ON_BUS) {
        // cannot set silent mode while on bus
        return;
    }
    if (silent) {
        hcan.Init.Mode = CAN_MODE_SILENT;
    } else {
        hcan.Init.Mode = CAN_MODE_NORMAL;
    }
}

uint32_t can_tx(CAN_TxHeaderTypeDef *tx_msg, uint8_t data[], uint32_t timeout)
{
    uint32_t status, transmit_mb = 0xFF;
    uint32_t started = HAL_GetTick();
    while (transmit_mb == 0xFF && (HAL_GetTick() - started < timeout)) {
    	status = HAL_CAN_AddTxMessage(&hcan, tx_msg, data, &transmit_mb);
    }

	led_on();
    return status;
}

uint32_t can_rx(CAN_RxHeaderTypeDef *rx_msg, uint8_t data[], uint32_t timeout)
{
    uint32_t status = HAL_ERROR;
    uint32_t started = HAL_GetTick();
	while (status == HAL_ERROR && (HAL_GetTick() - started < timeout)) {
		status = HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, rx_msg, data);
	}
	led_on();
    return status;
}

uint8_t is_can_msg_pending(uint8_t fifo) {
    if (bus_state == OFF_BUS) {
        return 0;
    }
    return (HAL_CAN_GetRxFifoFillLevel(&hcan, fifo) > 0);
}
