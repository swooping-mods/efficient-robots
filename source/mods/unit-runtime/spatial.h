#ifndef MOON_UNIT_SPATIAL_H
#define MOON_UNIT_SPATIAL_H
/* Exact, allocation-free broad phase. Stable slots are identities, never groups.
 * Like the fleet runtime, long queries visit occupied cells rather than empty
 * world volume. All callers publish movement/removal before querying. */
#define MU_MAX 2048
#define MU_BUCKETS 4096
#define MU_CELL 4.f
typedef struct {float x,y,z;} MUVec;
typedef struct {int x,y,z;} MUCell;
typedef struct {
    MUVec point;float radius;unsigned id,kind,active;
    int bucket,previous,next;
} MUItem;
typedef struct {MUCell key;int head;unsigned state,count,occupied_slot;} MUBucket;
typedef struct {
    /* One transient cell can be reserved before unlinking a moving unit. */
    MUItem items[MU_MAX];MUBucket buckets[MU_BUCKETS];unsigned occupied_buckets[MU_MAX+1];
    unsigned count,occupied,tombstones;float max_radius;
    unsigned long long publications,cells_tested,candidates_tested,rehashes;
} MUSpatial;
static inline void mu_zero(void *p,unsigned long long n){unsigned char *b=p;while(n--)*b++=0;}
static inline int mu_finite(float x){return x==x&&x>=-1000000.f&&x<=1000000.f;}
static inline int mu_valid(MUVec p){return mu_finite(p.x)&&mu_finite(p.y)&&mu_finite(p.z);}
static inline int mu_floor(float x){int i=(int)x;return i-(x<(float)i);}
static inline MUCell mu_cell(MUVec p){return (MUCell){mu_floor(p.x/MU_CELL),mu_floor(p.y/MU_CELL),mu_floor(p.z/MU_CELL)};}
static inline int mu_same(MUCell a,MUCell b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static inline unsigned mu_hash(MUCell c){return ((unsigned)c.x*73856093u^(unsigned)c.y*19349663u^(unsigned)c.z*83492791u)&(MU_BUCKETS-1);}
static inline float mu_distance2(MUVec a,MUVec b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z;}
static inline int mu_bucket(MUSpatial *s,MUCell key,int create){
    unsigned at=mu_hash(key);int tomb=-1;
    for(unsigned probe=0;probe<MU_BUCKETS;probe++,at=(at+1)&(MU_BUCKETS-1)){
        MUBucket *b=&s->buckets[at];
        if(b->state==1&&mu_same(key,b->key))return (int)at;
        if(b->state==2&&tomb<0)tomb=(int)at;
        if(!b->state){
            if(!create)return -1;if(tomb>=0){at=(unsigned)tomb;s->tombstones--;}
            s->buckets[at]=(MUBucket){key,-1,1,0,s->occupied};s->occupied_buckets[s->occupied++]=at;return (int)at;
        }
    }
    if(create&&tomb>=0){s->tombstones--;s->buckets[tomb]=(MUBucket){key,-1,1,0,s->occupied};s->occupied_buckets[s->occupied++]=(unsigned)tomb;return tomb;}
    return -1;
}
static inline void mu_unlink(MUSpatial *s,unsigned slot){
    MUItem *p=&s->items[slot];MUBucket *b=&s->buckets[p->bucket];
    if(p->previous>=0)s->items[p->previous].next=p->next;else b->head=p->next;
    if(p->next>=0)s->items[p->next].previous=p->previous;
    if(!--b->count){
        b->state=2;s->tombstones++;unsigned moved=s->occupied_buckets[--s->occupied];
        s->occupied_buckets[b->occupied_slot]=moved;s->buckets[moved].occupied_slot=b->occupied_slot;
    }
    p->bucket=p->previous=p->next=-1;
}
static inline void mu_remove(MUSpatial *s,unsigned slot){
    if(!s||slot>=MU_MAX||!s->items[slot].active)return;
    mu_unlink(s,slot);s->items[slot].active=0;s->count--;s->publications++;
    /* A conservative retained maximum remains correct after removal. Reset
     * when empty; shrinking it eagerly would cost a roster scan per death. */
    if(!s->count)s->max_radius=0;
}
/* Bound tombstone debt after long movement histories. This main-thread-only
 * scratch storage is shared by serial index calls, never concurrent queries. */
static inline void mu_rehash(MUSpatial *s){
    static MUBucket saved[MU_BUCKETS];
    for(unsigned k=0;k<s->occupied;k++)saved[k]=s->buckets[s->occupied_buckets[k]];
    s->rehashes++;unsigned occupied=s->occupied;mu_zero(s->buckets,sizeof(s->buckets));s->occupied=s->tombstones=0;
    for(unsigned k=0;k<occupied;k++){
        MUBucket old=saved[k];int bucket=mu_bucket(s,old.key,1);
        s->buckets[bucket].head=old.head;s->buckets[bucket].count=old.count;
        for(int i=old.head;i>=0;i=s->items[i].next)s->items[i].bucket=bucket;
    }
}
static inline int mu_publish(MUSpatial *s,unsigned slot,unsigned id,unsigned kind,MUVec point,float radius){
    if(!s||slot>=MU_MAX||!id||!mu_valid(point)||!mu_finite(radius)||radius<0||radius>1000)return 0;
    if(s->tombstones>MU_BUCKETS/4)mu_rehash(s);
    MUCell cell=mu_cell(point);MUItem *p=&s->items[slot];int old=p->active?p->bucket:-1;
    if(old<0||!mu_same(s->buckets[old].key,cell)){
        int bucket=mu_bucket(s,cell,1);if(bucket<0)return 0;
        if(p->active)mu_unlink(s,slot);else s->count++;
        MUBucket *b=&s->buckets[bucket];int previous=-1,next=b->head;
        while(next>=0&&(unsigned)next<slot){previous=next;next=s->items[next].next;}
        p->bucket=bucket;p->previous=previous;p->next=next;
        if(previous>=0)s->items[previous].next=(int)slot;else b->head=(int)slot;
        if(next>=0)s->items[next].previous=(int)slot;b->count++;
    }
    p->point=point;p->radius=radius;p->id=id;p->kind=kind;p->active=1;
    if(radius>s->max_radius)s->max_radius=radius;s->publications++;return 1;
}
static inline void mu_sort(unsigned *out,unsigned count){
    int ordered=1;for(unsigned i=1;i<count;i++)if(out[i-1]>out[i]){ordered=0;break;}
    if(ordered)return;
    /* Heap sort bounds large-output work without query allocations. */
    for(unsigned root=count/2;root--;){
        unsigned at=root,value=out[at];
        while(at*2+1<count){unsigned child=at*2+1;if(child+1<count&&out[child]<out[child+1])child++;if(value>=out[child])break;out[at]=out[child];at=child;}out[at]=value;
    }
    for(unsigned end=count-1;end;end--){
        unsigned value=out[end];out[end]=out[0];unsigned at=0;
        while(at*2+1<end){unsigned child=at*2+1;if(child+1<end&&out[child]<out[child+1])child++;if(value>=out[child])break;out[at]=out[child];at=child;}out[at]=value;
    }
}
static inline void mu_query_cell(MUSpatial *s,int bucket,MUVec point,float radius,int excluded,unsigned kind_mask,unsigned *out,unsigned *count){
    for(int i=s->buckets[bucket].head;i>=0;i=s->items[i].next){
        MUItem *p=&s->items[i];s->candidates_tested++;
        if(i==excluded||(kind_mask&&!(p->kind&kind_mask)))continue;
        float reach=radius+p->radius;
        if(mu_distance2(point,p->point)<=reach*reach)out[(*count)++]=(unsigned)i;
    }
}
/* The caller supplies MU_MAX entries: queries cannot silently truncate contact.
 * Stable-slot ordering is independent of hash placement or publication order. */
static inline unsigned mu_query(MUSpatial *s,MUVec point,float radius,int excluded,unsigned kind_mask,unsigned out[MU_MAX]){
    if(!s||!out||!s->count||!mu_valid(point)||!mu_finite(radius)||radius<0||radius>1000)return 0;
    float reach=radius+s->max_radius;
    MUCell lo=mu_cell((MUVec){point.x-reach,point.y-reach,point.z-reach});
    MUCell hi=mu_cell((MUVec){point.x+reach,point.y+reach,point.z+reach});
    unsigned count=0;
    double volume=((double)hi.x-lo.x+1)*((double)hi.y-lo.y+1)*((double)hi.z-lo.z+1);
    if(volume>s->occupied){
        for(unsigned k=0;k<s->occupied;k++){
            unsigned i=s->occupied_buckets[k];MUBucket *b=&s->buckets[i];s->cells_tested++;
            if(b->key.x<lo.x||b->key.x>hi.x||b->key.y<lo.y||b->key.y>hi.y||b->key.z<lo.z||b->key.z>hi.z)continue;
            mu_query_cell(s,(int)i,point,radius,excluded,kind_mask,out,&count);
        }
    }else{
        for(int x=lo.x;x<=hi.x;x++)for(int y=lo.y;y<=hi.y;y++)for(int z=lo.z;z<=hi.z;z++){
            s->cells_tested++;int b=mu_bucket(s,(MUCell){x,y,z},0);
            if(b>=0)mu_query_cell(s,b,point,radius,excluded,kind_mask,out,&count);
        }
    }
    mu_sort(out,count);return count;
}
/* Advisory only: direct commands/contact/death invalidate immediately. */
static inline int mu_due(unsigned long long tick,unsigned id,unsigned lane,unsigned interval){
    if(!interval)return 1;unsigned hash=id*2654435761u^(lane*2246822519u);return tick%interval==hash%interval;
}
#endif
