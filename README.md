# FreeSSM (maintained fork)

free diagnostics for subaru (SSM1/SSM2): read and clear codes, live data, adjustments.

the original by [Comer352L](https://github.com/Comer352L/FreeSSM) has been quiet since april 2024. this fork keeps it building and merges the fixes people sent in. everything goes back upstream if he picks it up again ([note here](https://github.com/Comer352L/FreeSSM/issues/93)).

**what's different here**

- runs natively on mac (apple silicon). SSM1's odd 1953 baud speed is in there too, not tried on a real car yet
- 32-bit windows build, so 32-bit-only cable drivers like the tactrix openport 2.0 get found (fixes [#83](https://github.com/Comer352L/FreeSSM/issues/83))
- `j2534list.exe` shows which cable drivers windows can see
- mac, linux and windows downloads built automatically on every change

**download:** [releases](https://github.com/cuedown/FreeSSM/releases). these are test builds: reading codes and live data only, don't flash with them yet.

**tested on:** a simulated subaru ECU and a windows 11 VM so far. if you've got a real subaru, please try it and [open an issue](https://github.com/cuedown/FreeSSM/issues) saying what car and cable, worked or not. that's how this becomes a real release.

**coming in test build 2:** the PRs that sat unmerged upstream: Jbsco's echo fix, live curve plots and ECU simulator, jimihimi's TPMS support and jmadden173's CSV logging (with a crash fix). they're on the [community-prs](https://github.com/cuedown/FreeSSM/tree/community-prs) branch now.

**credit:** Comer352L and everyone who built FreeSSM. R-Peerayot for spotting the 32-bit fix first. the PR authors above.

the original readme is in [README.txt](README.txt). license: GPL-3.0, same as upstream.
