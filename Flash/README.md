这里面为烧录脚本

个人测试可用

- `.bat`为win下面的烧录脚本
- `.sh` 为linux下面的烧录脚本
  - .sh文件 需要提前`chmod +x` 但是只需要一次就可以一直授权
- `.cfg`为openocd用到的配置内容
- 带ozone的为ozone的相关配置内容，应该每个人都不同

需要自行安装+配置好openocd jlink的环境变量

## Linux 下烧录当前工程

当前固件由两个独立产物组成：

- `DM_MC02_internal.hex`：STM32 内部 Flash 镜像；
- `DM_MC02_usb_xip.bin`：写入 W25Q64JV `0x70110000` 的 USB XIP 镜像。

构建完成后必须使用双镜像脚本，不能只下载 ELF：

```bash
bash Flash/OpenOCD_flash.sh Debug
```

参数是 `build/` 下的目录名。例如 HID 验证构建位于 `build/HID-Debug` 时使用：

```bash
bash Flash/OpenOCD_flash.sh HID-Debug
```

脚本先下载并校验内部镜像，再让固件把 OCTOSPI2 切换到 OpenOCD 可识别的单线映射，
随后擦写、校验外置镜像并复位运行。CDC/HID 跨模式更新由类别相关的 XIP 签名保护；旧类别
载荷不会被新内部固件执行。

Linux 若把 HID 节点创建成 `0600 root:root`，可将 `Flash/99-dm-mc02-usb.rules` 安装到
`/etc/udev/rules.d/` 后重载规则并重新插拔设备。规则仅匹配本工程的 `cafe:4001` CDC 和
`cafe:4002` HID，不会影响独立的 CMSIS-DAP 虚拟串口。
