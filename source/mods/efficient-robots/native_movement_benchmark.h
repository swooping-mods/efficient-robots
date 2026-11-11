#ifndef MOON_UNIT_MOVEMENT_BENCHMARK_H
#define MOON_UNIT_MOVEMENT_BENCHMARK_H
/* Private synchronous Move+Gather workload. Native Update cannot interleave
 * this main-thread callback. Reference batches remove our four native hooks;
 * selected batches include their real exception, invalidation and query work.
 * This still excludes the remainder of Update, rendering and network dispatch. */
#define UMB_MAX 128u
#define UMB_TRACE 4096u
typedef struct {SPVec3 point,normal,delta;SUQuat rotation;} UMBPose;
static UMBPose umb_poses[UMB_MAX];static U64 umb_trace[UMB_TRACE];
static PTR umb_normal,umb_move;static unsigned umb_key;
static int (*umb_thread_times)(PTR,U64*,U64*,U64*,U64*);static PTR (*umb_current_thread)(void);
static MoonUnitNearby umb_near[MU_MAX];
static int umb_cpu(U64 *out){
    U64 creation=0,exit=0,kernel=0,user=0;
    if(!umb_thread_times||!umb_current_thread||!umb_thread_times(umb_current_thread(),&creation,&exit,&kernel,&user))return 0;
    *out=kernel+user;return 1;
}
static int umb_problem(const char *stage,unsigned actor,unsigned sweep,U64 expected,U64 actual){
    char line[384],*p=cat(line,"{\"event\":\"native-movement-check\",\"stage\":\"");p=cat(p,stage);p=cat(p,"\",\"actor\":");p=num(p,actor);p=cat(p,",\"sweep\":");p=num(p,sweep);p=cat(p,",\"expected\":");p=num(p,expected);p=cat(p,",\"actual\":");p=num(p,actual);cat(p,"}\n");un_line(line);return 0;
}
static int umb_hooks(int enabled){
    return un_native_hooks_set(enabled);
}
static int umb_rotation(PTR tr,SUQuat *out){
    int ok=1;PTR value=call(su.tr_get_rotation,tr,0,&ok);if(!ok||!value)return 0;su_copy(out,unbox(value),sizeof(*out));return 1;
}
static int umb_restore(unsigned count){
    for(unsigned i=0;i<count;i++){
        PTR robot=su_get(un_actors[i].robot),tr=su_get(un_actors[i].transform);int ok=1;
        void *position[]={&umb_poses[i].point},*rotation[]={&umb_poses[i].rotation};
        call(su.tr_position,tr,position,&ok);call(su.tr_rotation,tr,rotation,&ok);
        if(!ok||!un_set(robot,"groundNormal",&umb_poses[i].normal))return 0;
    }
    un_cache_frame=~(U64)0;su_zero(un_dirty,sizeof(un_dirty));un_dirty_count=0;return 1;
}
static U64 umb_bits(U64 hash,const void *bytes,unsigned count){
    const unsigned char *p=bytes;for(unsigned i=0;i<count;i++)hash=(hash^p[i])*1099511628211ull;return hash;
}
static U64 umb_witness(unsigned i){
    PTR robot=su_get(un_actors[i].robot),tr=su_get(un_actors[i].transform);SUQuat rotation;SPVec3 normal;
    if(!umb_rotation(tr,&rotation))return 0;field_value(robot,umb_normal,&normal);SPVec3 point=un_position(tr);
    U64 hash=1469598103934665603ull;hash=umb_bits(hash,&point,sizeof(point));hash=umb_bits(hash,&rotation,sizeof(rotation));hash=umb_bits(hash,&normal,sizeof(normal));
    PTR list=0,items=0,size=0,version=0,array=0;if(!un_list_fields(robot,&list,&items,&size,&version))return 0;
    int count=0;field_value(list,size,&count);field_value(list,items,&array);
    if(count<0||count>MU_MAX*2||!array||su.array_length(array)<(unsigned)count)return 0;
    hash=umb_bits(hash,&count,sizeof(count));
    for(int n=0;n<count;n++){
        UNMover mover;su_copy(&mover,su.array_address(array,sizeof(mover),(unsigned)n),sizeof(mover));
        /* Padding and List revision are not observable mover values. */
        hash=umb_bits(hash,&mover,18);
    }return hash;
}
static int umb_pass(unsigned count,unsigned sweeps,int selected,int qualify,int compare,U64 *witness){
    for(unsigned sweep=0;sweep<sweeps;sweep++){
        /* One cache preparation per simulated roster sweep, plus every Move
         * invalidation/flush. Both costs stay inside the measured pass. */
        if(selected)un_cache_frame=~(U64)0;
        for(unsigned i=0;i<count;i++){
            PTR robot=su_get(un_actors[i].robot);SPVec3 delta=umb_poses[i].delta;if(sweep&1)delta=su_mul(delta,-1);
            ((void(*)(PTR,PTR))*(PTR*)un_gather)(robot,un_gather);
            ((void(*)(PTR,SPVec3,PTR))*(PTR*)umb_move)(robot,delta,umb_move);
            if(qualify){
                if(selected&&((un_roster_count>=UN_INDEX_MIN||un_sources->count(0))&&!un_dirty[i]))return umb_problem("dirty",i,sweep,1,0);
                if(un_oracle_failures)return umb_problem("oracle",i,sweep,0,un_oracle_failures);
                /* The reference has no mod hooks or SDK contract. The selected
                 * path must also refresh read-only queries between same-frame
                 * native moves, even when the small-roster fast path is empty. */
                if(selected){
                    SPVec3 point=un_position(su_get(un_actors[i].transform));unsigned required=0,found=0;
                    if(un_units->nearby((MoonUnitVector){point.x,point.y,point.z},0,0,0,umb_near,MU_MAX,&required)!=1)return umb_problem("sdk-query",i,sweep,1,0);
                    for(unsigned k=0;k<required;k++)if(umb_near[k].provider==1&&umb_near[k].id==un_actors[i].id){
                        MoonUnitVector value=umb_near[k].position;if(value.x!=point.x||value.y!=point.y||value.z!=point.z)return umb_problem("sdk-position",i,sweep,umb_bits(1,&point,12),umb_bits(1,&value,12));found=1;break;
                    }
                    if(!found)return umb_problem("sdk-identity",i,sweep,un_actors[i].id,0);
                }
                U64 value=umb_witness(i);unsigned at=sweep*count+i;if(!value||at>=UMB_TRACE)return 0;
                if(compare){if(umb_trace[at]!=value)return umb_problem("pose-movers",i,sweep,umb_trace[at],value);}else umb_trace[at]=value;
            }
        }
    }
    if(witness){
        U64 hash=1469598103934665603ull;for(unsigned i=0;i<count;i++){U64 value=umb_witness(i);if(!value)return 0;hash=umb_bits(hash,&value,sizeof(value));}*witness=hash;
    }return 1;
}
static void un_movement_benchmark(void){
    if(!un_test||!un_sources->main_thread()||un_profile_enabled||!un_refresh()||!un_count||un_count>UMB_MAX||!un_counter||!un_frequency){event("native-movement-benchmark-admission-failed");return;}
    umb_move=su_method("MoonCleaners","MoonRobotVacuum","Move","UnityEngine.Vector3");umb_normal=get_field(find_class("MoonCleaners","MoonRobotVacuum"),"groundNormal");
    if(!umb_move||method_token(umb_move)!=100670021||!umb_normal||su.field_offset(umb_normal)!=320){event("native-movement-benchmark-binding-failed");return;}
    unsigned count=un_count,sweeps=32768/(count*count);if(sweeps<2)sweeps=2;if(sweeps>256)sweeps=256;
    umb_thread_times=(int(*)(PTR,U64*,U64*,U64*,U64*))GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetThreadTimes");
    umb_current_thread=(PTR(*)(void))GetProcAddress(GetModuleHandleA("kernel32.dll"),"GetCurrentThread");
    U64 clock=0;if(!umb_cpu(&clock)){event("native-movement-clock-failed");return;}
    for(unsigned i=0;i<count;i++){
        PTR tr=su_get(un_actors[i].transform);umb_poses[i].point=un_position(tr);field_value(su_get(un_actors[i].robot),umb_normal,&umb_poses[i].normal);
        umb_poses[i].delta=su_mul(su_vector(su.tr_forward,tr),.02f);if(!umb_rotation(tr,&umb_poses[i].rotation)){event("native-movement-benchmark-capture-failed");return;}
    }
    int oracle=un_oracle,ok=1;U64 ticks[8]={0},cpu[8]={0},witnesses[8]={0};un_oracle=0;
    /* A full original mover oracle runs in the selected qualification pass.
     * Per-step pose/mover fingerprints also compare the actual Move outputs. */
    for(unsigned selected=0;selected<2&&ok;selected++){
        if(!umb_hooks((int)selected)||!umb_restore(count)){ok=0;break;}
        un_oracle=selected&&un_roster_count>=UN_INDEX_MIN;
        if(!umb_pass(count,sweeps,(int)selected,1,(int)selected,0))ok=0;un_oracle=0;
    }
    for(unsigned batch=0;batch<8&&ok;batch++){
        int selected=(batch&3)==1||(batch&3)==2;if(!umb_hooks(selected)||!umb_restore(count)){ok=0;break;}
        U64 begin=0,end=0,start_cpu=0,end_cpu=0;if(!umb_cpu(&start_cpu)){ok=0;break;}
        un_counter(&begin);ok=umb_pass(count,sweeps,selected,0,0,0);un_counter(&end);ticks[batch]=end-begin;
        if(!umb_cpu(&end_cpu)||end_cpu<=start_cpu){ok=0;break;}cpu[batch]=end_cpu-start_cpu;
        if(ok&&!umb_pass(count,0,selected,0,0,&witnesses[batch]))ok=0;
        if(batch&&witnesses[batch]!=witnesses[0])ok=umb_problem("timed-continuation",batch,0,witnesses[0],witnesses[batch]);
    }
    int restored=umb_restore(count),hooked=umb_hooks(1);un_oracle=oracle;
    if(!restored||!hooked){un_sources->suspend();event("native-movement-benchmark-restore-failed");return;}
    if(!ok){event("native-movement-benchmark-parity-failed");return;}
    for(unsigned batch=0;batch<8;batch++){
        char b[512],*p=cat(b,"{\"event\":\"native-movement-benchmark\",\"optimized\":");p=num(p,(batch&3)==1||(batch&3)==2);p=cat(p,",\"actors\":");p=num(p,count);p=cat(p,",\"sweeps\":");p=num(p,sweeps);p=cat(p,",\"ticks\":");p=num(p,ticks[batch]);p=cat(p,",\"frequency\":");p=num(p,un_frequency);p=cat(p,",\"cpu_ticks\":");p=num(p,cpu[batch]);p=cat(p,",\"cpu_frequency\":10000000,\"witness\":");p=num(p,witnesses[batch]);cat(p,"}\n");un_line(b);
    }
}
#endif
