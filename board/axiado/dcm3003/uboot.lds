/*

 Custom linker script specific for Axiado AX3000 SOC
Copyright (c) 2021-22 Axiado Corporation (or its affiliates). All rights reserved.
Use, modification and redistribution of this file is subject to your possession
of a valid End User License Agreement (EULA) for the Axiado Product of which
these sources are part of and your compliance with all applicable terms and
conditions of such licence agreement.
*/

#include <config.h>

MEMORY {
        RAM : ORIGIN = RAM_DATA_BASE, LENGTH = RAM_DATA_SIZE
}

OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
        . = CONFIG_SYS_MONITOR_BASE;
        
		. = ALIGN(8);
		
        .text : {
				*(__image_copy_start)
                CPUDIR/start.o (.text*)
                *(.text*)
        }

        . = ALIGN(8);
        .rodata : {
                *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*)))
        }

        . = ALIGN(8);
        .u_boot_list : {
                KEEP(*(SORT(.u_boot_list*)));

                . = ALIGN(8);
                __rom_end = .;
        }

		. = ALIGN(8);
        .data : {
                __ram_start = .;

                *(.data*)

                __ram_end = .;
        }

        .bss : {
                . = ALIGN(8);
                __bss_start = .;
                *(.bss*)
                __bss_end = .;
        } > RAM

        /* Keep relocation-related symbols */
        __rel_dyn_start = .;
        __rel_dyn_end = .;
        __image_copy_end = .;
        __init_end = .;
}
