#ifndef __BLE_DETECT_H__
#define __BLE_DETECT_H__

typedef struct _staple_time_t {
    uint32_t sn;
    uint32_t time;
} staple_time_t;

int ble_detect(int argc, char *argv[]);


#endif

