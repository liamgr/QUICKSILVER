/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(FAST_RAM) + LENGTH(FAST_RAM);     /* end of 128K RAM */

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x4000;      /* required amount of heap  */
_Min_Stack_Size = 0x4000; /* required amount of stack */

/* Specify the memory areas */
/*
Flash Layout:
0x08000000 - 0x0800C000: isr, rodata, data (Section 0 - Section 2)
0x0800C000 - 0x08010000: configuration, profile (Section 3)
0x08010000 - 0x080FFFFF: code (Section 4 - Section 11)
*/
MEMORY
{
  FLASH (rx)        : ORIGIN = 0x08000000, LENGTH = 48K
  FLASH_CONFIG (r)  : ORIGIN = 0x0800C000, LENGTH = 16K
  FLASH_CODE   (rx) : ORIGIN = 0x08010000, LENGTH = 448K
  FAST_RAM (rw)     : ORIGIN = 0x10000000, LENGTH = 32K
  RAM (xrw)         : ORIGIN = 0x20000000, LENGTH = 96K
}

/* Define output sections */
SECTIONS
{
  .config_flash  :
  {
    _config_flash_start = .;
    KEEP(*(.config_flash))
    _config_flash_end = .;
  } >FLASH_CONFIG

  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH_CODE

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> FLASH

  _fast_ram_data = LOADADDR(.fast_ram);

  .fast_ram :
  {
    . = ALIGN(4);
    _fast_ram_start = .;       /* create a global symbol at fast_ram start */
    *(.fast_ram)
    *(.fast_ram*)
    
    . = ALIGN(4);
    _fast_ram_end = .;       /* create a global symbol at fast_ram end */
  } >FAST_RAM AT> FLASH

  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap :
  {
    . = ALIGN(4);
    _heap_end = .;
    . = . + _Min_Heap_Size;
    . = ALIGN(4);
    _heap_start = .;
  } >RAM

  ._user_stack :
  {
    . = ALIGN(4);
    . = . + _Min_Stack_Size;
    . = ALIGN(4);
  } >FAST_RAM

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
