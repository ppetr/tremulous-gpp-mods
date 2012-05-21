Antifeed
========
Feed protection timer. Each player has to wait g_spawnLimitTime before
each spawn. However, by staying alive, the player can accumulate up to
g_spawnLimitBuffer spawns, which are not constrained by the limit.
    
This prevents excesive feeding of newbies, but still allows a player to
spawn several times quickly, if (s)he hasn't fed before (for example
when defending a base).
