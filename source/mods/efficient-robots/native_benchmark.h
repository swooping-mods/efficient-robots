#ifndef MOON_UNIT_NATIVE_BENCHMARK_H
#define MOON_UNIT_NATIVE_BENCHMARK_H
/* Private game-object workload. Each sweep includes our frame refresh and
 * sequential actor publication. This times crowd collection, not rendering,
 * physics or the game's full Update; oracle comparisons stay outside timing. */
static unsigned un_bench_key,un_load_key,un_load_stage,un_load_cells;
static void un_benchmark(void){
    if(!un_refresh()||!un_count||!un_counter||!un_frequency)return;
    unsigned count=un_count,iterations=1024/count;if(iterations<4)iterations=4;if(iterations>64)iterations=64;
    for(unsigned i=0;i<count;i++){
        PTR robot=su_get(un_actors[i].robot);unsigned n=0,vanilla=0;
        if(!un_fill_movers(robot,&n,&vanilla)){event("native-benchmark-preparation-failed");return;}
        un_inside=1;((void(*)(PTR,PTR))*(PTR*)un_gather)(robot,un_gather);un_inside=0;
        if(!un_compare_movers(robot,vanilla)){event("native-benchmark-oracle-failed");return;}
        if(!un_store_movers(robot,n))return;
    }
    int oracle=un_oracle;un_oracle=0;
    for(unsigned batch=0;batch<4;batch++){
        unsigned optimized=batch==1||batch==2;U64 begin=0,end=0,checksum=0;un_counter(&begin);
        for(unsigned sweep=0;sweep<iterations;sweep++){
            int indexed=optimized&&un_use_index();
            if(indexed){un_cache_frame=~(U64)0;if(!un_refresh())return;}
            for(unsigned i=0;i<count;i++){
                PTR robot=su_get(un_actors[i].robot);
                if(indexed){unsigned n=0,vanilla=0;if(!un_fill_movers(robot,&n,&vanilla)||!un_store_movers(robot,n)){event("native-benchmark-collector-failed");return;}checksum+=n;}
                else {un_inside=1;((void(*)(PTR,PTR))*(PTR*)un_gather)(robot,un_gather);un_inside=0;PTR list=0;int n=0;field_value(robot,un_movers,&list);if(list)field_value(list,get_field(object_class(list),"_size"),&n);checksum+=(unsigned)n;}
            }
        }
        un_counter(&end);char b[512],*p=cat(b,"{\"event\":\"native-benchmark\",\"optimized\":");p=num(p,optimized);p=cat(p,",\"actors\":");p=num(p,count);p=cat(p,",\"sweeps\":");p=num(p,iterations);p=cat(p,",\"ticks\":");p=num(p,end-begin);p=cat(p,",\"frequency\":");p=num(p,un_frequency);p=cat(p,",\"checksum\":");p=num(p,checksum);cat(p,"}\n");un_line(b);
    }
    un_oracle=oracle;
}
static void un_load_fixture(PTR walker){
    if(un_load_stage>=5)return;unsigned amounts[]={12,1,16,16,3};unsigned amount=amounts[un_load_stage++],base=un_load_cells;un_load_cells+=amount;
    int ok=1;PTR world=un_sources->world(),tr=call(un_component_transform,walker,0,&ok);if(!tr||!ok)return;
    PTR owner=call(method("MoonCleaners","MoonCrew","get_LocalPlayerId",0),0,0,&ok);SURoot held=su_hold(owner);if(!held)return;
    SPVec3 at=un_position(tr),up=su_vector(su.tr_up,tr),forward=su_vector(su.tr_forward,tr),right=su_vector(su.tr_right,tr);
    for(unsigned i=0;i<amount;i++){
        unsigned cell=i+base;
        SPVec3 p=su_add(at,su_add(su_mul(right,((float)(cell%8)-3.5f)*6),su_mul(forward,20+(cell/8)*6))),point;
        if(!un_floor(p,up,&point))continue;
        int kind=i%3==0?91:i%3==1?92:99;int dock=un_fixture_dock(world,su_get(held),kind,point,up,forward,3);
        if(dock){unsigned char yes=1;void *args[]={&dock,&yes};call(su_method("MoonCleaners","MoonWorld","SetRobotOn","System.Int32;System.Boolean"),world,args,&ok);}
    }
    su_drop(&held);event("native-load-fixture-created");
}
#endif
