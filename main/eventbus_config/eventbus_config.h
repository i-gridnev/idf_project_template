#ifndef _EVENTBUS_CONFIG_H_
#define _EVENTBUS_CONFIG_H_

#define EVT_QUEUE_SIZE 20

typedef enum {
    MODULE_1,
    MODULE_2,
    MODULE_3,
    MODULE_4,
    MODULE_5,
    MODULES_MAX,
} module_id;

typedef enum {
    GROUP_1,
    GROUP_2,
    GROUP_3,
    GROUP_4,
    GROUPS_MAX
} group_id;

#endif /* _EVENTBUS_CONFIG_H_ */
