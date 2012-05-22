Defence Computer Limits
=======================
Two new CVARs are added:
 - *g_humanDefenceComputerRate* allows setting the repair rate of defence computers.
 - *g_humanDefenceComputerLimit* sets a limit on the number of defence computers that can be repairing a single building. This means that no building will be repaired faster than *g_humanDefenceComputerLimit × g_humanDefenceComputerRate*.
