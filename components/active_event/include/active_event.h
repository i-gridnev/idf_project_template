/*
 * Component: Active Event
 * 
 * Used to provide event driven mechanism and distinction access to application variables/memory. 
 * Based on own internal event Queue and a set of handler functions that are invoked inside that queue context. 
 * So all events handling is computed in a separated sequence (no need for mutexes and other protection features).
 * Allows to add Events (statically via 'APPLICATION_EVENTS.h' signature) and subscribe any amount of handler functions 
 * (dynamically at runtime). On Event occurance all handlers will be called sequentially in subscription order. After all
 * handlers returned, the provided data free function will be called (defifined at posting event stage!).
*/

#ifndef _ACTIVE_EVENT_H_
#define _ACTIVE_EVENT_H_

#include <stdbool.h>
#include "APPLICATION_EVENTS.h"
#include "esp_err.h"

/// @brief Generalized single value blueprint with fixed 4 byte size (int). Used for ease of passing event variables
typedef union {
    void* ptr;
    int integer;
    float decimal;

    struct {
        uint8_t p1;
        uint8_t p2;
        uint8_t p3;
        uint8_t p4;
    } param_4byte;

    struct {
        uint8_t index;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } smart_led;
} value_t;

/// @brief callback signature to be called to free data after all handlers
typedef void (*clear_data_f)(value_t* data);

/// @brief evant handler signature to be called on event
typedef esp_err_t (*evt_handler_t)(evt_sign_t sign, value_t data);

/// @brief Init Active Event component
/// @return ESP error
esp_err_t ae_init();

/// @brief Send new event to internal queue to be captured by handlers
/// @param sign[in] event signature
/// @param data[in] event data
/// @param free_fcn[in] pointer to fcn to free data when all handlers finished
/// @return ESP error
esp_err_t ae_post_evt(evt_sign_t sign, value_t data, clear_data_f free_fcn);

/// @brief Send new event to internal queue. Same as 'ae_post_evt()' but can be called inside Interrupt
/// @param sign[in] event signature
/// @param data[in] event data
/// @param free_fcn[in] pointer to fcn to free data when all handlers finished
/// @return boolean for high_task_wakeup Inrerrupt flow
bool ae_post_evt_FromISR(evt_sign_t sign, value_t data, clear_data_f free_fcn);

/// @brief Send new event to internal queue. Same as 'ae_post_evt_FromISR()' but (!) puts event to the very TOP slot to handle (only for super critical actions)
/// @param sign[in] event signature
/// @param data[in] event data
/// @param free_fcn[in] pointer to fcn to free data when all handlers finished
/// @return boolean for high_task_wakeup Inrerrupt flow
bool ae_post_evt_CRITICAL_FromISR(evt_sign_t sign, value_t data, clear_data_f free_fcn);

/// @brief Subscribe to event
/// @param evt_sign[in] event signature
/// @param handler[in] handler callback
/// @return ESP error
esp_err_t ae_subscribe(evt_sign_t evt_sign, evt_handler_t handler);

/// @brief Unsubscribe from event.(!!Prototype!!) todo: further development and designing
/// @param evt_sign[in] event signature
/// @param handler[in] handler callback
/// @return ESP error
esp_err_t ae_unsubscribe(evt_sign_t evt_sign, evt_handler_t handler);

/// @brief Default free fcn. Used with static (not allocated) data
/// @param data[in] event data pointer
void ae_free_pointer(value_t* data);
#endif /* _ACTIVE_EVENT_H_ */