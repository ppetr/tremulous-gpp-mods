Antifeed
========
Each player has a new variable holding so called *spawn points*. The variable
ranges from 0 to /g_spawnLimitBuffer/. Spawning is possible only when the value
is greater than 0. Each time the player spawns (s)he spends one of these
points. The points replenish over time with the rate 1 point per
/g_spawnLimitTime/ seconds. At the beginning each player has the maximum
possible number of spawn points.

This prevents excesive feeding of newbies, but still allows a player to
spawn several times quickly, if (s)he hasn't fed before (for example
when defending a base and dying several times in a short time).

Implementation note: The implementation actually doesn't use the concept of
spawn points, it uses a special internal timer, but the result is as
described above.
