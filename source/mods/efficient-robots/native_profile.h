#ifndef MOON_UNIT_NATIVE_PROFILE_H
#define MOON_UNIT_NATIVE_PROFILE_H
/* Attribution only: private-test hooks add invocation/exception/timer overhead.
 * Update is inclusive of its other phases. Never use these as clean FPS data. */
static PTR un_profile_methods[3];
static int un_profile_suite,un_profile_ground_checked;
#include "profile_abi.h"
static unsigned un_profile_enabled,un_profile_inside[3],un_profile_key;
static U64 un_profile_ticks[4],un_profile_calls[4],un_profile_begin;
static int un_profile_before(unsigned index,Context *context){
    if(!un_profile_enabled||!un_sources->main_thread()||un_profile_inside[index])return 0;
    U64 begin=0,end=0;un_counter(&begin);un_profile_inside[index]=1;
    PTR method=un_profile_methods[index];int valid=1;
    if(un_profile_suite==1&&index==1){
        valid=un_profile_ground_call(*(PTR*)method,method,context);
        if(valid&&!un_profile_ground_checked){
            SPVec3 point,result;su_copy(&point,(PTR)context->r8,sizeof(point));su_copy(&result,(PTR)context->rcx,sizeof(result));
            int ok=1;SPVec3 expected=un_vector_argument(method,(PTR)context->rdx,point,&ok);un_profile_ground_checked=1;
            if(!ok||result.x!=expected.x||result.y!=expected.y||result.z!=expected.z)event("native-profile-ground-abi-failed");else event("native-profile-ground-abi-checked");
        }
    }else if(un_profile_suite==1&&index==2)valid=un_profile_drive_call(*(PTR*)method,method,context);
    else if(un_profile_suite==2&&index==1)valid=un_profile_animate_call(*(PTR*)method,method,context);
    else if(un_profile_suite==2&&index==2)valid=un_profile_track_call(*(PTR*)method,method,context);
    else ((void(*)(PTR,PTR))*(PTR*)method)((PTR)context->rcx,method);
    if(!valid)event("native-profile-context-abi-failed");
    un_profile_inside[index]=0;un_counter(&end);un_profile_ticks[index]+=end-begin;un_profile_calls[index]++;return valid;
}
static void un_profile_control(unsigned pressed){
    if(pressed&&!un_profile_key){
        un_profile_enabled=!un_profile_enabled;
        if(un_profile_enabled){un_oracle=0;su_zero(un_profile_calls,sizeof(un_profile_calls));su_zero(un_profile_ticks,sizeof(un_profile_ticks));un_counter(&un_profile_begin);event("native-profile-start");}
        else {un_oracle=un_test;event("native-profile-stop");}
    }
    un_profile_key=pressed;if(!un_profile_enabled)return;U64 now=0;un_counter(&now);if(now-un_profile_begin<un_frequency*5)return;
    const char *names[]={"Update",un_profile_suite==2?"Animate":un_profile_suite?"Ground":"Decide",un_profile_suite==2?"Track":un_profile_suite?"Drive":"Hoses","GatherMovers"};char b[1024],*p=cat(b,"{\"event\":\"native-phase-profile\",\"suite\":\"");p=cat(p,un_profile_suite==2?"presentation":un_profile_suite?"movement":"overview");p=cat(p,"\",\"frequency\":");p=num(p,un_frequency);p=cat(p,",\"window_ticks\":");p=num(p,now-un_profile_begin);p=cat(p,",\"actors\":");p=num(p,un_roster_count);p=cat(p,",\"phases\":[");
    for(unsigned i=0;i<4;i++){if(i)*p++=',';p=cat(p,"{\"name\":\"");p=cat(p,names[i]);p=cat(p,"\",\"calls\":");p=num(p,un_profile_calls[i]);p=cat(p,",\"ticks\":");p=num(p,un_profile_ticks[i]);p=cat(p,"}");}
    cat(p,"]}\n");un_line(b);su_zero(un_profile_calls,sizeof(un_profile_calls));su_zero(un_profile_ticks,sizeof(un_profile_ticks));un_profile_begin=now;
}
static int un_profile_bind(void){
    char suite[32];unsigned n=GetEnvironmentVariableA("MOONLAB_UNIT_PROFILE_SUITE",suite,sizeof(suite));
    if(n){if(n>=sizeof(suite)||(!su_eq(suite,"movement")&&!su_eq(suite,"overview")&&!su_eq(suite,"presentation")))return 0;un_profile_suite=su_eq(suite,"presentation")?2:su_eq(suite,"movement");}
    un_profile_methods[0]=un_update;
    un_profile_methods[1]=su_method("MoonCleaners","MoonRobotVacuum",un_profile_suite==2?"Animate":un_profile_suite?"Ground":"Decide",un_profile_suite==2?"System.Boolean;System.Single":un_profile_suite?"UnityEngine.Vector3":"");
    un_profile_methods[2]=un_profile_suite==2?su_method("MoonCleaners","MoonRobotVacuum","Track","System.Single"):un_profile_suite?su_method("MoonCleaners","MoonRobotVacuum","Drive","UnityEngine.Vector3;System.Single;System.Single;MoonCleaners.MoonRobotVacuum.Steer"):su_method("MoonCleaners","MoonJoonasRig","Hoses","");
    const unsigned tokens[]={100669962,un_profile_suite==2?100670039u:un_profile_suite?100670023u:100669975u,un_profile_suite==2?100670031u:un_profile_suite?100670018u:100667730u};
    for(unsigned i=0;i<3;i++)if(!prepare_hook(un_profile_methods[i],tokens[i],6+i,un_game))return 0;return 1;
}
#endif
