#ifndef __LINKKIT_SOLO_H__
#define __LINKKIT_SOLO_H__

#include <rtthread.h>

void app_post_property_CurrentTime(uint32_t value);
int linkkit_solo_main(void);

extern struct rt_mailbox mb;

#endif

