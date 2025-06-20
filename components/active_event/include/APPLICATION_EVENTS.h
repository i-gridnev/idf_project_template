/*
 * !!! Important event signature static initialization (code compilation stage)
 * Any event to watch in that application should be set here/
*/

#ifndef _APPLICATION_EVENTS_H_
#define _APPLICATION_EVENTS_H_

/// @brief Event signature
typedef enum {
    // system events
    AEVT_SUBSCRIBE = 0,
    AEVT_UNSUBSCRIBE,
    // vvvvvv application events
    AEVT__________,
    // ^^^^^^
    AEVT_MAX_EVT
} evt_sign_t;

#endif /* _APPLICATION_EVENTS_H_ */