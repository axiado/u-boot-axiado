// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021-25 Axiado Corporation (or its affiliates). All rights reserved.
 */

#ifndef	__AX3000_EVK_H
#define __AX3000_EVK_H

#include <linux/sizes.h>

#define CONFIG_ARMV8_SWITCH_TO_EL1

#define CONFIG_GICV3
#define GICD_BASE		0x80300000
#define GICR_BASE		0x80380000


#define SYS_TIMER_CTRL   0x8A020000
#define SYS_TIMER_ENABLE    0x1
#define CLKRST_CPU_PLL_POSTDIV_REG 0x8000000C
#define CLKRST_CPU_PLL_STS_ADRS_OFFSET 0x80000014
#define REG_CPU_PLL_POSTDIV_FLD_POSTDIV0_0_LSB 0
#define REG_CPU_PLL_POSTDIV_FLD_POSTDIV1_0_LSB 12
#define POST_DIV_BITS_MASK 0x7
#define CPU_PLL_STS_LOCK 0x1
#define AX_PLL_CLK_4000MHZ			4000000000 



#define CONFIG_NR_CPUS			4
/* This address points to a start address in the DDR */
#define CONFIG_SYS_SDRAM_BASE		0x3C000000
#define CONFIG_SYS_SDRAM_SIZE 		32 /* Reserve 32M */
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + SZ_1M)
#define CONFIG_SYS_MEMTEST_START	CONFIG_SYS_SDRAM_BASE
#define CONFIG_SYS_MEMTEST_END          ((CONFIG_SYS_SDRAM_SIZE - 3) << 20)

#define CONFIG_SYS_LOAD_ADDR		0x3D000000

#define CONFIG_SYS_CBSIZE		SZ_1K
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
		sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_MAXARGS		64
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE

#ifndef CONFIG_CPU_FREQ_HZ
# define CONFIG_CPU_FREQ_HZ 10000000
#endif
#define CONFIG_SYS_MALLOC_LEN  (128 << 10) /* Reserve 128 kB for malloc() */
/* Serial Options */
#define CONFIG_CPU_ARMV8

#define CONFIG_SYS_BAUDRATE_TABLE \
	{ 4800, 9600, 19200, 38400, 57600, 115200 }

/* for bigger files */
#define CONFIG_SYS_BOOTM_LEN	0x2000000

/* Define secure and unsecure operation modes */
#define ENV_SECURE_OPS 0
#define ENV_UNSECURE_OPS 1

/* Current operation mode - set to unsecure for now */
#define CURRENT_OPS_MODE ENV_UNSECURE_OPS

/* Dual-boot configuration */
#define ENV_DUAL_BOOT_SETTINGS \
    "bootside=a\0" \
    "bootcount=0\0" \
    "bootlimit=3\0" \
    \
    /* Define slot-specific boot commands */ \
    "bootcmd_a=" \
        "echo \"Loading FIT image from slot A...\"; " \
        "fatload mmc 0:2 ${fit_addr_r} ${image_path}; " \
        "bootm ${fit_addr_r}#${fdt_conf}\0" \
    \
    "bootcmd_b=" \
        "echo \"Loading FIT image from slot B...\"; " \
        "fatload mmc 0:3 ${fit_addr_r} ${image_path}; " \
        "bootm ${fit_addr_r}#${fdt_conf}\0" \
    \
    /* Boot count check and slot switching */ \
    "check_boot_count=" \
        "if test ${bootcount} -ge ${bootlimit}; then " \
            "run switch_slot; " \
        "else " \
            "setexpr bootcount ${bootcount} + 1; " \
            "saveenv; " \
        "fi\0" \
    \
    "switch_slot=" \
        "if test ${bootside} = a; then " \
            "setenv bootside b; " \
            "echo \"Switching to slot B\"; " \
        "else " \
            "setenv bootside a; " \
            "echo \"Switching to slot A\"; " \
        "fi; " \
        "setenv bootcount 0; " \
        "saveenv\0"

/* Basic environment settings that are common to both secure and unsecure modes
 * Update the boot-up done event by writing the 0x80620804 register
 * with 0x54d (UBOOT boot-up successful) value.
 */
#define ENV_BASIC_ENV_INFO \
    "bootm_size=0x20000000\0" \
    "fdtaddr=0x3EF00000\0" \
    "loadaddr=0x3D000000\0" \
    "scratchpad1=80620804\0" \
    "ubootsuccess=54d\0" \
    "ubootstatus=mw.l ${scratchpad1} ${ubootsuccess}\0" \
    "bootk=run ubootstatus; booti ${loadaddr} - ${fdtaddr}\0" \
    "newloadaddr=0x4D000000\0" \
    "bootgz=run ubootstatus; unzip ${loadaddr} ${newloadaddr}; booti ${newloadaddr} - ${fdtaddr}\0"

/* Secure operation settings for future use */
#define ENV_SECURE_OPS_SETTINGS \
    "secure_boot=1\0" \
    /* Additional secure boot parameters will be added here in the future */ \
    "default_bootcmd=echo \"Booting in secure mode\"\0"

/* DTB configuration helper commands */
#define ENV_DTB_HELPER_SETTINGS \
    /* Command to show available DTB configurations */ \
    "show_dtbs=" \
        "echo \"Loading FIT image to check available DTB configurations...\"; " \
        "if fatload mmc 0:2 ${fit_addr_r} ${image_path}; then " \
            "echo \"\"; " \
            "echo \"Available DTB configurations:\"; " \
            "echo \"---------------------------\"; " \
            "fdt addr ${fit_addr_r}; " \
            "fdt list /configurations; " \
            "echo \"\"; " \
            "echo \"Current configuration: ${fdt_conf}\"; " \
            "echo \"\"; " \
            "echo \"To change configuration: setenv fdt_conf <config-name>\"; " \
            "echo \"Then save with: saveenv\"; " \
        "else " \
            "echo \"Error: Could not load FIT image from current slot.\"; " \
            "echo \"Make sure bootable media is inserted.\"; " \
        "fi\0" \
    \
    /* Command to set a DTB configuration */ \
    "set_dtb=" \
        "if test $# -eq 1; then " \
            "setenv fdt_conf $1; " \
            "saveenv; " \
            "echo \"Set DTB configuration to: $1\"; " \
            "echo \"This will be used on next boot.\"; " \
        "else " \
            "echo \"Usage: run set_dtb <config-name>\"; " \
            "echo \"Available configurations can be viewed with: run show_dtbs\"; " \
        "fi\0" \
    \
    /* Command to check if DTB configuration is set */ \
    "check_dtb=" \
        "if test -z \"${fdt_conf}\"; then " \
            "echo \"\"; " \
            "echo \"WARNING: No DTB configuration set!\"; " \
            "echo \"Please use 'run show_dtbs' to see available configurations\"; " \
            "echo \"Then set one with 'run set_dtb <config-name>'\"; " \
            "echo \"\"; " \
            "false; " \
        "else " \
            "echo \"Using DTB configuration: ${fdt_conf}\"; " \
            "true; " \
        "fi\0"

/* Unsecure operation settings */
#define ENV_UNSECURE_OPS_SETTINGS \
    "fit_addr_r=0x3C100000\0" \
    "image_path=/fitImage\0" \
    "bootcmd=" \
        "run ubootstatus; " \
        "run check_dtb; " \
        "if test $? -eq 0; then " \
            "run check_boot_count; " \
            "if test ${bootside} = a; then " \
                "run bootcmd_a; " \
            "else " \
                "run bootcmd_b; " \
            "fi; " \
        "fi\0" \
    "secure_boot=0\0" \
    "default_bootcmd=run bootcmd\0"

/* Conditionally include settings based on board type and operation mode */
#if defined(CONFIG_AX3000_EVK)
  #if (CURRENT_OPS_MODE == ENV_SECURE_OPS)
    #define CONFIG_EXTRA_ENV_SETTINGS \
      ENV_BASIC_ENV_INFO \
      ENV_SECURE_OPS_SETTINGS \
      ""
  #else
    #define CONFIG_EXTRA_ENV_SETTINGS \
      ENV_BASIC_ENV_INFO \
      ENV_DUAL_BOOT_SETTINGS \
      ENV_DTB_HELPER_SETTINGS \
      ENV_UNSECURE_OPS_SETTINGS \
      ""
  #endif
  #ifndef  U_BOOT_DMI_DATE
    #define U_BOOT_DMI_DATE "04/01/2019"
  #endif
#else
/* Define other settings if needed */
#endif

/* Conditional bootcommand selection based on configuration */
#ifdef CONFIG_COMPRESSED_KERNEL_SUPPORT
    #define CONFIG_BOOTCOMMAND "run bootgz"
#else
    /* Use the default_bootcmd for standard boot */
    #define CONFIG_BOOTCOMMAND "run default_bootcmd"
#endif

/*
MEM_LAYOUT_ENV_SETTINGS
*/

/* MMC ENV related defines */
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV      0
#define CONFIG_SYS_MMC_ENV_PART     0
#endif

#endif /* __AX3000_EVK_H */
