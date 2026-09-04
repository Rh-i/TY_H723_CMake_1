/* 烧录工具直接跳入 Init，不使用标准 C 运行库启动入口。 */
ENTRY(Init)

MEMORY
{
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* RAM 向量表的初始 MSP；栈从 SRAM1 顶部向下增长。 */
__loader_stack_top__ = ORIGIN(RAM) + LENGTH(RAM);

PHDRS
{
  /* 将只读可执行段和可写数据段分开，避免生成 RWX 装载段。 */
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  .isr_vector :
  {
    /* Cortex-M7 VTOR 需要按向量表规模对齐。 */
    . = ALIGN(1024);
    KEEP(*(.isr_vector))
  } >RAM :text

  .text :
  {
    . = ALIGN(4);
    *(.text*)
    *(.rodata*)
    KEEP(*(.loader_api))
    . = ALIGN(4);
  } >RAM :text

  .Dev_Info :
  {
    /* 烧录工具按固定段名查找 StorageInfo。 */
    . = ALIGN(4);
    KEEP(*(.Dev_Info))
  } >RAM :text

  .data :
  {
    . = ALIGN(4);
    *(.data*)
    . = ALIGN(4);
  } >RAM :data

  .bss (NOLOAD) :
  {
    /* Init() 使用这两个符号手动清零 BSS。 */
    . = ALIGN(4);
    __bss_start__ = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
  } >RAM :data

  .stack (NOLOAD) :
  {
    /* 在链接布局中为运行栈显式保留 4 KiB。 */
    . = ALIGN(8);
    . += 0x1000;
  } >RAM :data

  /DISCARD/ :
  {
    *(.comment*)
    *(.note*)
  }
}
