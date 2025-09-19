/*

Copyright (c) 2021-22 Axiado Corporation (or its affiliates). All rights reserved.
Use, modification and redistribution of this file is subject to your possession
of a valid End User License Agreement (EULA) for the Axiado Product of which
these sources are part of and your compliance with all applicable terms and
conditions of such licence agreement.
*/

/* includes */
#include <common.h>
#include <config.h>
#include <dm.h>
#include <environment.h>
#include <init.h>
#include <memalign.h>
#include <asm/global_data.h>
#include <asm/armv8/mmu.h>
#include <asm/io.h>
/* Global variables */
static struct mm_region axiado_ax200_mem_map[] = {
       {
                .virt = 0x3C000000UL,
                .phys = 0x3C000000UL,
                .size = 0x02000000UL,
                .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
                         PTE_BLOCK_INNER_SHARE 
        }, {
                0,
        }
};

struct mm_region *mem_map = axiado_ax200_mem_map;

#ifdef CONFIG_MISC_INIT_R
/**
 * @brief Configure Board Specific parts
 *
 * @param void
 *
 * @return int
 */
int misc_init_r(void)
{
	return 0;
}
#endif

/**
 * @brief Name of the device tree to be loaded from the Flattened Image Tree (FIT)
 *        (Not used) 
 *
 * @param void
 *
 * @return void
 */
int board_fit_config_name_match(const char *name) 
{
	return 0;
}

/**
 * @brief board specific dram initialization 
 *
 * @param void
 *
 * @return void
 */
int dram_init(void)
{
	gd->ram_size = get_ram_size(0, mem_map[0].size);
	return 0;
}

void raw_write_cntfrq_el0(u32 cntfrq_el0)
{
	__asm__ __volatile__("msr CNTFRQ_EL0, %0\n\t" : : "r" (cntfrq_el0) : "memory");
}

int timer_init(void)
{
    u32 div0_val = 0, div1_val = 0, counter_frq = 0;
	/* Check if PLL is locked and calculate the timer counter */
	if((readl(CLKRST_CPU_PLL_STS_ADRS_OFFSET)) & CPU_PLL_STS_LOCK){
		u32 reg_val = readl(CLKRST_CPU_PLL_POSTDIV_REG); /* Read the CPU frq regiter */
		div0_val = ((reg_val >> REG_CPU_PLL_POSTDIV_FLD_POSTDIV0_0_LSB) & (POST_DIV_BITS_MASK));
		div1_val = ((reg_val >> REG_CPU_PLL_POSTDIV_FLD_POSTDIV1_0_LSB) & (POST_DIV_BITS_MASK));
		counter_frq = (AX_PLL_CLK_4000MHZ / ((div0_val + 1) * (div1_val + 1))); /* Calculate the counter frq value */
		raw_write_cntfrq_el0(counter_frq);	
	} else {
		/**
		* Control should not reach here...
		* If PLL is bypassed then recalculate the timer counter based on default frequency
		* TODO: KWS-5412-Notify system manager that system failed to boot.
		*/
		return -1;
	}
	writel(SYS_TIMER_ENABLE, SYS_TIMER_CTRL);
	return 0;
}
/**
 * @brief board specific initialization 
 *
 * @param void
 *
 * @return void
 */
int board_init(void)
{
	return 0;
}

/**
 * @brief board specific reset 
 *
 * @param void
 *
 * @return void
 */

void reset_cpu(ulong addr)
{
	return;
}


/**
 * @brief SOC specific CPU Setup 
 *
 * @param void
 *
 * @return void
 */
int mach_cpu_init(void)
{
	return 0;
}
