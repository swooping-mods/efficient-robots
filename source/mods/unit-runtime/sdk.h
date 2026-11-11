#ifndef MOON_UNIT_SDK_H
#define MOON_UNIT_SDK_H
/* Process-lifetime API: register a fully initialized immutable provider from the
 * ModStart thread; all other calls/callbacks use the game thread. Network receive
 * callbacks run only for authenticated current-host snapshots. */
#define MU_ABI 1u
#define MU_PROVIDER_MAX 8u
#define MU_PAYLOAD_MAX 960u
#define MU_COMMAND_MAX 64u
#define MU_UNIT_MAX 2048u
typedef struct {float x,y,z;} MoonUnitVector;
typedef struct {unsigned provider,id,kind;MoonUnitVector position;float radius;} MoonUnitNearby;
typedef struct {
    unsigned size,id;
    unsigned (*snapshot)(unsigned char *,unsigned);
    int (*receive)(const unsigned char *,unsigned);
    void (*reset)(void);
    void (*frame)(void *walker);
    /* Host callback receives the Steam-authenticated sender. Returning zero
     * rejects an invalid request without committing its sequence. */
    unsigned (*command)(unsigned long long peer,const unsigned char *,unsigned,unsigned char *,unsigned);
    int (*result)(const unsigned char *,unsigned);
} MoonUnitProvider;
typedef struct {
    unsigned size,abi;
    int (*register_provider)(const MoonUnitProvider *);
    int (*publish)(unsigned provider,unsigned id,unsigned kind,MoonUnitVector,float radius);
    void (*remove)(unsigned provider,unsigned id);
    /* Output capacity must cover every active unit: -1 requests a retry with
     * the returned required count. No contact query silently drops neighbors. */
    int (*nearby)(MoonUnitVector,float,unsigned provider,unsigned id,MoonUnitNearby *,unsigned capacity,unsigned *required);
    /* One clock per provider, once per engine frame. Unprocessed time stays in
     * the backlog; caller decides when to consume the returned fixed steps. */
    unsigned (*steps)(unsigned provider,unsigned long long frame,float dt);
    int (*host)(void);
    unsigned long long (*session)(void);
    /* Guest: at most one outstanding command per provider, retried until an
     * authenticated result is accepted or world authority ends. */
    int (*command)(unsigned provider,const unsigned char *,unsigned);
    /* Borrowed native remote-player component, valid only on this callback's
     * main thread and only while the authenticated peer remains loaded. */
    void *(*peer_actor)(unsigned long long peer);
} MoonUnits;
typedef const MoonUnits *(*MoonUnitsGet)(unsigned,unsigned);
/* Optional capability: preserves the original MoonUnits ABI/size for existing
 * consumers. Foreign modded markers only; never refreshes native scene poses. */
#define MU_CONTACT_ABI 1u
typedef struct {
    unsigned size,abi;
    int (*nearby)(MoonUnitVector,float,unsigned own_provider,MoonUnitNearby *,unsigned capacity,unsigned *required);
} MoonUnitContacts;
typedef const MoonUnitContacts *(*MoonUnitContactsGet)(unsigned,unsigned);
#endif
