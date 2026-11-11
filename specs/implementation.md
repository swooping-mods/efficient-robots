# Efficient Robots: behavior and implementation

Describes the exported implementation **0.1.0**. The linked source and metadata define this version; gameplay qualification is a separate requirement.

## Behavior

An optional Unit Runtime consumer optimizing native vacuum/Joonas mover queries. It owns pinned GatherMovers/Move and PlaceAt/ParkAtDock hooks. A bounded identity map and spatial cache preserve native roster order and final distance/contact tests; modded provider actors append through shared Contacts. Small rosters retain the native collector. World/list-version changes and inspected movement boundaries invalidate cached topology or transforms.

## State and lifecycle

All optimizer state is derived from the current native scene; it introduces no save format. Unit Runtime supplies world, thread, provider and authority admission through its Sources capability. Fast gateways activate only for checked instruction prefixes/ranges; unavailable fast backends retain the supported breakpoint path. Cache failures fall back rather than treating unknown entries as valid.

## Integration and configuration

Runtime dependencies: [unit-runtime](https://github.com/swooping-mods/unit-runtime).

The [module metadata](../source/mods/efficient-robots/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

| Service | ABI | Table size |
|---|---:|---|
| Requires `unit-runtime.units` | 1 | at least 80 bytes |
| Requires `unit-runtime.contacts` | 1 | at least 16 bytes |
| Requires `unit-runtime.sources` | 1 | at least 48 bytes |

## Implementation sources

- [mod.c](../source/mods/efficient-robots/mod.c)
- [native_movers.h](../source/mods/efficient-robots/native_movers.h)
- [native.h](../source/mods/efficient-robots/native.h)
- [fast_hooks.h](../source/mods/efficient-robots/fast_hooks.h)
- [actor_index.h](../source/mods/efficient-robots/actor_index.h)

## Verification boundary

Performance depends on workload; no general FPS gain is promised. Uninspected transform writers, cross-build hooks and live co-op require separate qualification. Historical pre-split timings do not qualify this exported version.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
