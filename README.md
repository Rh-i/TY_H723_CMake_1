# README

# todo

小项目先实现：
- USB-HID和CDC

总计实现：
- SPI-Flash
- 测试H7新功能 CORTEX_M7栏目内容
- 移植dsp库
- 写SPI/IIC的驱动
- 多种电机驱动（DJI DM）
- PID控制算法
- 自用0.96寸LCD屏驱动
- 灯 蜂鸣器 按钮 舵机 等等小驱动
- BMI088 / ICM42688 / 国产陀螺仪驱动

## 具体文件树

```txt
root              
│                   │.clang-format 代码格式化设置 可以按照自己风格改
│                   │README.md     本文件
│                   │其余为生成的文件，包括锁等等官方生成的文件
│                   |---
├─.vscode           |里面`launch`要修改`.elf`（调试）
|                   |（如果文件夹名字和工程名字一致，则可以自动找到）
│                   |`settings`根据自己电脑修改工具链位置（clangd）
│                   |`tasks`是终端执行的任务 看自己改不改了
├─build             |---
│  └─Debug          |这里面有一个`compile_commands.json`（clangd）以及编译出的东西
├─cmake             |---
│  └─stm32cubemx    |没病不用动
├─Core              |---
│  ├─Inc            |略
│  └─Src            |略
├─Drivers           |---
├─Flash             |个人写的烧录相关 修改flash的.elf内容（openocd） 有条件可以加除了dap之外的烧录器
├─Middlewares       |官方生成库，FreeRTOS等等
└─User              |---
    ├─Bsp           |板载支持驱动
    ├─Device        |设备层
    ├─Module        |模块层（ 多个设备组合 ）
    ├─Middleware    |中间层
    │  ├─Algorithm  |  算法层
    │  └─Service    |  服务层 
    └─App           |应用层 / c cpp混编接口层

```
> 对这几个层的理解是 板载支持包完全是按照芯片引脚等等信息来写一层封装，在BSP的基础上，一点点的封装出对应的驱动/设备Dvc，然后多个驱动/设备Dvc连接起来，这就是模块层Mod。应用层就是把mod和mid的东西都拎出来，变成具体的任务。算法Alg 服务风svc就是穿插在这里面的。
>
> 也就是说，移植工程只需重写BSP的接口，就可以完整的用上之前的工程。那就得做到尽可能的解耦，以及树状结构，还有良好的封装。

## 使用说明

配置和编译应该是使用CMake官方的按钮，当然大部分时候是CMake会自动配置

配置和编译：CMake处有固定的命令：（自行添加，官方提供了）
- 使用cmake栏中的上半部分的内容也可以
- 配置
- 生成
- 清除重新生成
- 清楚缓存并重新配置 

烧录可以使用VSCode官方的Task，使用的openocd实现。烧录时只需要执行这个任务就可以。也写了JLink的脚本，没测试，但是烧录过ti的没问题(但是当时有点小bug)

调试使用vscode的运行与调试栏

Cortex-Debug可以看rtos的简单运行情况，内存使用情况，查看变量等等

但是后来我们应该可以换到linux下，虽然这些win也可以用：Cortex-Debug + Ozone（调试 JLink DAP适配没JLink好）+ LinkScope + Systemviewer/FreeMaster（RTOS相关）

## 如何开发

1. 修改成适合自己电脑的一些配置文件，默认配置就是相应的文件和主文件夹名称一样（使用的主文件夹名称作为的索引）
2. 编译烧录调试都使用vscode中的内容，后期可以使用外部调试工具（但是没有keil了）

### 因为底层驱动涉及cpp的class等东西，有两种解决方案

1. 让main.c以cpp格式编译，所有问题不需要管，但是你只能在官方生成的代码里面的，main.cpp中进行操作，如果使用freertos同理
2. 严格在每一个.c .cpp中写接口转接文档，每个不能调用的东西都进行接口转接。

我这边选择的第二个（第一个的问题是每一次都得改，以及可能会有兼容性问题）

写接口转接文档，发现在嵌入式中，只需要给`main.c`进行转接，使用`api_main`来当作提取出来的main即可。这样写的中断回调 初始化 while循环都没啥问题，FreeRTOS也是这样，只需要`extern "C"` 就可以了

HAL库也早就写好了cpp调用c的`extern "C"`内容

作为c调用cpp的转接cpp文件的就写`api_xxxx.cpp`和`api_xxx.h` 作为区分 用**`h`**

正常cpp文件为：`xxx_yyyy.cpp` 和 `xxx_yyyy.hpp` 作为区分 用**`hpp`**

`xxx为 app bsp alg`

**最终迭代成使用CMAKE，工程使用C++书写，开盒即用**

# 此处记载：修改了ST生成的文件



## CMakeList

为了更方便的添加.c .cpp文件 在User文件夹内添加了一个子文件夹的CMakeList.txt

```bash
add_library(User_lib OBJECT)

target_include_directories(User_lib PUBLIC
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/Device
    ${CMAKE_CURRENT_SOURCE_DIR}/Module
    ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/Algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/Service
    ${CMAKE_CURRENT_SOURCE_DIR}/App
)
target_link_libraries(User_lib PRIVATE stm32cubemx)

# 包含所有的用户.c .cpp
file(GLOB_RECURSE USER_SOURCE
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS  # 关键：启用依赖检查

    ../User/*.c
    ../User/*.cpp
)
target_sources(User_lib PRIVATE ${USER_SOURCE})



```

以及在最后一行，添加此内容，增加了对浮点数打印的支持

```bash
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)
```

## .ld文件

为了DMA传输，把需要用到DMA的东西的存储，换到了DTCM之外

```c
__attribute__((section(".dma_buffer"))) 使用这一个缀修饰
```

修改ld文件的内容如下，中文注释之间为添加内容，下方的.data是原来带的，用于确定方位

```c
  .fini_array (READONLY) : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

 /* 用户为dma传输配置的内存地址 */
  .dma_buffer (NOLOAD) :
  {
    . = ALIGN(32);
    _sdma_buffer = .;
    *(.dma_buffer)
    *(.dma_buffer*)
    . = ALIGN(32);
    _edma_buffer = .;
  } >RAM_D1

  /* 用户dma相关配置结束 */

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    . = ALIGN(4);
  } >DTCMRAM AT> FLASH

```


## 让clangd 不报头文件未使用的错误（间接使用 clangd识别不出来）

使用： 在include头文件后面添加

`// IWYU pragma: keep`

可以规避 `Included header XXX.h is not used directly (fixes available)` 这个错误
