#include <rtthread.h>
#include "string.h"
#include "mqtt_service.h"

#define OID_UART_NAME       "uart6"

static struct rt_semaphore rx_sem;
static rt_device_t serial;

static rt_err_t uart_input(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&rx_sem);

    return RT_EOK;
}

static void serial_thread_entry(void *parameter)
{
    rt_uint8_t cmd_buff[30];
    rt_uint8_t index = 0;
    rt_bool_t has_detected = RT_FALSE;

    char ch;

    while (1)
    {
        while (rt_device_read(serial, -1, &ch, 1) != 1)
        {
            rt_sem_take(&rx_sem, RT_WAITING_FOREVER);
        }
        //ch = ch + 1;
        //rt_device_write(serial, 0, &ch, 1);

        if (!has_detected)
        {
            if (ch == 0xFF)
            {
                index = 0;
                cmd_buff[index++] = ch;
                has_detected = RT_TRUE;
                //rt_kprintf("has_detected\r\n");
            }
            continue;
        }

        cmd_buff[index++] = ch;

        if ((index == cmd_buff[1] + 2) || index == 7)
        {
            uint8_t checksum = 0;
            for (int i = 0; i < index - 1; i++)
            {
                checksum += cmd_buff[i];
            }
            if (checksum + 4 == cmd_buff[index - 1])
            {
                rt_kprintf("oid code: 0x%04x\r\n", *(uint32_t *)&cmd_buff[2]);
                oid_publish(*(uint32_t *)&cmd_buff[2]);
            }
            else
            {
                rt_kprintf("checksum 0x%x error\r\n", checksum + 4);
            }
            has_detected = RT_FALSE;
            //rt_device_write(serial, 0, &cmd_buff[0], 7);
            memset(cmd_buff, 0x0, 30);
        }
    }
}

static void mqtt_thread_entry(void *parameter)
{
    ali_mqtt_init();
}

static int oid_detect(int argc, char *argv[])
{
    rt_err_t ret = RT_EOK;
    char uart_name[RT_NAME_MAX];

    if (argc == 2)
    {
        rt_strncpy(uart_name, argv[1], RT_NAME_MAX);
    }
    else
    {
        rt_strncpy(uart_name, OID_UART_NAME, RT_NAME_MAX);
    }

    serial = rt_device_find(uart_name);
    if (!serial)
    {
        rt_kprintf("find %s failed!\n", uart_name);
        return RT_ERROR;
    }

    rt_sem_init(&rx_sem, "rx_sem", 0, RT_IPC_FLAG_FIFO);

    rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);

    rt_device_set_rx_indicate(serial, uart_input);

    rt_thread_t thread_mqtt = rt_thread_create("mqtt", mqtt_thread_entry, RT_NULL, 3072, 25, 10);
    if (thread_mqtt != RT_NULL)
    {
        rt_thread_startup(thread_mqtt);
    }
    else
    {
        ret = RT_ERROR;
        return ret;
    }

    rt_thread_t thread = rt_thread_create("serial", serial_thread_entry, RT_NULL, 2048, 25, 10);
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
    }

    return ret;
}
MSH_CMD_EXPORT(oid_detect, detect oid code);
