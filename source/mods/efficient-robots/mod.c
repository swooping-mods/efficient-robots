#include "../../runtime/native/gameplay.h"
#include "../../runtime/native/input.h"
#include "../unit-runtime/adapters/unity.h"
#include "../unit-runtime/client.h"
#include "../unit-runtime/spatial.h"
int _fltused=0;
void *memcpy(void *to,const void *from,U64 n){su_copy(to,from,n);return to;}
void *memset(void *to,int c,U64 n){unsigned char *p=to;for(U64 i=0;i<n;i++)p[i]=(unsigned char)c;return to;}
static const MoonUnits *un_units;
static const MoonUnitContacts *un_contacts;
static const MoonUnitSources *un_sources;
static PTR un_log,un_frame_method;
static int un_started;
static void event(const char *name){
    char b[256],*p=cat(b,"{\"event\":\"");p=cat(p,name);p=cat(p,"\"}\n");
    DWORD n;WriteFile(un_log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(un_log);
}
#include "native.h"
static void optimizer_frame(void *walker){
    if(!__atomic_load_n(&un_started,__ATOMIC_ACQUIRE))return;
    un_frame(walker);un_test_frame(walker);
}
static int optimizer_prepare(void){
    return __atomic_load_n(&un_started,__ATOMIC_ACQUIRE)&&un_prepare_query();
}
static void inspect(PTR game){
    PTR (*methods)(PTR,PTR*)=(PTR(*)(PTR,PTR*))GetProcAddress(game,"il2cpp_class_get_methods");
    const char *(*name)(PTR)=(const char *(*)(PTR))GetProcAddress(game,"il2cpp_method_get_name");
    DWORD (*arity)(PTR)=(DWORD(*)(PTR))GetProcAddress(game,"il2cpp_method_get_param_count");
    PTR (*param)(PTR,DWORD)=(PTR(*)(PTR,DWORD))GetProcAddress(game,"il2cpp_method_get_param");
    PTR (*ret)(PTR)=(PTR(*)(PTR))GetProcAddress(game,"il2cpp_method_get_return_type");
    char *(*type_name)(PTR)=(char *(*)(PTR))GetProcAddress(game,"il2cpp_type_get_name");
    void (*free_string)(PTR)=(void(*)(PTR))GetProcAddress(game,"il2cpp_free");
    if(!methods||!name||!arity||!param||!ret||!type_name||!free_string)return;
    const char *types[]={"MoonRobotVacuum","MoonJoonasRig","MoonRobotDock","MoonCrewSession","MoonWorld"};
    for(unsigned t=0;t<sizeof(types)/sizeof(*types);t++){
        PTR cls=find_class("MoonCleaners",types[t]),it=0,m;
        while(cls&&(m=methods(cls,&it))){
            static char b[4096];char *p=cat(b,"{\"type\":\"");p=cat(p,types[t]);p=cat(p,"\",\"method\":\"");p=cat(p,name(m));
            p=cat(p,"\",\"token\":");p=num(p,method_token(m));p=cat(p,",\"rva\":");p=num(p,(U64)*(PTR*)m-(U64)game);
            p=cat(p,",\"parameters\":[");
            for(DWORD i=0;i<arity(m);i++){if(i)*p++=',';*p++='"';char *s=type_name(param(m,i));if(s){p=cat(p,s);free_string(s);}*p++='"';}
            p=cat(p,"],\"returns\":\"");char *s=type_name(ret(m));if(s){p=cat(p,s);free_string(s);}p=cat(p,"\"}\n");
            DWORD n;WriteFile(un_log,b,(DWORD)(p-b),&n,0);
        }
    }
    FlushFileBuffers(un_log);
    /* These two generic MethodInfo cells are read-only inspection inputs
     * identified by GatherMovers' checked disassembly, never invocation targets. */
    const unsigned cells[]={0x2331e08,0x2331240};
    for(unsigned j=0;j<2;j++){
        PTR m=*(PTR*)((unsigned char*)game+cells[j]);if(!m||((U64)m&7)||(U64)m<65536)continue;
        char b[1024],*p=cat(b,"{\"generic_cell\":");p=num(p,cells[j]);p=cat(p,",\"method\":\"");p=cat(p,name(m));p=cat(p,"\",\"parameters\":[");
        for(DWORD i=0;i<arity(m);i++){if(i)*p++=',';*p++='"';char *s=type_name(param(m,i));if(s){p=cat(p,s);free_string(s);}*p++='"';}
        p=cat(p,"]}\n");DWORD n;WriteFile(un_log,b,(DWORD)(p-b),&n,0);
    }
    FlushFileBuffers(un_log);
}
__declspec(dllexport) int MoonLab_ModStart(PTR domain,PTR game){
    if(__atomic_load_n(&un_started,__ATOMIC_ACQUIRE))return 1;
    char folder[1000],path[1100];DWORD n=GetEnvironmentVariableA("MOONLAB_EVIDENCE",folder,sizeof(folder));if(!n||n>=sizeof(folder))return 0;
    cat(cat(path,folder),"\\efficient-robots.jsonl");un_log=CreateFileA(path,0x40000000,1,0,1,0x80,0);if(un_log==(PTR)(U64)-1)return 0;
    if(!bind_runtime(domain,game)||!su_bind(game)){event("bind-failed");return 0;}
    un_units=mu_connect(domain,game);un_contacts=mu_connect_contacts();un_sources=mu_connect_sources();
    if(!un_units||!un_contacts||!mu_sources_valid(un_sources)){event("runtime-capability-missing");return 0;}
    un_frame_method=su_method("UnityEngine","Time","get_frameCount","");
    if(!un_frame_method||!un_bind(game)){event("native-bind-failed");return 0;}
    char inspection[8];if(GetEnvironmentVariableA("MOONLAB_UNIT_INSPECT",inspection,sizeof(inspection))==1&&inspection[0]=='1')inspect(game);
    before_hook=un_before;if(!install_hooks())return 0;
    MoonUnitProvider provider={sizeof(provider),1,un_snapshot_none,un_receive_none,un_movers_reset,optimizer_frame,0,0};
    if(!un_units->register_provider(&provider))return 0;
    MoonUnitQuerySource source={sizeof(source),1,optimizer_prepare};
    if(!un_sources->register_source(&source))return 0;
    __atomic_store_n(&un_started,1,__ATOMIC_RELEASE);event("ready");return 1;
}
