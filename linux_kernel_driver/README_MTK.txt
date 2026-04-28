# EFR32 BG22 - Linux 内核 I2C 驱动 (MTK SoC 平台)

## 文件清单

| 文件 | 说明 |
|------|------|
| `efr32_i2c.c` | **内核驱动源码** (唯一必须的 .c 文件) |
| `Makefile` | 内核模块编译脚本 |
| `Kconfig` | 内核菜单配置 (集成到内核树时使用) |
| `efr32_i2c_test.c` | 用户空间测试程序 |

## 快速开始

### 1. 原生编译 (x86_64 开发机上测试)

```bash
cd linux_kernel_driver
make                    # 编译 efr32_i2c.ko
sudo insmod efr32_i2c.ko # 加载驱动模块

# 手动创建 I2C 设备节点 (如果未用 DT)
echo efr32_i2c 0x53 > /sys/bus/i2c/devices/i2c-1/new_device

# 运行测试
gcc -Wall -O2 -o test efr32_i2c_test.c
sudo ./test 1
```

### 2. 交叉编译 (MTK ARM64 平台)

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export KERNEL_SRC=/path/to/mtk/kernel/source   # MTK 内核源码路径

make clean && make
# 输出: efr32_i2c.ko (ARM64 格式)
```

### 3. 部署到 MTK 设备

```bash
adb push efr32_i2c.ko /data/local/tmp/
adb push efr32_i2c_test /data/local/tmp/
adb shell "chmod 755 /data/local/tmp/efr32_i2c_test"

# 加载驱动
adb shell "insmod /data/local/tmp/efr32_i2c.ko"
adb shell "lsmod | grep efr32"          # 确认加载
adb shell "dmesg | grep efr32"          # 查看日志

# 如果 DT 中已配置, 设备会自动 probe
# 否则手动创建:
adb shell "echo efr32_i2c 0x53 > /sys/bus/i2c/devices/i2c-1/new_device"
```

## Device Tree 配置 (MTK 必需)

在板级 DTS 文件的对应 I2C bus 节点下追加:

```dts
&i2c1 {
    clock-frequency = <400000>;    /* Fast Mode 400kHz */
    status = "okay";

    efr32_i2c@53 {
        compatible = "silabs,efr32-i2c";
        reg = <0x53>;              /* 7-bit 从机地址 */
        /* 可选: pinctrl 配置 SDA/SCL 引脚 */
    };
};
```

**MTK DTS 路径参考:**
- `kernel/arch/arm64/boot/dts/mediatek/mtxxxx.dts`
- 或 vendor 分区中的 overlay dts

## 接口说明

### Sysfs 接口 (`/sys/bus/i2c/devices/X-0053/`)

```bash
cat device_id     # → "0xE3"           (设备ID, 只读)
cat version       # → "V1.0"            (固件版本, 只读)

echo 0xAB > echo # 写入回显寄存器
cat echo          # → "0xAB"            (回显, 可读写)

cat counter       # → "42"              (通信计数, 只读, 每次+1)
cat stats         # → rx_total/tx_fail  (驱动统计, 只读)
cat dump_all      # → 全部4个寄存器dump (只读)
```

### /dev 接口 (`/dev/efr32_i2c`)

```c
/* Ioctl 命令 */
#define EFR32_IOCREGREAD   _IOWR('E', 0, struct efr32_ioctl_reg)   /* 读单个寄存器 */
#define EFR32_IOCREGWRITE  _IOW ('E', 1, struct efr32_ioctl_reg)   /* 写单个寄存器 */
#define EFR32_IOCREGDUMP   _IOR ('E', 2, struct efr32_ioctl_dump)  /* Dump全部 */

/* 示例代码 */
int fd = open("/dev/efr32_i2c", O_RDWR);
struct efr32_ioctl_reg reg;
reg.addr = 0x02; reg.value = 0xAB;      // 写 echo = 0xAB
ioctl(fd, EFR32_IOCREGWRITE, &reg);

reg.addr = 0x02;                          // 读 echo
ioctl(fd, EFR32_IOCREGREAD, &reg);
printf("echo = 0x%02X\n", reg.value);    // → 0xAB
close(fd);
```

## 寄存器映射

```
地址   名称        属性  说明
────   ──────      ────  ────────────────────────
0x00   DEVICE_ID   R     固定值 0xE3 (探测握手)
0x01   VERSION     R     固件版本 V1.0 (0x10)
0x02   ECHO        R/W   回显寄存器 (通信测试用)
0x03   COUNTER     R     递增计数器 (每次STOP+1)
```

## 硬件接线

```
MTK SoC              EFR32 BG22
──────               ─────────
GPIO(SDA)  ───┬─── PB0 (SDA)
GPIO(SCL)  ───┼─── PB1 (SCL)
GND        ───┴─── GND
                  (SDA/SCL 各接 ~4.7kΩ 上拉到 VDD)
```

## 调试命令

```bash
# 查看 dmesg 驱动日志
dmesg | grep efr32

# I2C 总线扫描 (看 0x53 是否出现)
i2cdetect -y 1

# 查看所有 sysfs 属性
ls -la /sys/bus/i2c/devices/*/device_id

# 卸载驱动
rmmod efr32_i2c
```
