#include <rtthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include "string.h"
//#include "mqtt_service.h"
#include "linkkit_solo.h"

#define BLE_UART_NAME       "uart6"

#define UART_RX_MAX_DATA_LEN                128
#define PARAM_SIZE                          UART_RX_MAX_DATA_LEN - 3

#define CMD_TS  (((uint16_t)'T'<<8)|'S') //Time Stamp
#define CMD_SN  (((uint16_t)'S'<<8)|'N') //Serial Number

/* 邮箱控制块 */
struct rt_mailbox mb;
static rt_timer_t timer;
/* 用于放邮件的内存池 */
static rt_uint32_t mb_pool[128];

static struct rt_semaphore rx_sem;
static rt_device_t serial;

static uint32_t uint32_decode(const uint8_t * p_encoded_data)
{
    return ( (((uint32_t)((uint8_t *)p_encoded_data)[0]) << 0)  |
            (((uint32_t)((uint8_t *)p_encoded_data)[1]) << 8)  |
            (((uint32_t)((uint8_t *)p_encoded_data)[2]) << 16) |
            (((uint32_t)((uint8_t *)p_encoded_data)[3]) << 24 ));
}

static uint32_t uint32_big_decode(const uint8_t * p_encoded_data)
{
    return ( (((uint32_t)((uint8_t *)p_encoded_data)[0]) << 24) |
            (((uint32_t)((uint8_t *)p_encoded_data)[1]) << 16) |
            (((uint32_t)((uint8_t *)p_encoded_data)[2]) << 8)  |
            (((uint32_t)((uint8_t *)p_encoded_data)[3]) << 0) );
}

static void timeout(void *parameter)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    rt_mb_send(&mb, (rt_ubase_t)tv.tv_sec);
    rt_kprintf("send staple time: %lu\n", tv.tv_sec);
}


static rt_err_t uart_input(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&rx_sem);

    return RT_EOK;
}

static void serial_thread_entry(void *parameter)
{
    rt_uint8_t cmd_buff[UART_RX_MAX_DATA_LEN];
    rt_uint8_t index = 0;
    rt_int8_t  para[PARAM_SIZE+1];
    rt_bool_t has_detected = RT_FALSE;
    rt_uint16_t cmd_code;
    rt_uint8_t paramlen;
    rt_uint64_t tmp_64;
    rt_uint8_t len;

    char *p;
    char ch;

//    uint8_t test[] = "hello, world";
//    rt_device_write(serial, 0, test, sizeof(test));

    while (1)
    {
        while (rt_device_read(serial, -1, &ch, 1) != 1)
        {
            rt_sem_take(&rx_sem, RT_WAITING_FOREVER);
        }
        //ch = ch + 1;
        rt_device_write(serial, 0, &ch, 1);

        if (!has_detected)
        {
            if (ch == '$')
            {
                index = 0;
                //cmd_buff[index++] = ch;
                //rt_kprintf("has_detected\r\n");
            }
            cmd_buff[index++] = ch;
            if ((ch == '\r') || (ch == '\n') || (index >= UART_RX_MAX_DATA_LEN - 1))
            {
                cmd_buff[index - 1] = '\0';
                has_detected = RT_TRUE;
            }
            else
            {
                continue;
            }
        }

        rt_kprintf("rec: %s\n", cmd_buff);
 
        if (cmd_buff[0] != '$')
        {
            has_detected = RT_FALSE;
            memset(cmd_buff, 0x0, UART_RX_MAX_DATA_LEN);
            index = 0;
            continue;
        }
        len = strlen((char*)cmd_buff);
        if (index > 3)
        {
            paramlen = len - 3 > PARAM_SIZE ? PARAM_SIZE : len - 3;
            memcpy(para, (char*)cmd_buff + 3, paramlen);
            para[paramlen] = 0;
        }
        else
        {
            paramlen = 0;
        }

        cmd_code = cmd_buff[1];
        cmd_code <<= 8;
        cmd_code |= cmd_buff[2];

        switch (cmd_code)
        {
            case CMD_TS:
                para[paramlen] = 0;
                tmp_64 = strtoull((char*)para, &p, 10);
                rt_kprintf("ble timestamp: %d\r\n", (uint32_t)tmp_64);
                //ble_publish((uint32_t)tmp_64);
                //app_post_property_CurrentTime((uint32_t)tmp_64);
                rt_mb_send(&mb, (rt_ubase_t)tmp_64);
                break;
            case CMD_SN:
                //ntp_request();
                rt_kprintf("SN have not resoved\n");
                break;
            default:
                rt_kprintf("Unkown Command.\r\n");
                break;
        }
        has_detected = RT_FALSE;
        memset(cmd_buff, 0x0, UART_RX_MAX_DATA_LEN);
        index = 0;
    }
}

#if 0
static void mqtt_thread_entry(void *parameter)
{
    ali_mqtt_init();
}
#else
static void link_thread_entry(void *parameter)
{
    linkkit_solo_main();
}
#endif

int ble_detect(int argc, char *argv[])
{
    rt_err_t ret = RT_EOK;
    char uart_name[RT_NAME_MAX];

    if (argc == 2)
    {
        rt_strncpy(uart_name, argv[1], RT_NAME_MAX);
    }
    else
    {
        rt_strncpy(uart_name, BLE_UART_NAME, RT_NAME_MAX);
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

    ret = rt_mb_init(&mb,
            "mbt",                      /* 名称是 mbt */
            &mb_pool[0],                /* 邮箱用到的内存池是 mb_pool */
            sizeof(mb_pool) / 4,        /* 邮箱中的邮件数目，因为一封邮件占 4 字节 */
            RT_IPC_FLAG_FIFO);          /* 采用 FIFO 方式进行线程等待 */
    if (ret != RT_EOK)
    {
        rt_kprintf("init mailbox failed.\n");
        return -1;
    }
#if 0 
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
#else
    rt_thread_t thread_link = rt_thread_create("link", link_thread_entry, RT_NULL, 4096, 25, 10);
    if (thread_link != RT_NULL)
    {
        rt_thread_startup(thread_link);

        /* 创建定时器  周期定时器 */
        timer = rt_timer_create("timer", timeout,
                RT_NULL, 10000,
                RT_TIMER_FLAG_PERIODIC);
        /* 启动定时器 */
        if (timer != RT_NULL)
            rt_timer_start(timer);
    }
    else
    {
        ret = RT_ERROR;
        return ret;
    }
#endif

    rt_thread_t thread = rt_thread_create("serial", serial_thread_entry, RT_NULL, 2048, 25, 10);
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
    }

    rt_kprintf("ble detect demo started\n");

    return ret;
}
MSH_CMD_EXPORT(ble_detect, detect ble data);
