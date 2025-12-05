// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021-25 Axiado Corporation (or its affiliates).
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
#include <image.h>
#include <bootm.h>
#include <mapmem.h>
#include <linux/delay.h>

/* Global variables */
static struct mm_region axiado_mem_map[] = {
       {
                .virt = 0x3C000000UL,
                .phys = 0x3C000000UL,
                .size = 0x04000000UL,
                .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
                         PTE_BLOCK_INNER_SHARE 
        }, {
                0,
        }
};

struct mm_region *mem_map = axiado_mem_map;

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
	u32 div0_val = 0, div1_val = 0;
	u64 counter_frq = 0;
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

/**
 * @brief Verify FIT image using hardware verification mechanism
 *
 * This function performs hardware-based verification of a FIT image by:
 * 1. Determining the original file address (img_addr may be loadaddr + 0x10)
 * 2. Getting the file size from environment or calculating from FIT
 * 3. Writing the image size to SPR1 register
 * 4. Writing status to scratchpad1 to trigger verification
 * 5. Polling for verification completion
 * 6. Reading verification result from SPR3
 *
 * @param img_addr: Address passed to bootm (may be loadaddr + 0x10)
 * @param img_size: Unused (calculated internally)
 *
 * @return 0 on success, non-zero on failure
 */
int board_verify_fit_image(ulong img_addr, ulong img_size)
{
	u32 spr_val;
	u8 spr3_val;
	int count;
	const void *fit;
	ulong verify_addr;
	ulong file_size;

	/* Determine original file address - img_addr may be loadaddr + 0x10 */
	/* For hardware verification, we need the original loadaddr where the file starts */
	ulong loadaddr_env = env_get_hex("loadaddr", 0);
	if (loadaddr_env && loadaddr_env >= CONFIG_SYS_SDRAM_BASE &&
	    img_addr == loadaddr_env + 0x10) {
		verify_addr = loadaddr_env;
		debug("Using original loadaddr 0x%08lx (img_addr was 0x%08lx)\n",
		      verify_addr, img_addr);
	} else {
		verify_addr = img_addr;
	}

	/* Ensure verify_addr is within valid memory range */
	if (verify_addr < CONFIG_SYS_SDRAM_BASE ||
	    verify_addr >= (CONFIG_SYS_SDRAM_BASE + (CONFIG_SYS_SDRAM_SIZE << 20))) {
		printf("ERROR: board_verify_fit_image: Invalid address 0x%08lx\n", verify_addr);
		return -EINVAL;
	}

	/* Check FIT format at img_addr (which we know is valid) */
	fit = map_sysmem(img_addr, 0);
	if (!fit_check_format(fit)) {
		/* Not a FIT image - this is an error since we expect FIT format */
		printf("ERROR: board_verify_fit_image: Not a valid FIT image at 0x%08lx\n",
		       img_addr);
		return -EINVAL;
	}

	/* Get file size from environment (set by fatload) */
	file_size = env_get_hex("filesize", 0);
	if (!file_size) {
		/* Fallback: calculate from FIT size */
		ulong fit_size = fit_get_size(fit);
		file_size = fit_size;
		/* If verify_addr is different from img_addr, add the offset */
		if (verify_addr != img_addr) {
			file_size += 0x10; /* Add header offset */
		}
		debug("Using calculated file size: 0x%08lx\n", file_size);
	}

	/* Always use verify_addr for hardware verification (original file start address) */
	/* This ensures we verify the complete file including any header */
	debug("Verifying FIT image at 0x%08lx (size: 0x%08lx)\n",
	      verify_addr, file_size);

	/* Step 1: Write image size to SPR1 */
	writel(file_size, AX3000_SPR1_ADDR);

	/* Step 2: Write status to scratchpad1 to trigger verification */
	writel(AX3000_UBOOT_LOADED_IMAGES, AX3000_SCRATCHPAD1_ADDR);

	/* Step 3: Poll scratchpad1 until verification is done */
	count = 0;
	while (count < AX3000_VERIFY_TIMEOUT_SEC) {
		spr_val = readl(AX3000_SCRATCHPAD1_ADDR);

		/* Check if verification is done */
		if (spr_val != AX3000_UBOOT_LOADED_IMAGES) {
			debug("Verification done. spr_val = 0x%x\n", spr_val);
			break;
		}

		debug("Waiting for verification... (%d)\n", count);
		mdelay(1000); /* Wait 1 second */
		count++;
	}

	if (count >= AX3000_VERIFY_TIMEOUT_SEC) {
		printf("ERROR: Timed out waiting for verification status.\n");
		return -ETIMEDOUT;
	}

	/* Step 4: Check if verification completed successfully */
	if (spr_val != AX3000_VERIFY_DONE) {
		printf("ERROR: Unexpected verification status: 0x%x\n", spr_val);
		return -EINVAL;
	}

	/* Step 5: Read verification result from SPR3 */
	spr3_val = readb(AX3000_SPR3_ADDR);

	if (spr3_val == AX3000_VERIFIED_SUCCESS) {
		/* Step 6: Write ubootstatus (0x54d) to scratchpad1 after successful verification */
		writel(AX3000_UBOOT_SUCCESS, AX3000_SCRATCHPAD1_ADDR);
		debug("Image verification successful. Updated ubootstatus.\n");
		return 0;
	} else {
		printf("ERROR: Image verification failed. Result: 0x%x\n", spr3_val);
		return -EACCES;
	}
}
