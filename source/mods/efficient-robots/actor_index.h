#ifndef EFFICIENT_ROBOTS_ACTOR_INDEX_H
#define EFFICIENT_ROBOTS_ACTOR_INDEX_H
/* Only identity lookup is hashed. Native roster slots remain the ordering key
 * for every spatial query; shared framework allocation never changes that order. */
#define UN_ID_BUCKETS 4096u
typedef struct {unsigned identity[UN_ID_BUCKETS],slot[UN_ID_BUCKETS];} UNIdentityIndex;
static inline int un_identity_find(const UNIdentityIndex *index,unsigned identity){
    if(!identity)return -1;
    unsigned at=(identity*2654435761u)&(UN_ID_BUCKETS-1);
    for(unsigned i=0;i<UN_ID_BUCKETS;i++,at=(at+1)&(UN_ID_BUCKETS-1)){
        if(!index->identity[at])return -1;
        if(index->identity[at]==identity)return (int)index->slot[at];
    }
    return -1;
}
static inline int un_identity_put(UNIdentityIndex *index,unsigned identity,unsigned slot){
    if(!identity)return 0;
    unsigned at=(identity*2654435761u)&(UN_ID_BUCKETS-1);
    for(unsigned i=0;i<UN_ID_BUCKETS;i++,at=(at+1)&(UN_ID_BUCKETS-1)){
        if(index->identity[at]==identity)return index->slot[at]==slot;
        if(!index->identity[at]){index->identity[at]=identity;index->slot[at]=slot;return 1;}
    }
    return 0;
}
#endif
