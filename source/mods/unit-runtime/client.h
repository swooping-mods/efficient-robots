#ifndef MOON_UNIT_CLIENT_H
#define MOON_UNIT_CLIENT_H
#include "sdk.h"
#include "sources.h"
#include "../../runtime/native/services.h"
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(PTR,char *,DWORD);
static const MoonUnits *mu_connect(PTR domain,PTR game){
    if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        return runtime?(const MoonUnits *)runtime->find("unit-runtime","units",MU_ABI,sizeof(MoonUnits)):0;
    }
    char path[1200];DWORD n=GetModuleFileNameA(0,path,1000);if(!n||n>=1000)return 0;
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;if(!n)return 0;
    cat(path+n,"MoonLab\\mods\\unit-runtime\\mod.dll");PTR module=LoadLibraryA(path);if(!module)return 0;
    int (*start)(PTR,PTR)=(int(*)(PTR,PTR))GetProcAddress(module,"MoonLab_ModStart");
    MoonUnitsGet get=(MoonUnitsGet)GetProcAddress(module,"MoonLab_Units_v1");
    if(!start||!get||!start(domain,game))return 0;return get(MU_ABI,sizeof(MoonUnits));
}
static inline const MoonUnitContacts *mu_connect_contacts(void){
    if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        return runtime?(const MoonUnitContacts *)runtime->find("unit-runtime","contacts",MU_CONTACT_ABI,sizeof(MoonUnitContacts)):0;
    }
    char path[1200];DWORD n=GetModuleFileNameA(0,path,1000);if(!n||n>=1000)return 0;
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;if(!n)return 0;
    cat(path+n,"MoonLab\\mods\\unit-runtime\\mod.dll");PTR module=GetModuleHandleA(path);if(!module)return 0;
    MoonUnitContactsGet get=(MoonUnitContactsGet)GetProcAddress(module,"MoonLab_UnitContacts_v1");
    return get?get(MU_CONTACT_ABI,sizeof(MoonUnitContacts)):0;
}
static inline const MoonUnitSources *mu_connect_sources(void){
    if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        return runtime?(const MoonUnitSources *)runtime->find("unit-runtime","sources",MU_SOURCES_ABI,sizeof(MoonUnitSources)):0;
    }
    char path[1200];DWORD n=GetModuleFileNameA(0,path,1000);if(!n||n>=1000)return 0;
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;if(!n)return 0;
    cat(path+n,"MoonLab\\mods\\unit-runtime\\mod.dll");PTR module=GetModuleHandleA(path);if(!module)return 0;
    MoonUnitSourcesGet get=(MoonUnitSourcesGet)GetProcAddress(module,"MoonLab_UnitSources_v1");
    return get?get(MU_SOURCES_ABI,sizeof(MoonUnitSources)):0;
}
#endif
