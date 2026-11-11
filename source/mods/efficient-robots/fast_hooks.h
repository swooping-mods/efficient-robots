#ifndef MOON_UNIT_FAST_HOOKS_H
#define MOON_UNIT_FAST_HOOKS_H
#include "fast_jump.h"
typedef struct {PTR base,allocation;DWORD allocation_protect,pad;U64 size;DWORD state,protect,type,pad2;} UNMemory;
_Static_assert(sizeof(UNMemory)==48&&__builtin_offsetof(UNMemory,size)==24,"Win64 memory region ABI");
typedef struct {Hook *hook;UNJump jump;unsigned char *gateway;} UNFastHook;
static UNFastHook un_fast_hooks[2];static unsigned char *un_fast_page;static int un_fast_ready,un_fast_attempted;
static void un_fast_gather(PTR robot,PTR method_id);
static void un_fast_move(PTR robot,const SPVec3 *delta,PTR method_id);
static int unf_word(unsigned char *address,const unsigned char bytes[8]){
    U64 value=0;su_copy(&value,bytes,8);if(__atomic_load_n((U64*)address,__ATOMIC_ACQUIRE)==value)return 1;
    DWORD old=0,ignored=0;if(!VirtualProtect(address,8,0x40,&old))return 0;
    __atomic_store_n((U64*)address,value,__ATOMIC_SEQ_CST);
    int flushed=FlushInstructionCache(GetCurrentProcess(),address,8),protected=VirtualProtect(address,8,old,&ignored);return flushed&&protected;
}
static unsigned char *unf_allocate(U64 entry){
    PTR kernel=GetModuleHandleA("kernel32.dll");
    U64 (*query)(PTR,UNMemory*,U64)=(U64(*)(PTR,UNMemory*,U64))GetProcAddress(kernel,"VirtualQuery");
    PTR (*allocate)(PTR,U64,DWORD,DWORD)=(PTR(*)(PTR,U64,DWORD,DWORD))GetProcAddress(kernel,"VirtualAlloc");
    if(!query||!allocate)return 0;
    U64 low=entry>0x70000000ull?entry-0x70000000ull:0x10000,high=entry+0x70000000ull,at=low&~0xffffull;
    for(unsigned regions=0;at<high&&regions<65536;regions++){
        UNMemory memory;if(query((PTR)at,&memory,sizeof(memory))!=sizeof(memory)||!memory.size)return 0;
        U64 end=(U64)memory.base+memory.size;if(end<=at)return 0;
        if(memory.state==0x10000){
            U64 candidate=((U64)memory.base+0xffff)&~0xffffull;if(candidate<at)candidate=(at+0xffff)&~0xffffull;
            if(candidate>=low&&candidate+4096<=end&&candidate+4096<high){PTR page=allocate((PTR)candidate,4096,0x3000,4);if(page)return page;}
        }at=end;
    }return 0;
}
static int un_fast_install(void){
    if(un_fast_attempted)return un_fast_ready;
    if(!un_sources->main_thread()||!__atomic_load_n(&un_started,__ATOMIC_ACQUIRE))return 0;
    un_fast_attempted=1;unsigned found=0;U64 started=0,finished=0;if(un_counter)un_counter(&started);
    for(int i=0;i<hook_count;i++)if(hooks[i].id==1||hooks[i].id==3){
        unsigned slot=hooks[i].id==1?0:1;UNFastHook *f=&un_fast_hooks[slot];f->hook=&hooks[i];
        if(((U64)f->hook->address&7)||*f->hook->address!=0xcc)goto unavailable;
        su_copy(f->jump.original,f->hook->address,8);f->jump.original[0]=f->hook->original[0];found++;
    }
    if(found!=2)goto unavailable;
    un_fast_page=unf_allocate((U64)un_fast_hooks[0].hook->address);if(!un_fast_page)goto unavailable;
    for(unsigned i=0;i<2;i++){
        UNFastHook *f=&un_fast_hooks[i];unsigned char original[8];su_copy(original,f->jump.original,8);f->gateway=un_fast_page+i*64;
        if(!unj_build(&f->jump,f->hook->id,original,(U64)f->hook->address,f->gateway,(U64)f->gateway,(U64)(i?(PTR)un_fast_move:(PTR)un_fast_gather)))goto unavailable;
    }
    DWORD old=0;if(!VirtualProtect(un_fast_page,4096,0x20,&old)||!FlushInstructionCache(GetCurrentProcess(),un_fast_page,4096))goto unavailable;
    /* Only the Unity main thread invokes these transform-mutating methods.
     * Installation runs in the player callback after startup publication,
     * while none of their original prologues is executing on that thread. */
    for(unsigned i=0;i<2;i++)if(!unf_word(un_fast_hooks[i].hook->address,un_fast_hooks[i].jump.patched)){
        int restored=1;for(unsigned j=0;j<2;j++){
            unsigned char bytes[8];su_copy(bytes,un_fast_hooks[j].jump.original,8);bytes[0]=0xcc;
            if(!unf_word(un_fast_hooks[j].hook->address,bytes))restored=0;
        }
        if(!restored){un_fast_ready=1;un_sources->suspend();event("native-fast-hook-restore-failed");return 0;}
        goto unavailable;
    }
    un_fast_ready=1;if(un_counter)un_counter(&finished);
    char evidence[256],*p=cat(evidence,"{\"event\":\"native-fast-hooks-ready\",\"page_bytes\":4096,\"setup_ticks\":");p=num(p,finished-started);p=cat(p,",\"frequency\":");p=num(p,un_frequency);cat(p,"}\n");un_line(evidence);return 1;
unavailable:
    if(un_fast_page){
        int (*release)(PTR,U64,DWORD)=(int(*)(PTR,U64,DWORD))GetProcAddress(GetModuleHandleA("kernel32.dll"),"VirtualFree");
        if(release)release(un_fast_page,0,0x8000);un_fast_page=0;
    }
    event("native-fast-hooks-unavailable");return 0;
}
/* Private benchmark admission changes all four owned native entries together.
 * Production activation is one-way; gateways remain resident until process exit. */
static int un_native_hooks_set(int enabled){
    unsigned count=0;int ok=1;
    for(int i=0;i<hook_count;i++)if(hooks[i].id==1||hooks[i].id==3||hooks[i].id==4||hooks[i].id==5){
        Hook *h=&hooks[i];UNFastHook *f=un_fast_ready&&(h->id==1||h->id==3)?&un_fast_hooks[h->id==1?0:1]:0;
        if(f){
            U64 current=__atomic_load_n((U64*)h->address,__ATOMIC_ACQUIRE),original=0,patched=0;
            su_copy(&original,f->jump.original,8);su_copy(&patched,f->jump.patched,8);if(current!=original&&current!=patched)return 0;
        }else if(*h->address!=0xcc&&*h->address!=h->original[0])return 0;count++;
    }
    if(count!=4)return 0;
    for(int i=0;i<hook_count;i++)if(hooks[i].id==1||hooks[i].id==3||hooks[i].id==4||hooks[i].id==5){
        Hook *h=&hooks[i];UNFastHook *f=un_fast_ready&&(h->id==1||h->id==3)?&un_fast_hooks[h->id==1?0:1]:0;
        if(f){if(!unf_word(h->address,enabled?f->jump.patched:f->jump.original))ok=0;}
        else {
            unsigned char target=enabled?0xcc:h->original[0];if(*h->address==target)continue;
            DWORD old=0,ignored=0;if(!VirtualProtect(h->address,1,0x40,&old)){ok=0;continue;}
            *(volatile unsigned char*)h->address=target;
            int flushed=FlushInstructionCache(GetCurrentProcess(),h->address,1),protected=VirtualProtect(h->address,1,old,&ignored);if(!flushed||!protected)ok=0;
        }
    }
    return ok;
}
#endif
