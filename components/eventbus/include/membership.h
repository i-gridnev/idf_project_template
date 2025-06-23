
#ifndef _MEMBERSHIP_H_
#define _MEMBERSHIP_H_

#include <stdio.h>
#include <sys/queue.h>

#include "eventbus.h"

typedef struct group_member {
    ModuleBase* mod;
    SLIST_ENTRY(group_member) next;
} group_member_t;

//////////////////////////////////////////

SLIST_HEAD(group_head, group_member);

typedef struct subscription_member {
    ModuleBase* module;
    SLIST_ENTRY(subscription_member) next;
} subscription_member_t;

SLIST_HEAD(subscription_member_head, subscription_member);

typedef struct subscription_event {
    int event_id;
    struct subscription_member_head sub;
    SLIST_ENTRY(subscription_member) next;
} subscription_event_t;

SLIST_HEAD(subscription_event_head, subscription_event);

//////////////////////////////////////////

#endif /* _MEMBERSHIP_H_ */