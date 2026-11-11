# Efficient Robots

Optional native vacuum and Joonas crowd collection optimization.

## Install

Install [Moon Sweepers Mod Loader](https://github.com/swooping-mods/moon-sweepers-mod-loader) first.

Download [efficient-robots-0.1.0.msmod](content/efficient-robots-0.1.0.msmod), import it with **Install mod package**, then select the mod and its required frameworks.

Required mods: [unit-runtime](https://github.com/swooping-mods/unit-runtime).

## Compatibility

Version 0.1.0. Targets the pinned Steam Windows x64 IL2CPP build of Moon Sweepers (Steam app 5335510). On Linux, the game runs through Proton. Experimental: back up saves before using save-affecting mods.

## Build

Run `python3 build.py --check` to verify source hashes, then `python3 build.py` to write a build under `.local/build/`. Requires installed Clang, lld-link and llvm-dlltool.

## Credits

See [CREDITS.txt](CREDITS.txt) and [third-party notices](vendored/THIRD_PARTY_NOTICES.md).
