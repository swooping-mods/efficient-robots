#ifndef MOON_UNIT_NATIVE_ADAPTER_H
#define MOON_UNIT_NATIVE_ADAPTER_H
#include "../unit-runtime/field_access.h"
/* Pinned native robot adapter and private inspection fixtures. */
static PTR un_game,un_gather,un_update,un_world,un_movers,un_active,un_place;
static int un_inside,un_test,un_inspected,un_key;
static unsigned un_ready_frames,un_ready_signal;
static PTR un_focused,un_blocked,un_first;
static SURoot un_test_docks[3];static unsigned un_test_seen[3];
static SURoot un_fixture_roots[64];static unsigned un_fixture_count;static PTR un_fixture_world;
static unsigned un_view_key,un_order_key,un_ordered[3];static SPVec3 un_initial[3];
static unsigned un_view_at,un_view_ready;
static U64 un_calls,un_cost,un_frequency;
static BOOL (*un_counter)(U64*);
static PTR (*un_field_type)(PTR);
static void un_line(const char *b){DWORD n=0,size=0;while(b[size])size++;WriteFile(un_log,b,size,&n,0);FlushFileBuffers(un_log);}
static int un_fast_install(void);
#include "native_movers.h"
#include "native_profile.h"
static void un_inspect(PTR robot){
    if(un_inspected)return;un_inspected=1;
    const unsigned cells[]={0x2331e08,0x2331240};
    for(unsigned j=0;j<2;j++){
        PTR m=*(PTR*)((unsigned char*)un_game+cells[j]);if(!m||((U64)m&7)||(U64)m<65536)continue;
        char b[1024],*p=cat(b,"{\"event\":\"native-generic\",\"cell\":");p=num(p,cells[j]);p=cat(p,",\"method\":\"");p=cat(p,su.name(m));p=cat(p,"\",\"parameters\":[");
        for(DWORD i=0;i<su.arity(m);i++){if(i)*p++=',';*p++='"';char *s=su.type_name(su.param(m,i));if(s){p=cat(p,s);su.free(s);}*p++='"';}
        cat(p,"]}\n");un_line(b);
    }
    const char *inspect_types[]={"MoonTerrain","MoonWalker"};
    for(unsigned i=0;i<2;i++){
        PTR cls=find_class("MoonCleaners",inspect_types[i]),iterator=0,m;
        while(cls&&(m=su.methods(cls,&iterator))){
            U64 rva=(U64)*(PTR*)m-(U64)un_game;
            if(i==0&&(rva!=0x5dfde0&&rva!=0x5dd8d0))continue;
            if(i==1&&!su_eq(su.name(m),"get_Riding"))continue;
            char b[1024],*p=cat(b,"{\"event\":\"native-support-method\",\"type\":\"");p=cat(p,inspect_types[i]);p=cat(p,"\",\"method\":\"");p=cat(p,su.name(m));p=cat(p,"\",\"rva\":");p=num(p,rva);p=cat(p,",\"parameters\":[");
            for(DWORD j=0;j<su.arity(m);j++){if(j)*p++=',';*p++='"';char *s=su.type_name(su.param(m,j));if(s){p=cat(p,s);su.free(s);}*p++='"';}
            cat(p,"]}\n");un_line(b);
        }
    }
    PTR list=0;field_value(robot,un_movers,&list);if(list){
        PTR cls=object_class(list);const char *names[]={"_items","_size","_version"};
        for(unsigned i=0;i<3;i++){
            PTR f=get_field(cls,names[i]);if(!f)continue;char *type=su.type_name(un_field_type(f));char b[1024],*p=cat(b,"{\"event\":\"mover-list-field\",\"field\":\"");p=cat(p,names[i]);p=cat(p,"\",\"type\":\"");p=cat(p,type);p=cat(p,"\",\"offset\":");p=num(p,(unsigned)su.field_offset(f));cat(p,"}\n");un_line(b);su.free(type);
        }
    }
}
static int un_gather_robot(PTR robot){
        if(!un_sources->main_thread())return 0;
        PTR world=0;field_value(robot,un_world,&world);
        if(un_units->host()&&world==un_sources->world()&&un_optimize){
            if(!un_use_index()){
                if(!un_sources->count(0))return 0;
                un_inside=1;((void(*)(PTR,PTR))*(PTR*)un_gather)(robot,un_gather);un_inside=0;
                if(!un_append_small(robot)){un_fallbacks++;event("native-small-append-failed");}return 1;
            }
            unsigned count=0,vanilla=0;
            if(un_fill_movers(robot,&count,&vanilla)){
                if(un_oracle){
                    un_inside=1;((void(*)(PTR,PTR))*(PTR*)un_gather)(robot,un_gather);un_inside=0;un_oracle_checks++;
                    if(!un_compare_movers(robot,vanilla)){un_oracle_failures++;event("native-oracle-failed");return 1;}
                }
                if(un_store_movers(robot,count)){un_replaced++;if(un_test)un_inspect(robot);return 1;}
            }
            un_fallbacks++;if(un_fallbacks==1)event("native-collector-fallback");
            if(un_sources->count(0)){
                un_inside=1;((void(*)(PTR,PTR))*(PTR*)un_gather)(robot,un_gather);un_inside=0;
                if(!un_append_small(robot))event("native-fallback-append-failed");return 1;
            }
        }
        if(!un_test)return 0;
        U64 begin=0,end=0;if(un_counter)un_counter(&begin);un_inside=1;
        ((void(*)(PTR,PTR))*(PTR*)un_gather)(robot,un_gather);
        un_inside=0;if(un_counter)un_counter(&end);un_calls++;un_cost+=end-begin;
        if(un_test)un_inspect(robot);return 1;
    return 0;
}
static int un_gather_dispatch(PTR robot){
    if(!un_profile_enabled)return un_gather_robot(robot);
    U64 begin=0,end=0;un_counter(&begin);int replaced=un_gather_robot(robot);
    if(!replaced){un_inside=1;((void(*)(PTR,PTR))*(PTR*)un_gather)(robot,un_gather);un_inside=0;}
    un_counter(&end);un_profile_ticks[3]+=end-begin;un_profile_calls[3]++;return 1;
}
#include "fast_hooks.h"
static void un_fast_gather(PTR robot,PTR method_id){
    if(!un_sources->main_thread()){
        ((void(*)(PTR,PTR))(un_fast_hooks[0].gateway+16))(robot,method_id);return;
    }
    if(!un_profile_enabled&&!un_oracle&&!un_sources->count(0)&&un_roster_count<UN_INDEX_MIN){
        ((void(*)(PTR,PTR))(un_fast_hooks[0].gateway+16))(robot,method_id);return;
    }
    if(!un_inside&&un_gather_dispatch(robot))return;
    ((void(*)(PTR,PTR))(un_fast_hooks[0].gateway+16))(robot,method_id);
}
static void un_fast_move(PTR robot,const SPVec3 *delta,PTR method_id){
    if(un_units->host()&&!un_inside&&un_count&&(un_oracle||un_sources->count(0)||un_roster_count>=UN_INDEX_MIN))un_mark_pose(robot);
    /* Win64 passes this 12-byte value indirectly in RDX. Forward its native
     * storage unchanged so the original body can remain a tail call. */
    ((void(*)(PTR,const SPVec3*,PTR))(un_fast_hooks[1].gateway+16))(robot,delta,method_id);
}
static int un_before(int id,Context *context){
    if(!__atomic_load_n(&un_started,__ATOMIC_ACQUIRE))return 0;
    if(!un_sources->main_thread())return 0;
    if(un_inside)return 0;
    if(id>=6&&id<=8)return un_profile_before((unsigned)id-6,context);
    if(id>=3&&id<=5){if((un_oracle||un_sources->count(0)||un_roster_count>=UN_INDEX_MIN)&&un_units->host())un_mark_pose((PTR)context->rcx);return 0;}
    if(id==1)return un_gather_dispatch((PTR)context->rcx);
    return 0;
}
static int un_set(PTR object,const char *name,void *value){
    PTR field=object?get_field(object_class(object),name):0;
    void (*set)(PTR,PTR,void*)=(void(*)(PTR,PTR,void*))GetProcAddress(un_game,"il2cpp_field_set_value");
    if(!field||!set)return 0;set(object,field,value);return 1;
}
static int un_reference(PTR object,const char *name,PTR value){
    PTR field=object?get_field(object_class(object),name):0;
    MUFieldSet set=(MUFieldSet)GetProcAddress(un_game,"il2cpp_field_set_value");
    return mur_reference(object,field,value,set,field_value);
}
static int un_fixture_dock(PTR world,PTR owner,int kind,SPVec3 point,SPVec3 up,SPVec3 forward,unsigned slot){
    if(un_fixture_count>=64)return 0;
    int ok=1;void *which[]={&kind};PTR def=call(su_method("MoonCleaners","MoonWorld","BuildDefinition","System.Int32"),world,which,&ok);SURoot definition=su_hold(def);
    PTR prefab=0;if(def)field_value(def,get_field(object_class(def),"prefab"),&prefab);
    if(!ok||!prefab){su_drop(&definition);event("native-fixture-prefab-missing");return 0;}
    SUQuat rotation=su_rotation(forward,up);void *clone_args[]={prefab,&point,&rotation};
    PTR go=call(su_method("UnityEngine","Object","Instantiate","UnityEngine.Object;UnityEngine.Vector3;UnityEngine.Quaternion"),0,clone_args,&ok);SURoot root=su_hold(go);
    PTR dock_type=su_type("MoonCleaners","MoonRobotDock");void *component[]={dock_type};
    PTR dock=go?call(su.go_component,go,component,&ok):0;
    if(!dock&&go){unsigned char inactive=1;void *children[]={dock_type,&inactive};dock=call(su_method("UnityEngine","GameObject","GetComponentInChildren","System.Type;System.Boolean"),go,children,&ok);}
    if(!ok||!dock){if(go)su_destroy(go);su_drop(&root);su_drop(&definition);event("native-fixture-component-missing");return 0;}
    PTR cls=object_class(world),records=0;field_value(world,get_field(cls,"depots"),&records);
    int id=0;field_value(world,get_field(cls,"nextDepotId"),&id);if(id<1||id>=2147483600||!records){su_destroy(go);su_drop(&root);su_drop(&definition);return 0;}
    PTR record=su_new("MoonCleaners","MoonDepotSnapshot");SURoot snapshot=su_hold(record);
    call(method("MoonCleaners","MoonDepotSnapshot",".ctor",0),record,0,&ok);
    float yaw=0;
    ok=ok&&un_set(record,"id",&id)&&un_set(record,"kind",&kind)&&un_reference(record,"owner",owner)&&un_set(record,"x",&point.x)&&un_set(record,"y",&point.y)&&un_set(record,"z",&point.z)&&un_set(record,"yaw",&yaw);
    if(ok){void *insert[]={&id,record};call(get_method(object_class(records),"Add",2),records,insert,&ok);}
    if(ok){int next=id+1;un_set(world,"nextDepotId",&next);void *setup[]={world,&id,owner};call(su_method("MoonCleaners","MoonRobotDock","Setup","MoonCleaners.MoonWorld;System.Int32;System.String"),dock,setup,&ok);}
    if(ok){unsigned char yes=1;void *roam[]={&yes};call(su_method("MoonCleaners","MoonRobotDock","set_Roams","System.Boolean"),dock,roam,&ok);}
    if(ok){if(slot<3)un_test_docks[slot]=su_hold(dock);un_fixture_roots[un_fixture_count++]=root;root=0;}else if(go)su_destroy(go);
    su_drop(&snapshot);su_drop(&root);su_drop(&definition);return ok?id:0;
}
static void un_sample(void){
    char b[1024],*p=cat(b,"{\"event\":\"native-unit-sample\",\"actors\":");p=num(p,un_count);p=cat(p,",\"replaced\":");p=num(p,un_replaced);p=cat(p,",\"fallbacks\":");p=num(p,un_fallbacks);p=cat(p,",\"oracle_checks\":");p=num(p,un_oracle_checks);p=cat(p,",\"oracle_failures\":");p=num(p,un_oracle_failures);p=cat(p,",\"modded_contacts\":");p=num(p,un_modded_contacts);p=cat(p,",\"modded_helpers\":");p=num(p,un_sources->count(2));p=cat(p,",\"modded_slimes\":");p=num(p,un_sources->count(3));p=cat(p,",\"moving\":[");
    for(unsigned i=0;i<3;i++){
        if(i)*p++=',';unsigned moved=0;if(un_ordered[i]&&un_test_docks[i]){
            int ok=1;PTR robot=call(su_method("MoonCleaners","MoonRobotDock","get_Robot",""),su_get(un_test_docks[i]),0,&ok),tr=robot?call(un_component_transform,robot,0,&ok):0;
            if(tr&&ok){SPVec3 delta=su_sub(un_position(tr),un_initial[i]);moved=su_dot(delta,delta)>.01f;}
        }p=num(p,moved);
    }
    cat(p,"]}\n");un_line(b);
}
static int un_floor(SPVec3 query,SPVec3 up,SPVec3 *point){
    int ok=1,mask=-1,triggers=1;float length=30;SPVec3 origin=su_add(query,su_mul(up,8)),direction=su_mul(up,-1);
    union {U64 align;unsigned char bytes[96];} hit;su_zero(&hit,sizeof(hit));void *a[]={&origin,&direction,hit.bytes,&length,&mask,&triggers};PTR yes=call(su.physics_ray,0,a,&ok);
    if(!ok||!yes||!*(unsigned char*)unbox(yes))return 0;su_copy(point,hit.bytes+su.hit_point,sizeof(*point));return 1;
}
static void un_fixture_view(PTR walker){
    if(!un_test_docks[1])return;int ok=1;PTR tr=call(un_component_transform,su_get(un_test_docks[1]),0,&ok);if(!tr||!ok)return;
    SPVec3 up=su_vector(su.tr_up,tr),forward=su_vector(su.tr_forward,tr),at=un_position(tr),point;
    if(!un_floor(su_add(at,su_mul(forward,8)),up,&point))return;
    point=su_add(point,su_mul(up,.95f));SPVec3 facing=su_mul(forward,-1);float pitch=8;unsigned char refresh=1;
    void *teleport[]={&point,&refresh};call(su_method("MoonCleaners","MoonWalker","Teleport","UnityEngine.Vector3;System.Boolean"),walker,teleport,&ok);
    void *place[]={&point,&facing,&pitch};call(su_method("MoonCleaners","MoonWalker","Place","UnityEngine.Vector3;UnityEngine.Vector3;System.Single"),walker,place,&ok);if(ok){un_view_at=un_ready_frames;un_view_ready=0;event("native-fixture-view");}
}
static void un_fixture_order(void){
    event("native-fixture-order-key");
    PTR (*nested)(PTR,PTR*)=(PTR(*)(PTR,PTR*))GetProcAddress(un_game,"il2cpp_class_get_nested_types");const char *(*class_name)(PTR)=(const char *(*)(PTR))GetProcAddress(un_game,"il2cpp_class_get_name");
    PTR iterator=0,child=0,travel=0;if(!nested||!class_name)return;
    while((child=nested(find_class("MoonCleaners","MoonRobotVacuum"),&iterator)))if(su_eq(class_name(child),"State")){travel=get_field(child,"Travel");break;}if(!travel){event("native-fixture-state-missing");return;}
    int state=0;su.static_field(travel,&state);
    for(unsigned i=0;i<3;i++)if(un_test_docks[i]){
        int ok=1;PTR robot=call(su_method("MoonCleaners","MoonRobotDock","get_Robot",""),su_get(un_test_docks[i]),0,&ok),tr=robot?call(un_component_transform,robot,0,&ok):0;if(!tr||!ok)continue;
        SPVec3 at=un_position(tr),up=su_vector(su.tr_up,tr),forward=su_vector(su.tr_forward,tr),goal;
        if(!un_floor(su_add(at,su_mul(forward,6)),up,&goal)){event("native-fixture-route-floor-missing");continue;}
        void *args[]={&goal};PTR yes=call(su_method("MoonCleaners","MoonRobotVacuum","PlanRoute","UnityEngine.Vector3"),robot,args,&ok);
        if(!ok||!yes||!*(unsigned char*)unbox(yes)){event("native-test-route-rejected");continue;}
        float zero=0;if(un_set(robot,"target",&goal)&&un_set(robot,"state",&state)&&un_set(robot,"stateTime",&zero)){un_ordered[i]=1;un_initial[i]=at;event("native-point-order");}
    }
}
#include "native_benchmark.h"
#include "native_movement_benchmark.h"
static void un_test_frame(PTR walker){
    if(un_test&&un_fixture_world!=un_sources->world()){
        for(unsigned i=0;i<un_fixture_count;i++){PTR go=su_get(un_fixture_roots[i]);if(su_alive(go))su_destroy(go);su_drop(&un_fixture_roots[i]);}
        for(unsigned i=0;i<3;i++)su_drop(&un_test_docks[i]);un_fixture_count=0;
        su_zero(un_test_seen,sizeof(un_test_seen));su_zero(un_ordered,sizeof(un_ordered));un_view_at=un_view_ready=un_key=un_order_key=un_view_key=0;un_fixture_world=un_sources->world();
        un_profile_enabled=un_profile_key=0;un_oracle=un_test;
        un_bench_key=un_load_key=un_load_stage=un_load_cells=umb_key=0;
    }
    if(!un_test||!un_units->host()){un_ready_frames=un_ready_signal=0;return;}
    if(++un_ready_frames<120)return;
    int allowed=1;if(!boolean(un_focused,0,&allowed)||boolean(un_blocked,walker,&allowed)||!boolean(un_first,walker,&allowed)||!allowed)return;
    if(!un_ready_signal){un_ready_signal=1;event("native-fixture-ready");}
    if(un_view_at&&!un_view_ready&&un_ready_frames-un_view_at>=60){un_view_ready=1;event("native-fixture-view-ready");}
    for(unsigned i=0;i<3;i++)if(un_test_docks[i]&&!un_test_seen[i]){
        int ok=1;PTR robot=call(su_method("MoonCleaners","MoonRobotDock","get_Robot",""),su_get(un_test_docks[i]),0,&ok);
        if(robot&&ok){call(un_gather,robot,0,&ok);if(ok){un_test_seen[i]=1;event("native-robot-ready");}}
    }
    if(un_test_seen[0]&&un_test_seen[1]&&un_test_seen[2]){
        unsigned i=un_ready_frames%3;int ok=1;PTR robot=call(su_method("MoonCleaners","MoonRobotDock","get_Robot",""),su_get(un_test_docks[i]),0,&ok);if(robot&&ok)call(un_gather,robot,0,&ok);
    }
    if(un_count&&un_ready_frames%60==0)un_sample();
    /* Dependency fixtures must not consume a consumer's ordinary F-key tools. */
    int controls=moon_input_key(0x11)&&moon_input_key(0x10);
    unsigned view=controls&&moon_input_key(0x76),order=controls&&moon_input_key(0x77);if(view&&!un_view_key)un_fixture_view(walker);if(order&&!un_order_key)un_fixture_order();un_view_key=view;un_order_key=order;
    unsigned bench=controls&&moon_input_key(0x78),load=controls&&moon_input_key(0x79);if(bench&&!un_bench_key)un_benchmark();if(load&&!un_load_key)un_load_fixture(walker);un_bench_key=bench;un_load_key=load;
    unsigned movement=controls&&moon_input_key(0x7a);if(movement&&!umb_key)un_movement_benchmark();umb_key=movement;
    un_profile_control(controls&&moon_input_key(0x7b));
    int key=controls&&moon_input_key(0x75);if(!key||un_key){un_key=key;return;}un_key=key;
    if(un_fixture_count)return;
    event("native-fixture-key");
    int ok=1;PTR tr=call(method("UnityEngine","Component","get_transform",0),walker,0,&ok);if(!tr||!ok)return;
    SPVec3 at=su_vector(su.tr_get_position,tr),up=su_vector(su.tr_up,tr),right=su_vector(su.tr_right,tr),forward=su_vector(su.tr_forward,tr);
    PTR owner=call(method("MoonCleaners","MoonCrew","get_LocalPlayerId",0),0,0,&ok);if(!owner||!ok)return;
    const char *names[]={"RobotDockKind","RobotDockMk2Kind","JoonasKind"};
    for(unsigned i=0;i<3;i++){
        int kind=0;PTR f=get_field(find_class("MoonCleaners","MoonWorld"),names[i]);if(!f)continue;su.static_field(f,&kind);
        float distance=un_sources->count(0)?5:30;
        SPVec3 desired=su_add(at,su_add(su_mul(forward,distance),su_mul(right,((float)i-1)*12)));
        SPVec3 origin=su_add(desired,su_mul(up,5)),direction=su_mul(up,-1);float length=20;int mask=-1,triggers=1;
        union {U64 align;unsigned char bytes[96];} hit;su_zero(&hit,sizeof(hit));void *rays[]={&origin,&direction,hit.bytes,&length,&mask,&triggers};
        PTR found=call(su.physics_ray,0,rays,&ok);if(!ok||!found||!*(unsigned char*)unbox(found)){event("native-test-ray-miss");continue;}
        SPVec3 point;su_copy(&point,hit.bytes+su.hit_point,sizeof(point));
        int dock=un_fixture_dock(un_sources->world(),owner,kind,point,up,forward,i);
        char b[256],*p=cat(b,"{\"event\":\"native-test-dock\",\"kind\":");p=num(p,(unsigned)kind);p=cat(p,",\"dock\":");p=num(p,dock>0?(unsigned)dock:0);cat(p,"}\n");un_line(b);
        if(dock>0){unsigned char on=1;void *power[]={&dock,&on};call(su_method("MoonCleaners","MoonWorld","SetRobotOn","System.Int32;System.Boolean"),un_sources->world(),power,&ok);}
    }
}
static int un_bind(PTR game){
    un_game=game;un_gather=su_method("MoonCleaners","MoonRobotVacuum","GatherMovers","");un_update=su_method("MoonCleaners","MoonRobotVacuum","Update","");
    un_active=su_method("MoonCleaners","MoonRobotVacuum","get_Active","");un_place=su_method("MoonCleaners","MoonWorld","DevPlaceRobotDock","System.String;System.Int32;UnityEngine.Vector3;System.Single");
    PTR cls=find_class("MoonCleaners","MoonRobotVacuum");un_world=get_field(cls,"world");un_movers=get_field(cls,"movers");
    un_field_type=(PTR(*)(PTR))GetProcAddress(game,"il2cpp_field_get_type");
    un_focused=su_method("UnityEngine","Application","get_isFocused","");un_blocked=su_method("MoonCleaners","MoonWalker","get_MenuBlocked","");un_first=su_method("MoonCleaners","MoonWalker","get_IsFirstPerson","");
    un_counter=(BOOL(*)(U64*))GetProcAddress(GetModuleHandleA("kernel32.dll"),"QueryPerformanceCounter");
    BOOL (*frequency)(U64*)=(BOOL(*)(U64*))GetProcAddress(GetModuleHandleA("kernel32.dll"),"QueryPerformanceFrequency");if(frequency)frequency(&un_frequency);
    char test[8];un_test=GetEnvironmentVariableA("MOONLAB_TEST_TOOLS",test,sizeof(test))==1&&test[0]=='1';
    un_oracle=un_test;
    if(!un_gather||!un_world||!un_movers||!un_field_type||!un_active||!un_place||!un_focused||!un_blocked||!un_first)return 0;
    if(!un_movers_bind())return 0;
    return (!un_test||un_profile_bind())&&prepare_hook(un_gather,100670013,1,game)
        &&prepare_hook(su_method("MoonCleaners","MoonRobotVacuum","Move","UnityEngine.Vector3"),100670021,3,game)
        &&prepare_hook(su_method("MoonCleaners","MoonRobotVacuum","PlaceAt","UnityEngine.Vector3;UnityEngine.Vector3"),100669961,4,game)
        &&prepare_hook(su_method("MoonCleaners","MoonRobotVacuum","ParkAtDock",""),100669960,5,game);
}
#endif
