#ifndef MOON_UNIT_FAST_JUMP_H
#define MOON_UNIT_FAST_JUMP_H
/* Pinned AMD64 entry gates, not a general instruction relocator. Only these
 * seven-byte prefixes are accepted. They neither adjust RSP nor change a
 * nonvolatile register, so the gateway has leaf-function unwind semantics.
 * Original-body execution resumes under the game's own unwind table at +7. */
typedef struct {unsigned char original[8],patched[8];} UNJump;
static inline int unj_rel(unsigned char *out,U64 next,U64 target){
    if(next>0x7fffffffffffffffull||target>0x7fffffffffffffffull)return 0;
    long long distance=(long long)target-(long long)next;
    if(distance<(-2147483647ll-1)||distance>2147483647ll)return 0;
    int value=(int)distance;su_copy(out,&value,4);return 1;
}
static inline int unj_build(UNJump *jump,unsigned kind,const unsigned char original[8],U64 entry,unsigned char gateway[32],U64 gateway_address,U64 wrapper){
    const unsigned char gather[8]={0x48,0x8b,0xc4,0x48,0x89,0x48,0x08,0x53};
    const unsigned char move[8]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48};
    const unsigned char *expected=kind==1?gather:kind==3?move:0;
    if(!expected||(entry&7)||!wrapper||entry>0x7ffffffffffff000ull||gateway_address>0x7ffffffffffff000ull)return 0;
    for(unsigned i=0;i<8;i++)if(original[i]!=expected[i])return 0;
    unsigned char forward[4],back[4];
    if(!unj_rel(forward,entry+5,gateway_address)||!unj_rel(back,gateway_address+28,entry+7))return 0;
    su_copy(jump->original,original,8);su_copy(jump->patched,original,8);
    jump->patched[0]=0xe9;su_copy(jump->patched+1,forward,4);jump->patched[5]=jump->patched[6]=0x90;
    for(unsigned i=0;i<32;i++)gateway[i]=0x90;
    /* RIP-indirect tail jump preserves all argument registers and flags. */
    gateway[0]=0xff;gateway[1]=0x25;su_zero(gateway+2,4);su_copy(gateway+6,&wrapper,8);
    su_copy(gateway+16,original,7);gateway[23]=0xe9;su_copy(gateway+24,back,4);return 1;
}
#endif
