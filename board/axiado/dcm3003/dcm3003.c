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
#include <command.h>
#include <log.h>
#include <fdt_support.h>

#define MAX_STRING_SIZE 40

/* Forward declarations */
void upgrade_environment(void);

/* Global variables */
static struct mm_region axiado_ax3000_mem_map[] = {
	{
		.virt = 0x3C000000UL,
		.phys = 0x3C000000UL,
		.size = 0x01000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
		PTE_BLOCK_INNER_SHARE
	}, {
		.virt = 0x3D000000UL,
		.phys = 0x3D000000UL,
		.size = 0x23000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
		PTE_BLOCK_INNER_SHARE
	}, {
		.virt = 0x400000000UL,
		.phys = 0x400000000UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
		PTE_BLOCK_INNER_SHARE
	},	{
		0,
	}
};

struct mm_region *mem_map = axiado_ax3000_mem_map;

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
	upgrade_environment();
	return 0;
}

/**
 * @brief verify if the env variable is valid
 *
 * @param var: the env variable to be verified
 *      index: the index of the var
 *
 * @return 0  : Not a valid variable
 *         1  : a valid variable
 */
int validate_var(char *var, int index) {
	const char *bs_valid_vars[] = {"a", "b"};   /* bootside valid values*/
	const char **valid_vars[] = {
		NULL,
		bs_valid_vars,
		/* Add more variables to be valided here */
	};
	const int number_valid_vars[] = {0, 2};
	const int max_var_sizes[] = {0, 2};
	const char **valid_str = valid_vars[index];
	int i;
	if (!var) {
		return 0;
	}
	if (!valid_str) {
		return 1;  /* valid */
	}
	/* check all valid vars */
	for (i = 0; i < number_valid_vars[index]; i++) {
		if (strncmp(var, valid_str[i], max_var_sizes[index]) == 0) {
			return 1;
		}
	}
	return 0;
}

/* structure for Force Replace Variables */
typedef struct force_vars {
	char* force_replace_var;
	char* default_var;
} force_vars_t;

/* structure for Preserved Variables */
typedef struct preserve_vars {
	const char* preserve_var;
	const char* default_var;
	char preserved_value[MAX_STRING_SIZE];
} preserve_vars_t;

void upgrade_environment(void)
{
	/* Variables to force replace with default values */
	static const force_vars_t force_vars[] = {
		{"bootcmd", "ax3000_secure_boot"},
		{"bootside", "a"},
		/* Add more variables to here */
	};

	/* Variables to preserve (keep user values) during upgrade */
	static preserve_vars_t preserve_vars[] = {
		{"fdt_conf", "", {0}},
		{"bootside", "a", {0}},
		/* Add more variables to preserve here */
	};

	int i;
	const char *env_version_str;
	unsigned long env_version;
	int needs_full_reset = 0;
	char env_version_buf[16];
	char *temp;

	/* Check current env_version */
	env_version_str = env_get("env_version");
	if (env_version_str) {
		env_version = simple_strtoul(env_version_str, NULL, 10);
		if (env_version < AX3000_ENV_VERSION) {
			/* Environment is outdated, need full reset */
			needs_full_reset = 1;
			printf("Environment version %lu is outdated (current: %u), upgrading...\n",
			       env_version, AX3000_ENV_VERSION);
		} else {
			/* Environment is up to date, only reset bootcmd */
			needs_full_reset = 0;
			debug("Environment version %lu is up to date (current: %u), resetting bootcmd only\n",
			      env_version, AX3000_ENV_VERSION);
		}
	} else {
		/* env_version not set, treat as outdated */
		needs_full_reset = 1;
		printf("Environment version not set, upgrading...\n");
	}

	for (i = 0; i < sizeof(preserve_vars) / sizeof(preserve_vars[0]); i++) {
		temp = env_get(preserve_vars[i].preserve_var);
		if (temp) {
			snprintf(preserve_vars[i].preserved_value,
				sizeof(preserve_vars[i].preserved_value), "%s", temp);
		} else {
			preserve_vars[i].preserved_value[0] = '\0';
		}
		/* validate the perserved_value */
		if (!validate_var(temp, i)) {
			snprintf(preserve_vars[i].preserved_value,
				sizeof(preserve_vars[i].preserved_value),"%s",
				preserve_vars[i].default_var);
		}
		if (strnlen(preserve_vars[i].preserved_value, 1)) {
			debug("Preserving %s: %s\n", preserve_vars[i].preserve_var,
					preserve_vars[i].preserved_value);
		}
	}

	if (needs_full_reset) {
		/* Force reset all environment variables to defaults */
		set_default_env(NULL, 0);

		/* Restore preserved user variables */
		for (i = 0; i < sizeof(preserve_vars) / sizeof(preserve_vars[0]); i++) {
			if (strnlen(preserve_vars[i].preserved_value, 1)) {
				env_set(preserve_vars[i].preserve_var, preserve_vars[i].preserved_value);
				debug("Restored preserved %s: %s\n", preserve_vars[i].preserve_var,
						preserve_vars[i].preserved_value);
			}
		}

		/* Set env_version to current version */
		snprintf(env_version_buf, sizeof(env_version_buf), "%d", AX3000_ENV_VERSION);
		env_set("env_version", env_version_buf);

		/* Save environment */
		env_save();
		printf("Environment upgraded successfully.\n");
	} else {
		/* Set the var to be default if it is needed */
		int need_to_save = 0;

		for ( i= 0; i < sizeof(force_vars) / sizeof(force_vars[0]); i++) {
			/* Save only if we need to save */
			temp = env_get(force_vars[i].force_replace_var);
			/* Check if it is already a default value */
			if (!temp || strncmp(temp, force_vars[i].default_var, MAX_STRING_SIZE) != 0) {
				/* skip for checking bootcmd */
				if (i == 0 || !validate_var(temp, i)) {
					/* Force set it */
					env_set(force_vars[i].force_replace_var, force_vars[i].default_var);
					need_to_save = 1;
				}
			}
		}
		if(need_to_save) {
			env_save();
			debug("reset to default.\n");
		}
	}
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

/**
 * @brief Set up DRAM bank sizes for device tree fixup
 *
 * - Region 1: 0x3C000000, size 0x1000000 (16MB)
 * - Region 2: 0x3D000000, size 0x23000000 (560MB)
 * - Region 3: 0x400000000, size 0x80000000 (2GB)
 *
 * @return 0 on success
 */
int dram_init_banksize(void)
{
	/* Memory region 1: 0x3C000000, size 0x1000000 (16MB) */
	gd->bd->bi_dram[0].start = 0x3C000000ULL;
	gd->bd->bi_dram[0].size = 0x1000000ULL;

	/* Memory region 2: 0x3D000000, size 0x23000000 (560MB) */
	gd->bd->bi_dram[1].start = 0x3D000000ULL;
	gd->bd->bi_dram[1].size = 0x23000000ULL;

	/* Memory region 3: 0x400000000, size 0x80000000 (2GB) */
	gd->bd->bi_dram[2].start = 0x400000000ULL;
	gd->bd->bi_dram[2].size = 0x80000000ULL;

	return 0;
}

/**
 * @brief Board-specific FDT fixup to restore correct cpu-release-addr
 *
 * arch_fixup_fdt() calls spin_table_update_dt() which overwrites
 * cpu-release-addr with U-Boot's internal address. This function
 * restores the correct address (0x3c0013a0) from the device tree
 * source file, which matches what the kernel expects.
 *
 * @param blob: FDT blob
 * @param bd: Board data pointer
 * @return 0 on success
 */
int ft_board_setup(void *blob, bd_t *bd)
{
	int cpus_offset, offset;
	const char *prop;
	int ret;
	u64 cpu_release_addr = 0x3c0013a0; /* From ax3000.dtsi */

	cpus_offset = fdt_path_offset(blob, "/cpus");
	if (cpus_offset < 0)
		return 0; /* No /cpus node, nothing to fix */

	/* Restore cpu-release-addr for all CPU nodes */
	for (offset = fdt_first_subnode(blob, cpus_offset);
	     offset >= 0;
	     offset = fdt_next_subnode(blob, offset)) {
		prop = fdt_getprop(blob, offset, "device_type", NULL);
		if (!prop || strncmp(prop, "cpu", 4))
			continue;

		prop = fdt_getprop(blob, offset, "enable-method", NULL);
		if (!prop || strncmp(prop, "spin-table", 11))
			continue;

		/* Restore the correct cpu-release-addr */
		ret = fdt_setprop_u64(blob, offset, "cpu-release-addr",
				      cpu_release_addr);
		if (ret) {
			printf("WARNING: Failed to restore cpu-release-addr\n");
			return ret;
		}
	}

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

/* AX3000 Image Verification Register Definitions */
#define AX3000_SCRATCHPAD1_ADDR		0x80620804
#define AX3000_SPR1_ADDR		0x80702408
#define AX3000_SPR3_ADDR		0x80802008
#define AX3000_UBOOT_LOADED_IMAGES	0x542
#define AX3000_VERIFY_DONE		0x546
#define AX3000_UBOOT_SUCCESS		0x54d
#define AX3000_VERIFIED_SUCCESS		1
#define AX3000_VERIFY_TIMEOUT_SEC	10

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

/**
 * @brief Check boot count and switch slot if needed
 *
 * @return 0 on success, non-zero on failure
 */
static int ax3000_check_boot_count(void)
{
	const char *bootside;
	const char *bootcount_str;
	const char *bootlimit_str;
	int bootcount, bootlimit;

	bootside = env_get("bootside");
	if (!bootside) {
		env_set("bootside", "a");
		bootside = "a";
	}

	bootcount_str = env_get("bootcount");
	bootcount = bootcount_str ? simple_strtoul(bootcount_str, NULL, 10) : 0;

	bootlimit_str = env_get("bootlimit");
	bootlimit = bootlimit_str ? simple_strtoul(bootlimit_str, NULL, 10) : 3;

	if (bootcount >= bootlimit) {
		/* Switch slot */
		if (strncmp(bootside, "a", 2) == 0) {
			env_set("bootside", "b");
			printf("Switching to slot B\n");
		} else {
			env_set("bootside", "a");
			printf("Switching to slot A\n");
		}
		env_set("bootcount", "0");
		env_save();
	} else {
		/* Increment boot count */
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", bootcount + 1);
		env_set("bootcount", buf);
		env_save();
	}

	return 0;
}

/**
 * @brief Load FIT image from storage
 *
 * This function loads a FIT image using the 'load' command, which automatically
 * detects the filesystem type (FAT, ext4, etc.) and handles the file loading.
 *
 * @param storage_type: Storage type ("mmc" for eMMC, etc.)
 * @param dev_part: Device and partition string (e.g., "0:2" for mmc 0:2)
 * @param filename: Path to the FIT image file
 * @param loadaddr: Address to load the image
 * @return 0 on success, non-zero on failure
 */
static int ax3000_load_fit_image(const char *storage_type, const char *dev_part,
				  const char *filename, ulong loadaddr)
{
	int ret;
	char load_cmd[256];
	const char *filesize_str;

	printf("Loading FIT image from %s %s:%s...\n", storage_type, dev_part, filename);

	/* Use the 'load' command (same as original fatload/ext4load) */
	snprintf(load_cmd, sizeof(load_cmd), "load %s %s 0x%08lx %s",
		 storage_type, dev_part, loadaddr, filename);

	ret = run_command(load_cmd, 0);
	if (ret) {
		printf("WARNING: Failed to load file %s\n", filename);
		return ret;
	}

	/* Get file size from environment (set by load command) */
	filesize_str = env_get("filesize");
	if (!filesize_str) {
		printf("WARNING: Could not determine loaded file size\n");
		return -EINVAL;
	}

	printf("Loaded %s bytes to 0x%08lx\n", filesize_str, loadaddr);
	return 0;
}

/**
 * @brief Boot FIT image from eMMC with dual-boot slot support
 *
 * This function implements the boot logic that was previously done by
 * bootcmd_sa_itb and bootcmd_sb_itb shell scripts. It:
 * 1. Checks boot count and switches slots if needed
 * 2. Loads FIT image from the appropriate slot (A or B)
 * 3. Handles DTB configuration selection
 * 4. Boots the image using bootm
 *
 * @return 0 on success, non-zero on failure
 */
int ax3000_boot_fit_image(void)
{
	const char *bootside;
	const char *bootargs;
	const char *fdt_conf;
	const char *image_path;
	const char *loadaddr_str;
	ulong loadaddr;
	ulong bootm_addr;
	char args_str[256];
	char bootm_cmd[256];
	int ret;
	int mmc_dev = 0;
	int mmc_part;
	char dev_part_str[16];

	/* Check DTB configuration */
	fdt_conf = env_get("fdt_conf");
	if (!fdt_conf || strnlen(fdt_conf, 1) == 0) {
		printf("\n");
		printf("WARNING: No DTB configuration set!\n");
		printf("Please use 'run show_dtbs' to see available configurations\n");
		printf("Then set one with 'setenv fdt_conf <config-name>'\n");
		printf("\n");
		return -EINVAL;
	}

	printf("Using DTB configuration: %s\n", fdt_conf);

	/* Check boot count and switch slot if needed */
	ret = ax3000_check_boot_count();
	if (ret)
		return ret;

	/* Get current boot side */
	bootside = env_get("bootside");
	if (!bootside) {
		bootside = "a";
		env_set("bootside", "a");
	}

	/* Get image path */
	image_path = env_get("image_path");
	if (!image_path)
		image_path = "fitImage";

	/* Get load address */
	loadaddr_str = env_get("loadaddr");
	if (!loadaddr_str) {
		loadaddr = CONFIG_SYS_LOAD_ADDR;
	} else {
		loadaddr = simple_strtoul(loadaddr_str, NULL, 16);
	}

	/* Determine partition based on boot side */
	if (strncmp(bootside, "a", 2) == 0) {
		mmc_part = 2;
		printf("Loading signed images from slot A...\n");
	} else if (strncmp(bootside, "b", 2) == 0) {
		mmc_part = 3;
		printf("Loading signed images from slot B...\n");
	} else {
		/* Boot from slot A by default */
		bootside = "a";
		mmc_part = 2;
		printf("Loading signed images from slot A...\n");
	}

	/* Append to current bootargs */
	bootargs = env_get("bootargs");
	snprintf(args_str, sizeof(args_str), "%s rootwait root=PARTLABEL=rofs-%s", bootargs, bootside);
	env_set("bootargs", args_str);

	/* Load FIT image from eMMC */
	snprintf(dev_part_str, sizeof(dev_part_str), "%d:%d", mmc_dev, mmc_part);
	ret = ax3000_load_fit_image("mmc", dev_part_str, image_path, loadaddr);
	if (ret) {
		/* Try legacy name */
		ret = ax3000_load_fit_image("mmc", dev_part_str, "fitImage.asi", loadaddr);
		if (ret)
			return ret;
	}

	/* Calculate bootm address (loadaddr + 0x10) */
	bootm_addr = loadaddr + 0x10;
	printf("Loading signed fitImage using bootm...\n");

	/* Build bootm command with DTB configuration */
	snprintf(bootm_cmd, sizeof(bootm_cmd), "bootm 0x%08lx#%s", bootm_addr, fdt_conf);

	/* Execute bootm command */
	printf("Executing: %s\n", bootm_cmd);
	ret = run_command(bootm_cmd, 0);

	return ret;
}

/**
 * @brief Command handler for ax3000_secure_boot command
 *
 * @param cmdtp: Command table entry
 * @param flag: Command flags
 * @param argc: Argument count
 * @param argv: Argument vector
 * @return 0 on success, non-zero on failure
 */
static int do_ax3000_secure_boot(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	return ax3000_boot_fit_image();
}

U_BOOT_CMD(
	ax3000_secure_boot, 1, 1, do_ax3000_secure_boot,
	"Boot FIT image from eMMC with dual-boot slot support",
	""
);
