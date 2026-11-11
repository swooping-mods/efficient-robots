#ifndef MOON_UNIT_PROFILE_ABI_H
#define MOON_UNIT_PROFILE_ABI_H
/* Private local exception hooks receive the full OS AMD64 CONTEXT. The public
 * shared-hook API copies only its integer prefix and must never use this view.
 * FXSAVE: 32 control bytes + eight 16-byte FP slots precede sixteen XMM slots. */
typedef struct {Context integer;unsigned char fxsave_prefix[160],xmm[16][16];} UNProfileContext;
_Static_assert(sizeof(Context)==256,"pinned AMD64 integer CONTEXT prefix");
_Static_assert(__builtin_offsetof(UNProfileContext,xmm)==416,"AMD64 FXSAVE XMM0 offset");
#ifndef UN_PROFILE_CALL
#define UN_PROFILE_CALL
#endif
static int un_profile_ground_call(PTR address,PTR method,Context *context){
    if(!context->rcx||!context->rdx||!context->r8)return 0;
    SPVec3 point;su_copy(&point,(PTR)context->r8,sizeof(point));
    /* A 12-byte aggregate return uses RCX as hidden result pointer; this shifts
     * self to RDX and the by-value Vector3's indirect storage to R8. */
    SPVec3 result=((SPVec3(UN_PROFILE_CALL *)(PTR,SPVec3,PTR))address)((PTR)context->rdx,point,method);
    su_copy((PTR)context->rcx,&result,sizeof(result));context->rax=context->rcx;return 1;
}
static int un_profile_drive_call(PTR address,PTR method,Context *context){
    if(!(context->flags&8)||!context->rcx||!context->rdx||!context->rsp)return 0;
    SPVec3 point;float a,b;int steer;
    su_copy(&point,(PTR)context->rdx,sizeof(point));
    su_copy(&a,((UNProfileContext*)context)->xmm[2],sizeof(a));
    su_copy(&b,((UNProfileContext*)context)->xmm[3],sizeof(b));
    su_copy(&steer,(const unsigned char*)context->rsp+40,sizeof(steer));
    unsigned char result=((unsigned char(UN_PROFILE_CALL *)(PTR,SPVec3,float,float,int,PTR))address)((PTR)context->rcx,point,a,b,steer,method);
    context->rax=result;return 1;
}
static int un_profile_animate_call(PTR address,PTR method,Context *context){
    if(!(context->flags&8)||!context->rcx)return 0;float dt;
    su_copy(&dt,((UNProfileContext*)context)->xmm[2],sizeof(dt));
    ((void(UN_PROFILE_CALL *)(PTR,unsigned char,float,PTR))address)((PTR)context->rcx,(unsigned char)context->rdx,dt,method);return 1;
}
static int un_profile_track_call(PTR address,PTR method,Context *context){
    if(!(context->flags&8)||!context->rcx)return 0;float dt;
    su_copy(&dt,((UNProfileContext*)context)->xmm[1],sizeof(dt));
    ((void(UN_PROFILE_CALL *)(PTR,float,PTR))address)((PTR)context->rcx,dt,method);return 1;
}
#endif
