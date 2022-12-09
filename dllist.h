struct dllist {
    struct dllist *prev, *next;
};

#define DLLIST_INIT(nm)       { (nm), (nm) }
#define DLLIST_DECL_INIT(nm)  struct dllist nm[1] = { DLLIST_INIT(nm) }

static inline void dllist_init(struct dllist *li)
{
    li->prev = li->next = li;
}


static inline struct dllist *dllist_first(const struct dllist *li)
{
    return (li->next);
}


static inline struct dllist *dllist_last(const struct dllist *li)
{
    return (li->prev);
}


static inline struct dllist *dllist_end(struct dllist *li)
{
    return (li);
}


static inline unsigned dllist_is_empty(const struct dllist *li)
{
    DEBUG_ASSERT((li->prev == li) == (li->next == li));
    
    return (li->next == li);
}

struct dllist *dllist_insert(struct dllist *nd, struct dllist *before);
struct dllist *dllist_erase(struct dllist *nd);



