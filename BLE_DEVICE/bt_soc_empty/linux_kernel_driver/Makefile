# ============================================================
#  efr32_i2c - Linux 内核模块 Makefile
#
#  用法:
#    编译:   make
#    清理:   make clean
#    安装:   make install
#    查看信息: make info
#
#  前提条件:
#    - 已安装内核头文件: sudo apt-get install linux-headers-$(uname -r)
#    - 或指定内核源码路径: make KERNEL_SRC=/path/to/kernel
# ============================================================

# 内核源码目录 (默认使用当前运行内核)
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

# 模块目标
obj-m += efr32_i2c.o

# 交叉编译配置 (MTK 平台需要取消注释并修改)
# ARCH        ?= arm64
# CROSS_COMPILE ?= aarch64-linux-gnu-

.PHONY: all clean install info help modules

all: modules

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) $(subst ,,ARCH=$(ARCH)) \
		$(subst ,,CROSS_COMPILE=$(CROSS_COMPILE)) modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean

install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install
	depmod -a
	@echo ""
	@echo "=== 安装完成 ==="
	@echo "加载驱动: sudo modprobe efr32_i2c"
	@echo "或手动:    sudo insmod /lib/modules/$(shell uname -r)/extra/efr32_i2c.ko"

info:
	@echo "============================================="
	@echo "  EFR32 BG22 Linux I2C Kernel Driver"
	@echo "============================================="
	@echo ""
	@echo "  内核版本 : $(shell uname -r)"
	@echo "  架构     : $(shell uname -m)"
	@echo "  内核源码 : $(KERNEL_SRC)"
	@echo "  交叉编译 : $(if $(CROSS_COMPILE),$(CROSS_COMPILE),未设置(原生编译))"
	@echo ""

help:
	@echo ""
	@echo "╔════════════════════════════════════════╗"
	@echo "║    EFR32 BG22 I2C 驱动构建系统         ║"
	@echo "╠════════════════════════════════════════╣"
	@echo "║                                        ║"
	@echo "║  make              编译 .ko 模块       ║"
	@echo "║  make clean        清理编译产物         ║"
	@echo "║  make install      安装到系统           ║"
	@echo "║  make info         显示环境信息         ║"
	@echo "║  make help         显示此帮助           ║"
	@echo "║                                        ║"
	@echo "╠───────────── 交叉编译示例 ─────────────╣"
	@echo "║                                        ║"
	@echo "║  make ARCH=arm64 CROSS_COMPILE=aarch64- ║"
	@echo "║       -linux-gnu- KERNEL_SRC=/path/to/  ║"
	@echo "║       mtk/kernel                       ║"
	@echo "║                                        ║"
	@echo "╚════════════════════════════════════════╝"
	@echo ""
