#ifndef __MQTT_SERVICE_H__
#define __MQTT_SERVICE_H__


void ali_mqtt_init(void);
void oid_publish(uint32_t oid_code);
void ble_publish(uint32_t time_stamp);
void ntp_request(void);

#endif

