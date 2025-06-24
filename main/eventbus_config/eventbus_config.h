#ifndef _EVENTBUS_CONFIG_H_
#define _EVENTBUS_CONFIG_H_

#define EVT_QUEUE_SIZE 20
#define EVT_TASK_PTIORITY 5
#define EVT_TASK_CORE 1

typedef enum {
    MODULE_1,
    MODULE_2,
    MODULE_3,
    MODULE_4,
    MODULE_5,
    MODULES_MAX,
} module_id;

#endif /* _EVENTBUS_CONFIG_H_ */
