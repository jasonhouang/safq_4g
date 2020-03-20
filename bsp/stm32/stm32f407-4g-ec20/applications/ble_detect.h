#ifndef __BLE_DETECT_H__
#define __BLE_DETECT_H__

typedef struct _staple_time_t {
    uint32_t sn;
    uint32_t time;
} staple_time_t;

typedef struct _device_state_t {
    uint32_t state;
    uint32_t time;
} device_state_t;

typedef enum {
    MAIL_TYPE_SAPLE_TIME    = 0x01,
    MAIL_TYPE_DEVICE_STATE  = 0x02
} mail_type_t;

typedef struct _mail_box_t {
    mail_type_t type;
    union {
        staple_time_t staple_time;
        device_state_t device_state;
    };
} mail_box_t;

int ble_detect(int argc, char *argv[]);


#endif

