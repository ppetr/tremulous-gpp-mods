A combined mod
==============

Currently contains:
 - The Granger dance;
 - Scaring/nade;
 - Disables grangers after Sudden death (with an exception when there is no OM).
 - Antifeed. Each player has to wait *g_spawnLimitTime* before each spawn. However, by staying alive, the player can accumulate up to *g_spawnLimitBuffer* spawns, which are not constrained by the limit.
 - Defence computer limits. Two new CVARs are added: *g_humanDefenceComputerRate* allows setting the repair rate of defence computers; *g_humanDefenceComputerLimit* sets a limit on the number of defence computers that can be repairing a single building. This means that no building will be repaired faster than *g_humanDefenceComputerLimit × g_humanDefenceComputerRate*.
 - List team' evos by issuing a new command *listteam*. Also visible in the team overview (after '$' sign).
