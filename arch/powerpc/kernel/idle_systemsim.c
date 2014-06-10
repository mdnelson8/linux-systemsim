/*
 * Idle daemon for the IBM Full System Simulator.
 *
 * Originally Written by Cort Dougan (cort@cs.nmt.edu)
 * Updated for use with ppc_md.power_save by Mark nelson (nelsondm@us.ibm.com)
 *    Copyright (c) 2003-2014 IBM Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/sched.h>
#include <linux/preempt.h>

#include <asm/systemsim.h>

#define SIM_HALT_CODE 126

static inline void systemsim_halt(void)
{
	callthru0(SIM_HALT_CODE);
}

void systemsim_idle(void)
{
	systemsim_halt();
}
