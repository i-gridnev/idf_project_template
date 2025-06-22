#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"

#include "eventbus.h"

#define EVT_QUEUE_SIZE 10

typedef struct {
    StaticQueue_t __q_struct;
    uint8_t __q_buf[EVT_QUEUE_SIZE * sizeof(event_t)];
    QueueHandle_t queue;
} eventbus_t;

static eventbus_t EVENTBUS;

void
eventbus_init(size_t modules_anount) {
    EVENTBUS.queue = xQueueCreateStatic(EVT_QUEUE_SIZE, sizeof(event_t), EVENTBUS.__q_buf, &EVENTBUS.__q_struct);
}