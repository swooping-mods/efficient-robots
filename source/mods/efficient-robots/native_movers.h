#ifndef MOON_UNIT_NATIVE_MOVERS_H
#define MOON_UNIT_NATIVE_MOVERS_H
/* Native crowd broad phase; terrain, capsule/rider admission and final movement
 * remain individually exact. Slot order follows the game's active list. */
#define UN_INDEX_MIN 96u
#include "actor_index.h"
/* Hook-inclusive sweeps below this count have no repeatable index benefit. */
typedef struct {SURoot robot,transform;unsigned dock,id;float radius;unsigned char immovable;} UNActor;
typedef struct {SPVec3 position;float radius;unsigned char robot,immovable,padding[2];} UNMover;
_Static_assert(sizeof(UNMover)==20,"pinned reference-free Mover value layout");
static UNActor un_actors[MU_MAX];static MUSpatial un_index;
static SURoot un_list_root;static unsigned un_count,un_roster_count;static int un_version;
static U64 un_cache_frame=~(U64)0;static PTR un_cache_world;
static PTR un_active_field,un_dock_field,un_dock_id_field,un_length_field,un_width_field,un_fixed_field;
static PTR un_active_enabled,un_component_transform,un_items_field,un_size_field,un_version_field;
static PTR un_instance_id;
static PTR (*un_element_class)(PTR);
static int (*un_value_size)(PTR,unsigned*);
static unsigned char (*un_has_references)(PTR);
static PTR un_mover_array_class;
static PTR un_mover_list_class,un_mover_items,un_mover_size,un_mover_version;
static MUFieldSet un_field_set;
static PTR un_observers_field,un_terrain_field,un_height_field,un_deck_field,un_observer_radius;
static PTR un_walker_riding,un_capsule_center,un_capsule_height,un_lossy_scale,un_up_method;
static PTR un_surface_point,un_base_point,un_get_component;
static U64 un_replaced,un_fallbacks,un_oracle_checks,un_oracle_failures,un_modded_contacts;
static int un_fast_checked,un_fast_ok,un_optimize=1,un_oracle;
static int un_cache_valid;
static int un_cache_tracking;
static UNMover un_output[MU_MAX*2];
static unsigned un_query_slots[MU_MAX];
static UNIdentityIndex un_identities;
static MoonUnitNearby un_foreign[MU_MAX];
static unsigned un_dirty_slots[MU_MAX],un_dirty_count;static unsigned char un_dirty[MU_MAX];
static float un_sqrt(float value){float root;__asm__("sqrtss %1, %0":"=x"(root):"x"(value));return root;}
static SPVec3 un_normal(SPVec3 value){float n=un_sqrt(su_dot(value,value));return n>.00001f?su_mul(value,1/n):su_v(0,0,0);}
static SPVec3 un_position(PTR transform){
    SPVec3 point=((SPVec3(*)(PTR,PTR))*(PTR*)su.tr_get_position)(transform,su.tr_get_position);
    if(!un_fast_checked){
        SPVec3 observed=su_vector(su.tr_get_position,transform);un_fast_checked=1;
        un_fast_ok=su_dot(su_sub(point,observed),su_sub(point,observed))<.0000001f;
        event(un_fast_ok?"native-position-abi-checked":"native-position-abi-failed");
    }
    return un_fast_ok?point:su_vector(su.tr_get_position,transform);
}
static void un_clear_units(void){
    for(unsigned i=0;i<un_count;i++){
        if(un_actors[i].id)un_units->remove(1,un_actors[i].id);
        su_drop(&un_actors[i].robot);su_drop(&un_actors[i].transform);
    }
    su_zero(un_actors,sizeof(un_actors));mu_zero(&un_index,sizeof(un_index));su_zero(&un_identities,sizeof(un_identities));su_zero(un_dirty,sizeof(un_dirty));un_dirty_count=0;un_count=0;un_cache_valid=un_cache_tracking=0;
}
static void un_movers_reset(void){
    un_clear_units();su_drop(&un_list_root);un_cache_world=0;un_cache_frame=~(U64)0;un_version=0;un_roster_count=0;
}
static int un_publish_actor(unsigned i){
    UNActor *actor=&un_actors[i];PTR robot=su_get(actor->robot),world=0;int ok=1;
    if(!robot)return 0;field_value(robot,un_world,&world);
    if(world!=un_sources->world()||!boolean(un_active_enabled,robot,&ok)||!ok){mu_remove(&un_index,i);if(actor->id)un_units->remove(1,actor->id);return 1;}
    if(!actor->transform){PTR tr=call(un_component_transform,robot,0,&ok);actor->transform=su_hold(ok?tr:0);}
    if(!actor->transform)return 0;
    if(!actor->id){
        int identity=integer(un_instance_id,robot,&ok);if(!ok||!identity)return 0;actor->id=(unsigned)identity;
        PTR dock=0;field_value(robot,un_dock_field,&dock);int id=0;if(dock)field_value(dock,un_dock_id_field,&id);actor->dock=id>0?(unsigned)id:0;
    }
    float length=0,width=0;field_value(robot,un_length_field,&length);field_value(robot,un_width_field,&width);field_value(robot,un_fixed_field,&actor->immovable);
    actor->radius=length>width?length:width;SPVec3 p=un_position(su_get(actor->transform));
    if(!mu_valid((MUVec){p.x,p.y,p.z})||!mu_finite(actor->radius)||actor->radius<=0||actor->radius>1000)return 0;
    if(!mu_publish(&un_index,i,i+1,1,(MUVec){p.x,p.y,p.z},0)||!un_units->publish(1,actor->id,1,(MoonUnitVector){p.x,p.y,p.z},actor->radius))return 0;
    return un_identity_put(&un_identities,actor->id,i);
}
static int un_refresh(void){
    if(!un_units->host())return 0;int ok=1,frame=integer(un_frame_method,0,&ok);if(!ok||frame<0)return 0;
    PTR list=0;su.static_field(un_active_field,&list);
    if(!list){list=call(un_active,0,0,&ok);if(!ok||!list)return 0;}
    if(list!=su_get(un_list_root)){
        un_movers_reset();un_list_root=su_hold(list);PTR cls=object_class(list);
        un_items_field=get_field(cls,"_items");un_size_field=get_field(cls,"_size");un_version_field=get_field(cls,"_version");
        if(!un_items_field||!un_size_field||!un_version_field||su.field_offset(un_items_field)!=16||su.field_offset(un_size_field)!=24||su.field_offset(un_version_field)!=28)return 0;
    }
    int count=0,version=0;field_value(list,un_size_field,&count);field_value(list,un_version_field,&version);
    if(count<0||count>MU_MAX)return 0;
    PTR world=un_sources->world();int topology=un_cache_world!=world||un_count!=(unsigned)count||un_version!=version;
    int tracking=un_oracle||un_sources->count(0)||un_roster_count>=UN_INDEX_MIN;
    if(!topology&&un_cache_frame==(U64)frame&&un_cache_tracking==tracking)return un_cache_valid;
    if(topology)un_clear_units();
    PTR array=0;field_value(list,un_items_field,&array);SURoot held=su_hold(array);
    if((count&&!held)||(array&&su.array_length(array)<(U64)count)){su_drop(&held);return 0;}
    unsigned n=(unsigned)count;int valid=1;
    for(unsigned i=0;i<n;i++){
        if(topology){PTR robot=*(PTR*)su.array_address(su_get(held),8,i);un_actors[i].robot=su_hold(robot);}
        if(!un_publish_actor(i))valid=0;
    }
    su_drop(&held);un_count=n;un_version=version;un_cache_world=world;un_cache_frame=(U64)frame;un_cache_valid=valid;un_cache_tracking=tracking;return valid;
}
static int un_actor_slot(PTR robot){
    int id=((int(*)(PTR,PTR))*(PTR*)un_instance_id)(robot,un_instance_id);if(!id)return -1;
    int slot=un_identity_find(&un_identities,(unsigned)id);
    return slot>=0&&(unsigned)slot<un_count&&un_index.items[slot].active&&un_actors[slot].id==(unsigned)id?slot:-1;
}
static void un_mark_pose(PTR robot){
    int slot=un_actor_slot(robot);if(slot>=0&&!un_dirty[slot]){un_dirty[slot]=1;un_dirty_slots[un_dirty_count++]=(unsigned)slot;}
}
static int un_flush_poses(void){
    int ok=1;unsigned count=un_dirty_count;un_dirty_count=0;
    for(unsigned i=0;i<count;i++){unsigned slot=un_dirty_slots[i];un_dirty[slot]=0;if(!un_publish_actor(slot))ok=0;}
    return ok;
}
static SPVec3 un_vector_argument(PTR m,PTR self,SPVec3 point,int *ok){
    void *a[]={&point};PTR boxed=call(m,self,a,ok);SPVec3 result={0};if(*ok&&boxed)su_copy(&result,unbox(boxed),sizeof(result));else *ok=0;return result;
}
static PTR un_component(PTR transform,const char *ns,const char *name,int *ok){PTR type=su_type(ns,name);void *a[]={type};return type?call(un_get_component,transform,a,ok):0;}
static int un_append(unsigned *count,SPVec3 p,float radius,int robot,int fixed){
    if(*count>=MU_MAX*2)return 0;
    un_output[(*count)++]=(UNMover){p,radius,(unsigned char)robot,(unsigned char)fixed,{0,0}};return 1;
}
static int un_admit_observer(PTR robot,PTR terrain,PTR tr,SPVec3 self,SPVec3 center,float body,float deck,int *ok){
    PTR walker=un_component(tr,"MoonCleaners","MoonWalker",ok);if(!*ok)return 0;
    if(!walker)return 1;
    SURoot held=su_hold(walker);PTR riding=call(un_walker_riding,su_get(held),0,ok);
    if(!*ok||su_alive(riding)){su_drop(&held);return 0;}
    PTR capsule=un_component(tr,"UnityEngine","CapsuleCollider",ok);SURoot collider=su_hold(capsule);
    if(!capsule||!*ok){su_drop(&held);su_drop(&collider);return *ok;}
    SPVec3 origin=un_position(tr),normal=su_vector(su.tr_up,tr),scale=su_vector(un_lossy_scale,tr),offset=su_vector(un_capsule_center,su_get(collider));
    float height=su_number(un_capsule_height,su_get(collider));
    SPVec3 feet=su_add(origin,su_mul(normal,(offset.y-height*.5f)*scale.y)),relative=su_sub(feet,center);
    SPVec3 surface=un_vector_argument(un_surface_point,terrain,relative,ok),radial=un_normal(relative);
    int accepted=0;
    if(*ok&&su_dot(su_sub(relative,surface),radial)<=body+.05f){
        SPVec3 up=un_vector_argument(un_up_method,robot,self,ok),base=un_vector_argument(un_base_point,terrain,relative,ok);
        float top=deck>.05f&&body>deck?deck:body;
        if(*ok){
            if(su_dot(su_sub(relative,base),up)<=top-.12f)accepted=1;
            else {SPVec3 delta=su_sub(feet,self);float norm=su_dot(up,up);if(norm>0)delta=su_sub(delta,su_mul(up,su_dot(delta,up)/norm));float radius=0,a=0,b=0;field_value(robot,un_length_field,&a);field_value(robot,un_width_field,&b);radius=a>b?a:b;accepted=un_sqrt(su_dot(delta,delta))>=radius;}
        }
    }
    su_drop(&held);su_drop(&collider);return accepted;
}
static int un_fill_movers(PTR robot,unsigned *written,unsigned *vanilla_count){
    if(!un_refresh()||!un_flush_poses())return 0;
    int slot=un_actor_slot(robot),ok=1;if(slot<0||!un_publish_actor((unsigned)slot)||!un_index.items[slot].active)return 0;
    MUVec published=un_index.items[slot].point;SPVec3 self={published.x,published.y,published.z};unsigned count=0;
    unsigned near=mu_query(&un_index,(MUVec){self.x,self.y,self.z},20,slot,0,un_query_slots);
    for(unsigned i=0;i<near;i++){
        unsigned id=un_query_slots[i];MUItem *p=&un_index.items[id];SPVec3 at={p->point.x,p->point.y,p->point.z};
        if(un_actors[id].id!=un_actors[slot].id&&su_dot(su_sub(at,self),su_sub(at,self))<400&&!un_append(&count,at,un_actors[id].radius,1,un_actors[id].immovable))return 0;
    }
    PTR world=un_sources->world(),observers=0,terrain=0;field_value(world,un_observers_field,&observers);field_value(world,un_terrain_field,&terrain);
    SURoot list=su_hold(observers),ground=su_hold(terrain);U64 n=observers?su.array_length(observers):0;
    if(n>4096||!terrain){su_drop(&list);su_drop(&ground);return 0;}
    SPVec3 center={0};PTR transform=call(un_component_transform,world,0,&ok);if(transform)center=un_position(transform);else ok=0;
    float body=0,deck=0;field_value(robot,un_height_field,&body);field_value(robot,un_deck_field,&deck);
    for(U64 i=0;ok&&i<n;i++){
        PTR tr=*(PTR*)su.array_address(su_get(list),8,i);if(!su_alive(tr))continue;SURoot held=su_hold(tr);
        SPVec3 at=un_position(su_get(held)),delta=su_sub(at,self);
        if(su_dot(delta,delta)<=400){
            void *args[]={su_get(held)};PTR boxed=call(un_observer_radius,0,args,&ok);float radius=ok&&boxed?*(float*)unbox(boxed):0;
            if(radius>0&&un_admit_observer(robot,su_get(ground),su_get(held),self,center,body,deck,&ok)&&!un_append(&count,at,radius,0,0))ok=0;
        }
        su_drop(&held);
    }
    su_drop(&list);su_drop(&ground);if(!ok)return 0;
    *vanilla_count=count;
    /* Modded actors enter the same physical crowd view; original oracle checks
     * exclude this intentionally new interaction when no providers are active. */
    unsigned extras=0;
    if(un_sources->count(0)&&un_contacts->nearby((MoonUnitVector){self.x,self.y,self.z},20,1,un_foreign,MU_MAX,&extras)!=1)return 0;
    for(unsigned i=0;i<extras;i++){
        MoonUnitNearby *p=&un_foreign[i];
        SPVec3 at={p->position.x,p->position.y,p->position.z};if(su_dot(su_sub(at,self),su_sub(at,self))<400){if(!un_append(&count,at,p->radius,1,0))return 0;un_modded_contacts++;}
    }
    *written=count;return 1;
}
static int un_list_fields(PTR robot,PTR *list,PTR *items,PTR *size,PTR *version){
    field_value(robot,un_movers,list);if(!*list)return 0;PTR cls=object_class(*list);if(!cls)return 0;
    if(cls!=un_mover_list_class){
        PTR a=get_field(cls,"_items"),b=get_field(cls,"_size"),c=get_field(cls,"_version");
        if(!a||!b||!c||su.field_offset(a)!=16||su.field_offset(b)!=24||su.field_offset(c)!=28)return 0;
        /* Class metadata outlives worlds; list instances and arrays do not. */
        un_mover_items=a;un_mover_size=b;un_mover_version=c;un_mover_list_class=cls;
    }
    *items=un_mover_items;*size=un_mover_size;*version=un_mover_version;return 1;
}
static int un_store_movers(PTR robot,unsigned count){
    PTR list=0,items=0,size=0,version=0;if(!un_list_fields(robot,&list,&items,&size,&version))return 0;
    SURoot held=su_hold(list);PTR array=0;field_value(list,items,&array);int ok=1;
    if(!array||su.array_length(array)<count){int capacity=(int)count;void *a[]={&capacity};call(get_method(object_class(list),"set_Capacity",1),list,a,&ok);field_value(su_get(held),items,&array);}
    if(!ok||!array||su.array_length(array)<count){su_drop(&held);return 0;}
    PTR array_class=object_class(array);
    if(array_class!=un_mover_array_class){
        PTR element=un_element_class(array_class);unsigned alignment=0;
        if(!element||un_value_size(element,&alignment)!=sizeof(UNMover)||alignment!=4||un_has_references(element)){su_drop(&held);return 0;}
        un_mover_array_class=array_class;
    }
    if(count)su_copy(su.array_address(array,sizeof(UNMover),0),un_output,(U64)count*sizeof(UNMover));
    int revision=0;field_value(su_get(held),version,&revision);unsigned next=(unsigned)revision+1;int length=(int)count;
    un_field_set(su_get(held),size,&length);un_field_set(su_get(held),version,&next);su_drop(&held);return 1;
}
static int un_compare_movers(PTR robot,unsigned expected){
    PTR list=0,items=0,size=0,version=0;if(!un_list_fields(robot,&list,&items,&size,&version))return 0;
    int count=0;PTR array=0;field_value(list,size,&count);field_value(list,items,&array);
    if(count<0||(unsigned)count!=expected||!array||su.array_length(array)<expected){
        if(!un_oracle_failures){char b[256],*p=cat(b,"{\"event\":\"native-oracle-count\",\"expected\":");p=num(p,expected);p=cat(p,",\"actual\":");p=num(p,count>=0?(unsigned)count:0);cat(p,"}\n");un_line(b);}return 0;
    }
    for(unsigned i=0;i<expected;i++){
        UNMover actual;su_copy(&actual,su.array_address(array,sizeof(UNMover),i),sizeof(actual));UNMover *p=&un_output[i];
        if(actual.position.x!=p->position.x||actual.position.y!=p->position.y||actual.position.z!=p->position.z||actual.radius!=p->radius||actual.robot!=p->robot||actual.immovable!=p->immovable){
            if(!un_oracle_failures){
                char b[768],*s=cat(b,"{\"event\":\"native-oracle-item\",\"actors\":");s=num(s,un_count);s=cat(s,",\"subject\":");int subject=un_actor_slot(robot);s=num(s,subject>=0?un_actors[subject].dock:0);s=cat(s,",\"index\":");s=num(s,i);s=cat(s,",\"expected_bits\":[");
                for(unsigned k=0;k<4;k++){unsigned bits=0;su_copy(&bits,(const unsigned char*)p+k*4,4);if(k)*s++=',';s=num(s,bits);}s=cat(s,"],\"actual_bits\":[");
                for(unsigned k=0;k<4;k++){unsigned bits=0;su_copy(&bits,(const unsigned char*)&actual+k*4,4);if(k)*s++=',';s=num(s,bits);}s=cat(s,"],\"expected_robot\":");s=num(s,p->robot);s=cat(s,",\"actual_robot\":");s=num(s,actual.robot);cat(s,"}\n");un_line(b);
            }return 0;
        }
    }
    return 1;
}
static int un_prepare_query(void){
    /* Empty small rosters use plain native movement. Read-only SDK queries can
     * still arrive between moves in one frame, so rebuild their small snapshot
     * on demand instead of retaining an unobserved movement cache. */
    if(!un_oracle&&!un_sources->count(0)&&un_roster_count<UN_INDEX_MIN)un_cache_frame=~(U64)0;
    return un_refresh()&&un_flush_poses();
}
static int un_use_index(void){return un_oracle||un_roster_count>=UN_INDEX_MIN;}
static int un_append_small(PTR robot){
    PTR list=0,items=0,size=0,version=0,array=0;int length=0;
    if(!un_list_fields(robot,&list,&items,&size,&version))return 0;
    field_value(list,size,&length);field_value(list,items,&array);
    if(length<0||length>MU_MAX||!array||su.array_length(array)<(unsigned)length)return 0;
    if(length)su_copy(un_output,su.array_address(array,sizeof(UNMover),0),(U64)length*sizeof(UNMover));
    int ok=1;PTR tr=call(un_component_transform,robot,0,&ok);if(!ok||!tr)return 0;SPVec3 self=un_position(tr);unsigned count=(unsigned)length;
    unsigned n=0;if(un_contacts->nearby((MoonUnitVector){self.x,self.y,self.z},20,1,un_foreign,MU_MAX,&n)!=1)return 0;
    for(unsigned i=0;i<n;i++){
        MoonUnitNearby *p=&un_foreign[i];SPVec3 at={p->position.x,p->position.y,p->position.z};
        if(su_dot(su_sub(at,self),su_sub(at,self))<400){if(!un_append(&count,at,p->radius,1,0))return 0;un_modded_contacts++;}
    }
    return count==(unsigned)length||un_store_movers(robot,count);
}
static unsigned un_snapshot_none(unsigned char *out,unsigned capacity){(void)out;(void)capacity;return 0;}
static int un_receive_none(const unsigned char *data,unsigned size){(void)data;(void)size;return 0;}
static void un_frame(PTR walker){
    (void)walker;if(!__atomic_load_n(&un_started,__ATOMIC_ACQUIRE))return;
    un_fast_install();if(!un_units->host())return;
    PTR list=0;su.static_field(un_active_field,&list);int count=0;
    if(list){PTR f=get_field(object_class(list),"_size");if(f)field_value(list,f,&count);}
    un_roster_count=count>0&&(unsigned)count<=MU_MAX?(unsigned)count:0;
    /* Small/idle rosters incur no transform scan unless an SDK query needs it.
     * Pose hooks keep already-published native entries valid between queries. */
    if(un_oracle)un_prepare_query();
}
static int un_movers_bind(void){
    un_field_set=(MUFieldSet)GetProcAddress(un_game,"il2cpp_field_set_value");if(!un_field_set)return 0;
    un_element_class=(PTR(*)(PTR))GetProcAddress(un_game,"il2cpp_class_get_element_class");
    un_value_size=(int(*)(PTR,unsigned*))GetProcAddress(un_game,"il2cpp_class_value_size");
    un_has_references=(unsigned char(*)(PTR))GetProcAddress(un_game,"il2cpp_class_has_references");
    if(!un_element_class||!un_value_size||!un_has_references)return 0;
    PTR robot=find_class("MoonCleaners","MoonRobotVacuum"),world=find_class("MoonCleaners","MoonWorld"),dock=find_class("MoonCleaners","MoonRobotDock");
    un_active_field=get_field(robot,"active");un_dock_field=get_field(robot,"dock");un_dock_id_field=get_field(dock,"<DockId>k__BackingField");
    un_length_field=get_field(robot,"halfLength");un_width_field=get_field(robot,"halfWidth");un_fixed_field=get_field(robot,"immovable");
    un_height_field=get_field(robot,"bodyHeight");un_deck_field=get_field(robot,"deckHeight");un_observers_field=get_field(world,"observers");un_terrain_field=get_field(world,"<Terrain>k__BackingField");
    un_component_transform=su_method("UnityEngine","Component","get_transform","");un_active_enabled=su_method("UnityEngine","Behaviour","get_isActiveAndEnabled","");
    un_instance_id=su_method("UnityEngine","Object","GetInstanceID","");
    un_observer_radius=su_method("MoonCleaners","MoonRobotVacuum","ObserverRadius","UnityEngine.Transform");un_up_method=su_method("MoonCleaners","MoonRobotVacuum","Up","UnityEngine.Vector3");
    un_surface_point=su_method("MoonCleaners","MoonTerrain","SurfacePoint","UnityEngine.Vector3");un_base_point=su_method("MoonCleaners","MoonTerrain","Point","UnityEngine.Vector3");
    un_get_component=su_method("UnityEngine","Component","GetComponent","System.Type");un_walker_riding=su_method("MoonCleaners","MoonWalker","get_Riding","");
    un_capsule_center=su_method("UnityEngine","CapsuleCollider","get_center","");un_capsule_height=su_method("UnityEngine","CapsuleCollider","get_height","");un_lossy_scale=su_method("UnityEngine","Transform","get_lossyScale","");
    if(!un_active_field||!un_dock_field||!un_dock_id_field||!un_length_field||!un_width_field||!un_fixed_field||!un_height_field||!un_deck_field||!un_observers_field||!un_terrain_field||!un_component_transform||!un_active_enabled||!un_instance_id||!un_observer_radius||!un_up_method||!un_surface_point||!un_base_point||!un_get_component||!un_walker_riding||!un_capsule_center||!un_capsule_height||!un_lossy_scale)return 0;
    return 1;
}
#endif
