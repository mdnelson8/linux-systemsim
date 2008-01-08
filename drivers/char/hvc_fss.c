/*
 * IBM Full System Simulator driver interface to hvc_console.c
 *
 * (C) Copyright IBM Corporation 2001-2005
 * Author(s): Maximino Augilar <IBM STI Design Center>
 *          : Ryan S. Arnold <rsa@us.ibm.com>
 *
 *    inspired by drivers/char/hvc_console.c
 *    written by Anton Blanchard and Paul Mackerras
 *
 * Some code is from the IBM Full System Simulator Group in ARL.
 * Author: Patrick Bohrer <IBM Austin Research Lab>
 *
 * Much of this code was moved here from the IBM Full System Simulator
 * Bogus console driver in order to reuse the framework provided by the hvc
 * console driver. Ryan S. Arnold <rsa@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/console.h>

#include <asm/prom.h>
#include <linux/irq.h>
#include <asm/rtas.h>
#include <asm/systemsim.h>
#include <asm/udbg.h>

#include "hvc_console.h"

#define SIM_WRITE_CONSOLE_CODE 0
#define SIM_READ_CONSOLE_CODE 60

#define hvc_fss_cookie 0x1aa70e29

static int hvc_fss_write_console(uint32_t vtermno, const char *buf, int count)
{
	int ret;
	ret = callthru3(SIM_WRITE_CONSOLE_CODE, (unsigned long)buf, count, 1);
	if (ret != 0)
		return (count - ret); /* is this right? */

	/* the calling routine expects to receive the number of bytes sent */
	return count;
}

static int hvc_fss_read_console(uint32_t vtermno, char *buf, int count)
{
	unsigned long got;
	int c, i;

	got = 0;
	for (i = 0; i < count; i++) {
		c = callthru0(SIM_READ_CONSOLE_CODE);
		if (c != -1) {
			buf[i] = c;
			++got;
		} else
			break;
	}
	return got;
}

#ifdef CONFIG_PPC_EARLY_DEBUG_FSS
void udbg_fss_real_putc(char c)
{
	callthru3(SIM_WRITE_CONSOLE_CODE, (unsigned long)&c, 1, 1);
}

void __init udbg_init_fss(void)
{
	udbg_putc = udbg_fss_real_putc;
	udbg_getc = NULL;
	udbg_getc_poll = NULL;
}
#endif /* CONFIG_PPC_EARLY_DEBUG_FSS */

static struct hv_ops hvc_fss_get_put_ops = {
	.get_chars = hvc_fss_read_console,
	.put_chars = hvc_fss_write_console,
};

static int __init hvc_fss_init(void)
{
	struct hvc_struct *hp;

	/* Allocate an hvc_struct for the console device we instantiated
	 * earlier.  Save off hp so that we can return it on exit */
	hp = hvc_alloc(hvc_fss_cookie, NO_IRQ, &hvc_fss_get_put_ops, 16);
	if (IS_ERR(hp))
		return PTR_ERR(hp);

	return 0;
}
module_init(hvc_fss_init);

/* This will happen prior to module init.  There is no tty at this time? */
static int __init hvc_fss_console_init(void)
{
	/* Don't register if we aren't running on the simulator */
	if (!of_find_node_by_path("/systemsim"))
		return -ENODEV;

#ifdef CONFIG_HVC_RTAS
	/* Also don't register if rtas console is implemented by
	 * systemsim, so we don't get duplicate output */
	if (rtas_token("put-term-char") != RTAS_UNKNOWN_SERVICE)
		return -EBUSY;
#endif

	/* Tell the driver we know of one console device.  We
	 * shouldn't get a collision on the index as long as no-one
	 * else instantiates on hardware they don't have. */
	hvc_instantiate(hvc_fss_cookie, 0, &hvc_fss_get_put_ops );
	add_preferred_console("hvc", 0, NULL);

	return 0;
}
console_initcall(hvc_fss_console_init);
