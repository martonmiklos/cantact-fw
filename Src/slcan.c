#include "stm32f0xx_hal.h"
#include "can.h"
#include "slcan.h"

static uint32_t current_filter_id = 0;
static uint32_t current_filter_mask = 0;

int8_t slcan_parse_frame(uint8_t *buf, CAN_RxHeaderTypeDef *header, uint8_t data[])
{
    uint8_t i = 0;
    uint8_t id_len, j;
    uint32_t tmp;

    for (j=0; j < SLCAN_MTU; j++) {
        buf[j] = '\0';
    }

    // add character for frame type
    if (header->RTR == CAN_RTR_DATA) {
        buf[i] = 't';
    } else if (header->RTR == CAN_RTR_REMOTE) {
        buf[i] = 'r';
    }

    // assume standard identifier
    id_len = SLCAN_STD_ID_LEN;
    tmp = header->StdId;
    // check if extended
    if (header->IDE == CAN_ID_EXT) {
        // convert first char to upper case for extended frame
        buf[i] -= 32;
        id_len = SLCAN_EXT_ID_LEN;
        tmp = header->ExtId;
    }
    i++;

    // add identifier to buffer
    for(j=id_len; j > 0; j--) {
        // add nybble to buffer
        buf[j] = (tmp & 0xF);
        tmp = tmp >> 4;
        i++;
    }

    // add DLC to buffer
    buf[i++] = header->DLC;

    // add data bytes
    for (j = 0; j < header->DLC; j++) {
        buf[i++] = (data[j] >> 4);
        buf[i++] = (data[j] & 0x0F);
    }

    // convert to ASCII (2nd character to end)
    for (j = 1; j < i; j++) {
        if (buf[j] < 0xA) {
            buf[j] += 0x30;
        } else {
            buf[j] += 0x37;
        }
    }

    // add carrage return (slcan EOL)
    buf[i++] = '\r';

    // return number of bytes in string
    return i;
}

int8_t slcan_parse_str(uint8_t *buf, uint8_t len)
{
    CAN_TxHeaderTypeDef header;
    uint8_t can_data[8];
    uint8_t i;

    // convert from ASCII (2nd character to end)
    for (i = 1; i < len; i++) {
        // lowercase letters
        if(buf[i] >= 'a')
            buf[i] = buf[i] - 'a' + 10;
        // uppercase letters
        else if(buf[i] >= 'A')
            buf[i] = buf[i] - 'A' + 10;
        // numbers
        else
            buf[i] = buf[i] - '0';
    }

    if (buf[0] == 'O') {
        // open channel command
        can_enable();
        return 0;

    } else if (buf[0] == 'C') {
        // close channel command
        can_disable();
        return 0;

    } else if (buf[0] == 'S') {
        // set bitrate command
        switch(buf[1]) {
        case 0:
            can_set_bitrate(CAN_BITRATE_10K);
            break;
        case 1:
            can_set_bitrate(CAN_BITRATE_20K);
            break;
        case 2:
            can_set_bitrate(CAN_BITRATE_50K);
            break;
        case 3:
            can_set_bitrate(CAN_BITRATE_100K);
            break;
        case 4:
            can_set_bitrate(CAN_BITRATE_125K);
            break;
        case 5:
            can_set_bitrate(CAN_BITRATE_250K);
            break;
        case 6:
            can_set_bitrate(CAN_BITRATE_500K);
            break;
        case 7:
            can_set_bitrate(CAN_BITRATE_750K);
            break;
        case 8:
            can_set_bitrate(CAN_BITRATE_1000K);
            break;
        default:
            // invalid setting
            return -1;
        }
        return 0;

    } else if (buf[0] == 'm' || buf[0] == 'M') {
        // set mode command
        if (buf[1] == 1) {
            // mode 1: silent
            can_set_silent(1);
        } else {
            // default to normal mode
            can_set_silent(0);
        }
        return 0;

    } else if (buf[0] == 'F') {
	// set filter command
	uint32_t id = 0;
	for (i = 1; i < len; i++) {
	    id *= 16;
	    id += buf[i];
	}
	current_filter_id = id;
	can_set_filter(current_filter_id, current_filter_mask);

    } else if (buf[0] == 'K') {
	// set mask command
	uint32_t mask = 0;
	for (i = 1; i < len; i++) {
	    mask *= 16;
	    mask += buf[i];
	}
	current_filter_mask = mask;
	can_set_filter(current_filter_id, current_filter_mask);

    } else if (buf[0] == 't' || buf[0] == 'T') {
        // transmit data frame command
        header.RTR = CAN_RTR_DATA;

    } else if (buf[0] == 'r' || buf[0] == 'R') {
        // transmit remote frame command
        header.RTR = CAN_RTR_REMOTE;

    } else {
        // error, unknown command
        return -1;
    }

    if (buf[0] == 't' || buf[0] == 'r') {
        header.IDE = CAN_ID_STD;
    } else if (buf[0] == 'T' || buf[0] == 'R') {
        header.IDE = CAN_ID_EXT;
    } else {
        // error
        return -1;
    }

    header.StdId = 0;
    header.ExtId = 0;
    if (header.IDE == CAN_ID_EXT) {
        uint8_t id_len = SLCAN_EXT_ID_LEN;
        i = 1;
        while (i <= id_len) {
            header.ExtId *= 16;
            header.ExtId += buf[i++];
        }
    }
    else {
        uint8_t id_len = SLCAN_STD_ID_LEN;
        i = 1;
        while (i <= id_len) {
            header.StdId *= 16;
            header.StdId += buf[i++];
        }
    }


    header.DLC = buf[i++];
    if (header.DLC < 0 || header.DLC > 8) {
        return -1;
    }

    uint8_t j;
    for (j = 0; j < header.DLC; j++) {
        can_data[j] = (buf[i] << 4) + buf[i+1];
        i += 2;
    }

    // send the message
    can_tx(&header, can_data, 10);

    return 0;
}
