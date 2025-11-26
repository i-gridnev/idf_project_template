/*
 * Helper patch/expander for ESP IDF solution of linked lists (default SLIST macros)
 * The base design is found in <sys/queue.h>
 *  
*/
#ifndef _SLIST_PATCH_H_
#define _SLIST_PATCH_H_

#include <sys/queue.h>

// SLIST find tail macro. Return pointer to tail item or NULL if list is empty
#define SLIST_TAIL(head, field)                                                                                        \
    ({                                                                                                                 \
        __typeof__(SLIST_FIRST(head)) _it, _last = NULL;                                                               \
        if (!SLIST_EMPTY(head)) {                                                                                      \
            SLIST_FOREACH(_it, head, field) { _last = _it; }                                                           \
        }                                                                                                              \
        _last;                                                                                                         \
    })

// SLIST find macro with filter callback.
// Footprint for callback: bool fcn(component_type* item, void* ctx), where component_type should be of list item type
// Return true if filter got triggered and with a pointer to the item in *res_or_tail*
// Return false if filter not triggered and *res_or_tail* NULL for case list is empty or a pointer to a tail item
#define SLIST_GET_WITH_TAIL(head, field, res_or_tail, callback, ctx)                                                   \
    ({                                                                                                                 \
        __typeof__(SLIST_FIRST(head)) _it, _last = NULL;                                                               \
        bool _found = false;                                                                                           \
        if (!SLIST_EMPTY(head)) {                                                                                      \
            SLIST_FOREACH(_it, head, field) {                                                                          \
                _last = _it;                                                                                           \
                if (callback(_it, ctx)) {                                                                              \
                    _found = true;                                                                                     \
                    *(res_or_tail) = _it;                                                                              \
                    break;                                                                                             \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        if (!_found) {                                                                                                 \
            *(res_or_tail) = _last;                                                                                    \
        }                                                                                                              \
        _found;                                                                                                        \
    })

#endif /* _SLIST_PATCH_H_ */