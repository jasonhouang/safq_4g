#include <rtthread.h>
#include <stdlib.h>
#include <sys/time.h>
//#include <signal.h>
#include "string.h"
//#include "mqtt_service.h"
#include "linkkit_solo.h"
#include "ble_detect.h"
#include "ntp.h"
#include "parse_cmd_line.h"

#define BLE_UART_NAME       "uart6"

#define UART_RX_MAX_DATA_LEN                128
#define PARAM_SIZE                          UART_RX_MAX_DATA_LEN - 3

#define CMD_TS  (((uint16_t)'T'<<8)|'S') //Time Stamp
#define CMD_SN  (((uint16_t)'S'<<8)|'N') //Serial Number
#define CMD_RD  (((uint16_t)'R'<<8)|'D') //Request Date

#define FIFO_SIZE                           512


/* 邮箱控制块 */
struct rt_mailbox mb;
static rt_timer_t timer;
static rt_bool_t is_sync_ntp_time = RT_FALSE;

static staple_time_t staple_time;
static device_state_t device_state;
static mail_box_t mail_box[FIFO_SIZE];
static rt_uint8_t head = 0;

/* 用于放邮件的内存池 */
static rt_ubase_t mb_pool[FIFO_SIZE];

static struct rt_semaphore rx_sem;
static struct rt_semaphore sync_sem;
static rt_device_t serial;
static rt_thread_t thread_link;

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
    rt_sem_release(&sync_sem);
}

static rt_err_t uart_input(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&rx_sem);

    return RT_EOK;
}

#if 0
static void send_mail(rt_ubase_t *mail)
{
    if (RT_EOK == rt_mb_send(&mb, (rt_ubase_t)&mail_box[head]))
    {
        rt_kprintf("send mail: type = %d, sn = %lu, time = %lu\n", mail_box[head].type, 
                mail_box[head].staple_time.sn, mail_box[head].staple_time.time);
        head ++;
        head %= FIFO_SIZE;
    }
    else
    {
        rt_kprintf("send mail fail, mail full\n");
    }
}
#endif

static void device_state_send_connected(rt_bool_t is_connected)
{
    device_state_t device_state;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    if (is_connected)
    {
        device_state.state = 1;
    }
    else
    {
        device_state.state = 0;
    }

    device_state.time = tv.tv_sec;

    memset((void *)&mail_box[head], 0x0, sizeof(mail_box_t));
    mail_box[head].type = MAIL_TYPE_DEVICE_STATE;
    memcpy((void *)&(mail_box[head].device_state), (void *)&device_state, sizeof(device_state_t)); 

    if (RT_EOK == rt_mb_send(&mb, (rt_ubase_t)&mail_box[head]))
    {
        rt_kprintf("send mail: type = %d, state = %lu, time = %lu\n", mail_box[head].type, 
                mail_box[head].device_state.state, mail_box[head].device_state.time);
        head ++;
        head %= FIFO_SIZE;
    }
    else
    {
        rt_kprintf("send mail fail, mail full\n");
    }
}

static void sync_ble_thread_entry(void *parameter)
{
    time_t time;
    struct timeval tv;
    char update_time_cmd[30];
    char time_str[11];

    rt_sem_init(&sync_sem, "sync_sem", 0, RT_IPC_FLAG_FIFO);
    rt_sem_release(&sync_sem);

    /* 创建定时器  周期定时器 */
    timer = rt_timer_create("timer", timeout,
            RT_NULL, 360000,/*60000,*/
            RT_TIMER_FLAG_PERIODIC);
    /* 启动定时器 */
    if (timer != RT_NULL)
        rt_timer_start(timer);

    while (1)
    {
        rt_sem_take(&sync_sem, RT_WAITING_FOREVER);

        if (is_sync_ntp_time == RT_FALSE)
        {
            time = ntp_sync_to_rtc(NULL);
            rt_kprintf("ntp_get_time: %lu\n", time); 
            if (time > 0)
            {
                is_sync_ntp_time = RT_TRUE;
            }
        }
        else
        {
            time = ntp_get_local_time(NULL);
            time -= NETUTILS_NTP_TIMEZONE * 3600;
            //gettimeofday(&tv, NULL);
            //time = tv.tv_sec - 3600 * 8;
            rt_kprintf("get_utc_times: %lu\n", time); 
        }

        if (time > 0)
        {
            memset(update_time_cmd, 0x0, 30);
            memset(time_str, 0x0, 11);
            rt_sprintf(time_str, "%lu", time);
            //rt_sprintf(update_time_cmd, "DA %lu\r\n", time);
            rt_uint32_t crc32 = crc32_compute((uint8_t const *)time_str, 10, NULL); 
            //rt_kprintf("crc32 = 0x%04X(%lu)\n", crc32, crc32);
            rt_sprintf(update_time_cmd, "DA %s 0x%X\r\n", time_str, crc32);
            rt_kprintf("uart_write: %s\n", update_time_cmd); 
            rt_device_write(serial, 0, &update_time_cmd[0], strlen(update_time_cmd));
        }
    }
}
#define ULONG_STR_SIZE  11
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
    char cmdname[15] = {0};
    char ulong_str[ULONG_STR_SIZE];
    char *ack_ok = "OK\r\n";
    rt_uint8_t para_len = 0;

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
        //rt_device_write(serial, 0, &ch, 1);

        if (!has_detected)
        {
#if 0
            if (ch == '$')
            {
                index = 0;
                //cmd_buff[index++] = ch;
                //rt_kprintf("has_detected\r\n");
            }
#endif
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

        rt_kprintf("uart_read: %s\n", &cmd_buff[0]);
#if 0
        for (int i = 0; i < index; i++)
        {
            rt_kprintf("%02x ", cmd_buff[i]);
        }
        rt_kprintf("\r\n");
#endif

        if (cmd_buff[0] == '#' || strlen(cmd_buff) == 0)
        {
            //rt_kprintf("go to exit, decteced\n");
            goto __exit;
        }

        covert_line(cmd_buff, cmdname);
        rt_kprintf("cmd_body: %s, cmd_name: %s\n", cmd_buff, cmdname);
        if (strcmp(cmdname, "STAPLE") == 0)
        {
            memset((void *)&staple_time, 0x0, sizeof(staple_time_t));
            para_len = 0;
            memset(ulong_str, 0x0, ULONG_STR_SIZE);
            if (find_command_para(cmd_buff, 'T', ulong_str, 0) != -1)
            {
                staple_time.time = atol(ulong_str);
                //rt_kprintf("staple.time = %lu\n", staple_time.time);
                para_len ++;
            }

            memset(ulong_str, 0x0, ULONG_STR_SIZE);
            if (find_command_para(cmd_buff, 'N', ulong_str, 0) != -1)
            {
                staple_time.sn = atol(ulong_str);
                //rt_kprintf("staple.sn = %lu\n", staple_time.sn);
                para_len ++;
            }

#if 0
            if (para_len == 2)
            {
                rt_device_write(serial, 0, ack_ok, strlen(ack_ok));
            }
#endif

            memset((void *)&mail_box[head], 0x0, sizeof(mail_box_t));
            mail_box[head].type = MAIL_TYPE_SAPLE_TIME;
            memcpy((void *)&(mail_box[head].staple_time), (void *)&staple_time, sizeof(staple_time_t)); 

            if (RT_EOK == rt_mb_send(&mb, (rt_ubase_t)&mail_box[head]))
            {
                rt_kprintf("send mail: type = %d, sn = %lu, time = %lu\n", mail_box[head].type, 
                        mail_box[head].staple_time.sn, mail_box[head].staple_time.time);
                head ++;
                head %= FIFO_SIZE;
            }
            else
            {
                rt_kprintf("send mail fail, mail full\n");
            }
        }
        else if (strcmp(cmdname, "CONNECTED") == 0)
        {
            rt_sem_release(&sync_sem);
            device_state_send_connected(RT_TRUE);

            //rt_thread_kill(thread_link, SIGUSR1);
        }
        else if (strcmp(cmdname, "DISCONNECTED") == 0)
        {
            rt_kprintf("device disconnected\n"); 
            device_state_send_connected(RT_FALSE);
            //rt_thread_kill(thread_link, SIGUSR2);
        }
        else
        {
            rt_kprintf("Unkowmn Command\n");
        }
#if 0
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
            case CMD_RD:
                time = ntp_get_time(NULL);
                rt_kprintf("ntp_get_time: %lu\n", time); 
                rt_sprintf(update_time_cmd, "DA %lu\r\n", time);
                rt_kprintf("update_time_cmd: %s\n", update_time_cmd); 
                rt_device_write(serial, 0, &update_time_cmd[0], strlen(update_time_cmd));
                break;
            default:
                rt_kprintf("Unkown Command.\r\n");
                break;
        }
#endif
__exit:
        has_detected = RT_FALSE;
        memset((void *)cmd_buff, 0x0, UART_RX_MAX_DATA_LEN);
        index = 0;
    }
}

#if 0
static void mqtt_thread_entry(void *parameter)
{
    ali_mqtt_init();
}
#endif
#if 0
void link_signal_handler(int sig)
{
    rt_kprintf("link received signal %d\n", sig);
    if (sig == SIGUSR1)
    {
        app_post_connected_event();    
    }
    if (sig == SIGUSR2)
    {
        app_post_disconnected_event();
    }
}
#endif
static void link_thread_entry(void *parameter)
{
    //rt_signal_install(SIGUSR1, link_signal_handler);
    //rt_signal_unmask(SIGUSR1);
    //rt_signal_install(SIGUSR2, link_signal_handler);
    //rt_signal_unmask(SIGUSR2);
    linkkit_solo_main();
}

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
    thread_link = rt_thread_create("link", link_thread_entry, RT_NULL, 4096, 25, 10);
    if (thread_link != RT_NULL)
    {
        rt_thread_startup(thread_link);

        rt_thread_t sync_ble = rt_thread_create("sync_ble", sync_ble_thread_entry, RT_NULL, 2048, 26, 10);
        if (sync_ble != RT_NULL)
        {
            rt_thread_startup(sync_ble);
        }
        else
        {
            ret = RT_ERROR;
        }
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
