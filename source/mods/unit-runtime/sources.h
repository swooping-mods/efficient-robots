#ifndef MOON_UNIT_SOURCES_H
#define MOON_UNIT_SOURCES_H
/* Optional capability; the original MoonUnits/MoonUnitContacts ABI is unchanged.
 * Query sources publish external scene markers on demand before nearby queries.
 * All callbacks and borrowed world access run on the current game thread. */
#include "sdk.h"
#define MU_SOURCES_ABI 1u
typedef struct {
    unsigned size,provider;
    int (*prepare)(void);
} MoonUnitQuerySource;
typedef struct {
    unsigned size,abi;
    int (*register_source)(const MoonUnitQuerySource *);
    void *(*world)(void);
    int (*main_thread)(void);
    /* Zero returns all modded markers (providers 2..8); 1..8 select a provider.
     * Provider 1 conventionally owns vanilla scene markers. */
    unsigned (*count)(unsigned provider);
    /* One-way failure for the current world: stop authority, discard markers,
     * reset providers, and require normal world reentry before resuming. */
    int (*suspend)(void);
} MoonUnitSources;
typedef const MoonUnitSources *(*MoonUnitSourcesGet)(unsigned,unsigned);
static inline int mu_sources_valid(const MoonUnitSources *api){
    return api&&api->size==sizeof(*api)&&api->abi==MU_SOURCES_ABI&&api->register_source
        &&api->world&&api->main_thread&&api->count&&api->suspend;
}
#endif
