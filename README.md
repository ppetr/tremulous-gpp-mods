A combined mod
==============

Currently contains:
 - The Granger dance;
 - Scaring/nade;
 - Disables grangers after Sudden death (with an exception when there is no OM).
 - Defence computer limits. Two new CVARs are added: *g_humanDefenceComputerRate* allows setting the repair rate of defence computers; *g_humanDefenceComputerLimit* sets a limit on the number of defence computers that can be repairing a single building. This means that no building will be repaired faster than *g_humanDefenceComputerLimit × g_humanDefenceComputerRate*.
 - List team' evos by issuing a new command *listteam*. Also visible in the team overview (after '$' sign).
 - A human touching a poisoned teammate will get poisoned too (with the same duration). The idea is to make camping harder for humans - they'll transfer the poison to their camping teammates. Most likely this needs some testing and evaluation to see if it's not too anoying. Touching a booster poisons a human in the same way as when bitten by a poisonous alien.
 - Basilisk enhancements: They are very resistant to flamers. Advanced ones more than regular ones. Swiping poisons humans even without being boosted (but for a shorter duration).
