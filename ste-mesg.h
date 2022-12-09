

#include "dllist.h"

typedef void *ste_mesg_data_t;

int ste_mesg_init(void);
int ste_mesg_pool_init(unsigned pool_id, unsigned char *addr, size_t size);
int ste_mesg_mbox_init(unsigned mbox_id, unsigned task_id, unsigned signal);

enum {
    STE_MESG_SEND_FLAG_NOWAIT = 1 << 0
};

int ste_mesg_send(unsigned mbox_id, unsigned pool_id, ste_mesg_t mesg, unsigned flags);
int ste_mesg_recv(unsigned mbox_id, ste_mesg_t *mesg);
    
enum {
    STE_ERR_MESG_POOL_EMPTY = -16
};
