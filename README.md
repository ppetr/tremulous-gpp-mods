Anti-camping mod
================

When a player is killed, the attackers gain credit bonus for each defensive structure (of the corresponding team) near the victim. This gives an advantage to a team that attacks their camping oponents.

The reward is controlled by 6 variables:
 - *g_humanAnticampRange* specifies the range in which the rets/teslas are counted around a human victim.
 - *g_humanAnticampBonus1* an additional reward multiplier given to the attacker when a human is killed near 1 turret (or 1 tesla).
 - *g_humanAnticampBonusMax* an additional reward multiplier given to the attacker when a human is killed near 10 or more turrets/teslas. 
 - When there are *1 < N < 10* defensive structures in the range, the bonus is interpolated appropriately between *g_humanAnticampBonus1* and *g_humanAnticampBonusMax*.
 - There are similar varibles for aliens: *g_alienAnticampBonusMax, g_alienAnticampBonus1, g_alienAnticampRange*.

This also means that the team that starts a pressing attack will very likely win, giving less chance to the defenders, even if they weren't camping.

Note that this applies to aliens too, so a building granger killed near acids gives even more evo's than usual.
