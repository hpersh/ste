
struct ste_mesg_pool_cfg {
    unsigned char *addr;
    size_t        size;
};

extern const struct ste_mesg_pool_cfg ste_mesg_pool_cfg[];

enum {
    STE_MESG_NUM_POOLS = 1,
    STE_MESG_NUM_MBOXES = 16
};



