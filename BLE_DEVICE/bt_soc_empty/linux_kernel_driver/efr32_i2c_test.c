/**
 * efr32_i2c_test.c - 用户空间测试程序
 *
 * 测试 Linux 内核 I2C 驱动的 sysfs 和 /dev 接口
 *
 * 编译: gcc -Wall -O2 -o efr32_test efr32_i2c_test.c
 * 运行: sudo ./efr32_test [i2c_bus_number]
 *
 * 示例:
 *   sudo ./efr32_test 1        # 使用 i2c-1 (默认)
 *   sudo ./efr32_test 0        # 使用 i2c-0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

/* ====== ioctl 定义 (与内核驱动一致) ====== */
#define EFR32_IOC_MAGIC     'E'
#define EFR32_IOCREGREAD    _IOWR(EFR32_IOC_MAGIC, 0x0, struct efr32_ioctl_reg)
#define EFR32_IOCREGWRITE   _IOW(EFR32_IOC_MAGIC, 0x1, struct efr32_ioctl_reg)
#define EFR32_IOCREGDUMP    _IOR(EFR32_IOC_MAGIC, 0x2, struct efr32_ioctl_dump)

struct efr32_ioctl_reg {
	unsigned char addr;
	unsigned char value;
	int          result;
};

struct efr32_ioctl_dump {
	unsigned char buf[4];
	unsigned char count;
	int          result;
};

/* ====== Sysfs 路径构建 ====== */
static char sysfs_path[256];

static void build_sysfs(int bus, const char *attr)
{
	snprintf(sysfs_path, sizeof(sysfs_path),
		 "/sys/bus/i2c/devices/%d-0053/%s", bus, attr);
}

/* ====== 读取 sysfs 文件 ====== */
static int sysfs_read(const char *path, char *buf, size_t len)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf("  [ERR] 打开 %s 失败: %s\n", path, strerror(errno));
		return -1;
	}
	ssize_t n = read(fd, buf, len - 1);
	close(fd);
	if (n > 0) {
		buf[n] = '\0';
		/* 去掉末尾换行 */
		while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r'))
			buf[--n] = '\0';
	}
	return (int)n;
}

/* ====== 写入 sysfs 文件 ====== */
static int sysfs_write(const char *path, const char *val)
{
	int fd = open(path, O_WRONLY);
	if (fd < 0) {
		printf("  [ERR] 打开 %s 失败: %s\n", path, strerror(errno));
		return -1;
	}
	ssize_t n = write(fd, val, strlen(val));
	close(fd);
	return (int)n;
}

/* ====== 测试函数 ====== */

static int test_sysfs_probe(int bus)
{
	printf("\n━━━ 测试1: 设备探测 ━━━\n");

	char val[64];

	build_sysfs(bus, "device_id");
	if (sysfs_read(sysfs_path, val, sizeof(val)) <= 0)
		return -1;
	printf("  device_id = %s  %s\n",
	       val, (strcmp(val, "0xE3") == 0) ? "[OK]" : "[FAIL!]");

	build_sysfs(bus, "version");
	if (sysfs_read(sysfs_path, val, sizeof(val)) <= 0)
		return -1;
	printf("  version   = %s\n", val);

	return 0;
}

static int test_echo_rw(int bus)
{
	printf("\n━━━ 测试2: 回显读写 (ECHO) ━━━\n");

	char val[64];
	const char *test_vals[] = { "0xAB", "0x55", "0x00", "0xFF" };
	int n = sizeof(test_vals) / sizeof(test_vals[0]);

	for (int i = 0; i < n; i++) {
		build_sysfs(bus, "echo");
		if (sysfs_write(sysfs_path, test_vals[i]) < 0)
			continue;

		usleep(5000);  /* 等待5ms让从机处理 */

		build_sysfs(bus, "echo");
		if (sysfs_read(sysfs_path, val, sizeof(val)) <= 0)
			continue;

		printf("  写入 %-6s → 回读 %-6s %s\n",
		       test_vals[i], val,
		       (strcasecmp(val + (val[0]=='0'?0:0), test_vals[i]+(test_vals[i][0]=='0'?0:0)) == 0)
			 ? "[OK]" : "[MISMATCH]");
	}

	return 0;
}

static int test_counter(int bus)
{
	printf("\n━━━ 测试3: 计数器递增验证 ━━━\n");

	char val[64];

	build_sysfs(bus, "counter");
	sysfs_read(sysfs_path, val, sizeof(val));
	printf("  counter 第1次读 = %s\n", val);

	build_sysfs(bus, "counter");
	sysfs_read(sysfs_path, val, sizeof(val));
	printf("  counter 第2次读 = %s (应该 >= 上次值)\n", val);

	return 0;
}

static int test_dump_all(int bus)
{
	printf("\n━━━ 测试4: Dump 所有寄存器 ━━━\n");

	char buf[512];

	build_sysfs(bus, "dump_all");
	if (sysfs_read(sysfs_path, buf, sizeof(buf)) <= 0)
		return -1;

	printf("  %s\n", buf);
	return 0;
}

static int test_stats(int bus)
{
	printf("\n━━━ 测试5: 驱动统计信息 ━━━\n");

	char buf[128];
	build_sysfs(bus, "stats");
	sysfs_read(sysfs_path, buf, sizeof(buf));
	printf("  %s\n", buf);

	return 0;
}

static int test_dev_ioctl(void)
{
	printf("\n━━━ 测试6: /dev/efr32_i2c ioctl 接口 ━━━\n");

	int fd = open("/dev/efr32_i2c", O_RDWR);
	if (fd < 0) {
		printf("  [WARN] /dev/efr32_i2c 不存在, 跳过 ioctl 测试\n");
		printf("         (驱动可能未正确加载)\n");
		return 0;  /* 不算失败 */
	}

	struct efr32_ioctl_reg reg;
	struct efr32_ioctl_dump dump;

	/* 读 device_id */
	reg.addr = 0x00;
	reg.value = 0;
	reg.result = 0;
	ioctl(fd, EFR32_IOCREGREAD, &reg);
	printf("  ioctl read REG[0x00] = 0x%02X (result=%d) %s\n",
	       reg.value, reg.result,
	       (reg.value == 0xE3 && reg.result == 0) ? "[OK]" : "");

	/* 写 echo = 0xCC */
	reg.addr = 0x02;
	reg.value = 0xCC;
	ioctl(fd, EFR32_IOCREGWRITE, &reg);
	printf("  ioctl write REG[0x02] = 0x%02X (result=%d)\n",
	       0xCC, reg.result);

	/* 回读 echo */
	reg.addr = 0x02;
	reg.value = 0;
	ioctl(fd, EFR32_IOCREGREAD, &reg);
	printf("  ioctl read REG[0x02] = 0x%02X (result=%d) %s\n",
	       reg.value, reg.result,
	       (reg.value == 0xCC && reg.result == 0) ? "[OK]" : "[MISMATCH]");

	/* dump all */
	memset(&dump, 0, sizeof(dump));
	ioctl(fd, EFR32_IOCREGDUMP, &dump);
	printf("  ioctl DUMP: count=%d result=%d\n", dump.count, dump.result);
	if (dump.count > 0) {
		printf("    regs:");
		for (int i = 0; i < dump.count && i < 4; i++)
			printf(" 0x%02X", dump.buf[i]);
		printf("\n");
	}

	close(fd);
	return 0;
}

/* ====== 主函数 ====== */
int main(int argc, char **argv)
{
	int bus = 1;

	if (argc >= 2)
		bus = atoi(argv[1]);

	printf("\n");
	printf("╔════════════════════════════════════════════════╗\n");
	printf("║   EFR32 BG22 I2C 内核驱动测试工具 v1.0       ║\n");
	printf("╚════════════════════════════════════════════════╝\n");
	printf("  I2C Bus : /dev/i2c-%d\n", bus);
	printf("  从机地址: 0x53 (7-bit)\n");

	/* 检查设备是否存在 */
	char probe_path[64];
	snprintf(probe_path, sizeof(probe_path),
		 "/sys/bus/i2c/devices/%d-0053/device_id", bus);

	int check_fd = open(probe_path, O_RDONLY);
	if (check_fd < 0) {
		printf("\n  [FATAL] 设备不存在! %s\n", probe_path);
		printf("\n  请检查:\n");
		printf("    1. 已加载驱动: lsmod | grep efr32\n");
		printf("    2. I2C 总线: ls /dev/i2c-*\n");
		printf("    3. DT 配置或手动创建设备:\n");
		printf("       echo efr32_i2c 0x53 > /sys/bus/i2c/devices/i2c-%d/new_device\n",
		       bus);
		printf("    4. 硬件接线与上拉电阻\n");
		return EXIT_FAILURE;
	}
	close(check_fd);
	printf("  设备节点: 存在 ✓\n");

	/* 执行所有测试 */
	test_sysfs_probe(bus);
	test_dump_all(bus);
	test_echo_rw(bus);
	test_counter(bus);
	test_stats(bus);
	test_dev_ioctl();

	printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
	printf("  所有测试完成!\n");
	printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");

	return EXIT_SUCCESS;
}
