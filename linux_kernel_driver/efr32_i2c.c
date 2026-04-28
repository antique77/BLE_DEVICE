// SPDX-License-Identifier: GPL-2.0
/*
 * efr32_i2c.c - Linux 内核 I2C 客户端驱动 (MTK SoC)
 *
 * 目标平台: MTK SoC (MediaTek) - ARM64 / ARM32
 * 通信对象: EFR32 BG22 I2C 从机设备
 *
 * 功能:
 *   - 通过 sysfs 提供寄存器读写接口 (cat / echo)
 *   - 通过 /dev/efr32_i2c 提供 ioctl 接口
 *   - 自动探测并验证从机设备 ID
 *
 * 编译方式:
 *   外部模块: make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
 *   集成内核: 放入 drivers/misc/, 配置 CONFIG_EFR32_I2C=m/y
 *
 * 加载: insmod efr32_i2c.ko
 * 卸载: rmmod efr32_i2c
 *
 * 作者: Auto-generated
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

/* ================================================================
 *                     寄存器地址定义
 *         必须与单片机端 bsp_i2c.h 保持完全一致!
 * ================================================================ */
#define REG_DEVICE_ID     0x00    /* [R] 设备ID, 固定值 0xE3 */
#define REG_VERSION       0x01    /* [R] 固件版本号 V1.0 = 0x10 */
#define REG_ECHO          0x02    /* [R/W] 回显寄存器 (测试用) */
#define REG_COUNTER       0x03    /* [R] 通信计数器, 每次 STOP +1 */
#define REG_TOTAL_COUNT   4       /* 寄存器总数 */

/* ================================================================
 *                     固定参数 & 常量
 * ================================================================ */
#define EFR32_I2C_ADDR_7BIT   0x53        /* 7位从机地址 */
#define DEVICE_ID_MAGIC       0xE3        /* 探测用握手ID      */
#define DRIVER_NAME           "efr32_i2c" /* 驱动名称          */
#define MAX_XFER_SIZE         256         /* 最大传输长度      */

/* ================================================================
 *                     设备私有数据结构
 * ================================================================ */

/**
 * struct efr32_data - 每个 i2c_client 实例的私有数据
 * @client:       关联的 i2c_client 指针
 * @lock:         互斥锁, 保护并发 I2C 操作
 * @miscdev:      misc device 结构 (/dev 节点)
 * @reg_echo:     回显寄存器本地缓存
 * @tx_fail_cnt:  发送失败累计次数
 * @rx_total_cnt: 接收操作总次数统计
 */
struct efr32_data {
	struct i2c_client	*client;
	struct mutex		lock;
	struct miscdevice	miscdev;
	uint8_t			reg_echo;
	uint32_t		tx_fail_cnt;
	uint32_t		rx_total_cnt;
};

/* ================================================================
 *                  底层 I2C 读写原语
 * ================================================================ */

/**
 * efr32_reg_read() - SMBus 方式读取单个寄存器
 *
 * I2C 时序:
 *   [START][ADDR+W][REG][RESTART][ADDR+R][DATA][NACK][STOP]
 *
 * @data: 驱动私有数据
 * @reg:  寄存器地址 (0x00 ~ 0x03)
 * @val:  输出: 读到的值
 *
 * 返回: 0=成功, 负数=错误码 (-ENXIO/-EREMOTEIO 等)
 */
static int efr32_reg_read(struct efr32_data *data, uint8_t reg, uint8_t *val)
{
	struct i2c_client *client = data->client;
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev,
			"reg_read(0x%02X) failed: %d\n", reg, ret);
		data->tx_fail_cnt++;
		return ret;
	}

	*val = (uint8_t)ret;
	data->rx_total_cnt++;

	dev_dbg(&client->dev, "reg_read(0x%02X) = 0x%02X\n", reg, *val);
	return 0;
}

/**
 * efr32_reg_write() - SMBus 方式写入单个寄存器
 *
 * I2C 时序:
 *   [START][ADDR+W][REG][DATA][STOP]
 *
 * @data:  驱动私有数据
 * @reg:   寄存器地址
 * @value: 要写入的值
 *
 * 返回: 0=成功, 负数=错误码
 */
static int efr32_reg_write(struct efr32_data *data, uint8_t reg, uint8_t value)
{
	struct i2c_client *client = data->client;
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0) {
		dev_err(&client->dev,
			"reg_write(0x%02X, 0x%02X) failed: %d\n",
			reg, value, ret);
		data->tx_fail_cnt++;
		return ret;
	}

	dev_dbg(&client->dev, "reg_write(0x%02X) = 0x%02X\n", reg, value);
	return 0;
}

/**
 * efr32_read_regs() - 连续读取多个寄存器
 *
 * 使用 I2C block read (SMBus emulation), 从 reg 地址开始连续读取 len 字节.
 *
 * @data:  驱动私有数据
 * @reg:   起始寄存器地址
 * @buf:   输出缓冲区
 * @len:   要读取的字节数
 *
 * 返回: 成功返回读取字节数, 失败返回负错误码
 */
static int efr32_read_regs(struct efr32_data *data, uint8_t reg,
			   uint8_t *buf, uint16_t len)
{
	struct i2c_client *client = data->client;
	int ret;

	/*
	 * i2c_smbus_read_i2c_block_data:
	 *   先写 reg 地址, 再连续读最多 32 字节 (I2C_SMBUS_BLOCK_MAX)
	 */
	if (len > I2C_SMBUS_BLOCK_MAX)
		len = I2C_SMBUS_BLOCK_MAX;

	ret = i2c_smbus_read_i2c_block_data(reg, len, buf);
	if (ret < 0) {
		dev_err(&client->dev,
			"read_regs(0x%02X, %d) failed: %d\n",
			reg, len, ret);
		data->tx_fail_cnt++;
		return ret;
	}

	data->rx_total_cnt++;
	return ret;
}

/* ================================================================
 *                   Sysfs 属性接口
 *     用户空间通过 /sys/bus/i2c/devices/X-0053/xxx 访问
 * ================================================================ */

/*
 * ========== 1. device_id (只读) ==========
 * 用法: cat .../device_id
 * 输出: "0xE3\n"
 */
static ssize_t device_id_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct efr32_data *data = i2c_get_clientdata(client);
	uint8_t val;
	int ret;

	mutex_lock(&data->lock);
	ret = efr32_reg_read(data, REG_DEVICE_ID, &val);
	mutex_unlock(&data->lock);

	if (ret < 0)
		return ret;

	return sprintf(buf, "0x%02X\n", val);
}
static DEVICE_ATTR_RO(device_id);

/*
 * ========== 2. version (只读) ==========
 * 用法: cat .../version
 * 输出: "V1.0\n"
 */
static ssize_t version_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct efr32_data *data = i2c_get_clientdata(client);
	uint8_t val;
	int ret;

	mutex_lock(&data->lock);
	ret = efr32_reg_read(data, REG_VERSION, &val);
	mutex_unlock(&data->lock);

	if (ret < 0)
		return ret;

	return sprintf(buf, "V%d.%d\n", (val >> 4) & 0xF, val & 0xF);
}
static DEVICE_ATTR_RO(version);

/*
 * ========== 3. echo (可读写) ==========
 * 用法:
 *   写: echo 0xAB > .../echo
 *   读: cat .../echo   → 输出 "0xAB\n"
 */
static ssize_t echo_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct efr32_data *data = i2c_get_clientdata(client);
	uint8_t val;
	int ret;

	mutex_lock(&data->lock);
	ret = efr32_reg_read(data, REG_ECHO, &val);
	mutex_unlock(&data->lock);

	if (ret < 0)
		return ret;

	data->reg_echo = val;
	return sprintf(buf, "0x%02X\n", val);
}

static ssize_t echo_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct efr32_data *data = i2c_get_clientdata(client);
	unsigned long val;
	int ret;

	if (kstrtoul(buf, 0, &val) != 0 || val > 0xFF)
		return -EINVAL;

	mutex_lock(&data->lock);
	ret = efr32_reg_write(data, REG_ECHO, (uint8_t)val);
	mutex_unlock(&data->lock);

	if (ret < 0)
		return ret;

	data->reg_echo = (uint8_t)val;
	dev_info(&client->dev, "echo written: 0x%02X\n", data->reg_echo);
	return count;  /* 返回已写字节数表示成功 */
}
static DEVICE_ATTR_RW(echo);

/*
 * ========== 4. counter (只读) ==========
 * 用法: cat .../counter
 * 输出: "42\n" (每次有效通信 +1)
 */
static ssize_t counter_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct efr32_data *data = i2c_get_clientdata(client);
	uint8_t val;
	int ret;

	mutex_lock(&data->lock);
	ret = efr32_reg_read(data, REG_COUNTER, &val);
	mutex_unlock(&data->lock);

	if (ret < 0)
		return ret;

	return sprintf(buf, "%u\n", val);
}
static DEVICE_ATTR_RO(counter);

/*
 * ========== 5. stats (只读) - 驱动内部统计 ==========
 * 用法: cat .../stats
 * 输出:
 *   rx_total: 100
 *   tx_fail : 2
 */
static ssize_t stats_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct efr32_data *data = i2c_get_clientdata(client);

	return sprintf(buf,
		       "rx_total: %u\ntx_fail : %u\n",
		       data->rx_total_cnt,
		       data->tx_fail_cnt);
}
static DEVICE_ATTR_RO(stats);

/*
 * ========== 6. dump_all (只读) - Dump 所有寄存器 ==========
 * 用法: cat .../dump_all
 */
static ssize_t dump_all_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct efr32_data *data = i2c_get_clientdata(client);
	uint8_t regs[REG_TOTAL_COUNT];
	ssize_t off = 0;
	int ret;
	int i;

	const char *names[] = {
		[REG_DEVICE_ID] = "DEVICE_ID",
		[REG_VERSION]   = "VERSION",
		[REG_ECHO]      = "ECHO",
		[REG_COUNTER]   = "COUNTER",
	};

	mutex_lock(&data->lock);
	ret = efr32_read_regs(data, REG_DEVICE_ID, regs, REG_TOTAL_COUNT);
	mutex_unlock(&data->lock);

	if (ret < 0) {
		off = sprintf(buf, "dump_failed: %d\n", ret);
		return off;
	}

	for (i = 0; i < REG_TOTAL_COUNT && i < ret; i++) {
		off += snprintf(buf + off, PAGE_SIZE - off,
				"[0x%02X %-12s] = 0x%02X (%3d)\n",
				i, names[i] ? names[i] : "???",
				regs[i], regs[i]);
	}

	return off;
}
static DEVICE_ATTR_RO(dump_all);

/* ====== Sysfs 属性组注册 ====== */
static struct attribute *efr32_attrs[] = {
	&dev_attr_device_id.attr,
	&dev_attr_version.attr,
	&dev_attr_echo.attr,
	&dev_attr_counter.attr,
	&dev_attr_stats.attr,
	&dev_attr_dump_all.attr,
	NULL,
};

static const struct attribute_group efr32_attr_group = {
	.attrs = efr32_attrs,
};

/* ================================================================
 *                Misc Device 接口 (/dev/efr32_i2c)
 *     提供高效的 ioctl 方式给 C/C++ 用户程序使用
 * ================================================================ */

/* Ioctl 命令定义 */
#define EFR32_IOC_MAGIC		'E'
#define EFR32_IOCREGREAD	_IOWR(EFR32_IOC_MAGIC, 0x0, \
					 struct efr32_ioctl_reg)
#define EFR32_IOCREGWRITE	_IOW(EFR32_IOC_MAGIC, 0x1, \
					struct efr32_ioctl_reg)
#define EFR32_IOCREGDUMP	_IOR(EFR32_IOC_MAGIC, 0x2, \
					struct efr32_ioctl_dump)

/** 单个寄存器读写的数据结构 */
struct efr32_ioctl_reg {
	__u8	addr;	/* 寄存器地址 */
	__u8	value;	/* 数据值 (读时为输出, 写时为输入) */
	__s32	result;	/* 操作结果: 0=成功, <0=错误码 */
};

/** 批量 Dump 数据结构 */
struct efr32_ioctl_dump {
	__u8	buf[REG_TOTAL_COUNT];	/* 寄存器数据 */
	__u8	count;			/* 实际返回数量 */
	__s32	result;		/* 操作结果 */
};

/**
 * efr32_ioctl() - 处理来自 /dev 的 ioctl 请求
 */
static long efr32_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct efr32_data *data = file->private_data;
	struct efr32_ioctl_reg reg_arg;
	struct efr32_ioctl_dump dump_arg;
	int ret = 0;

	/* 校验 magic 和序号 */
	if (_IOC_TYPE(cmd) != EFR32_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > 2)
		return -ENOTTY;

	mutex_lock(&data->lock);

	switch (cmd) {

	case EFR32_IOCREGREAD:
		if (copy_from_user(&reg_arg,
				   (void __user *)arg, sizeof(reg_arg))) {
			ret = -EFAULT;
			break;
		}
		ret = efr32_reg_read(data, reg_arg.addr, &reg_arg.value);
		reg_arg.result = ret;
		if (copy_to_user((void __user *)arg, &reg_arg, sizeof(reg_arg)))
			ret = -EFAULT;
		else
			ret = 0;  /* ioctl 本身成功 */
		break;

	case EFR32_IOCREGWRITE:
		if (copy_from_user(&reg_arg,
				   (void __user *)arg, sizeof(reg_arg))) {
			ret = -EFAULT;
			break;
		}
		ret = efr32_reg_write(data, reg_arg.addr, reg_arg.value);
		reg_arg.result = ret;
		if (copy_to_user((void __user *)arg, &reg_arg, sizeof(reg_arg)))
			ret = -EFAULT;
		else
			ret = 0;
		break;

	case EFR32_IOCREGDUMP:
		memset(dump_arg.buf, 0xFF, REG_TOTAL_COUNT);
		dump_arg.count = 0;
		ret = efr32_read_regs(data, REG_DEVICE_ID,
				      dump_arg.buf, REG_TOTAL_COUNT);
		if (ret > 0)
			dump_arg.count = (__u8)(ret & 0xFF);
		dump_arg.result = (ret >= 0) ? 0 : ret;
		if (copy_to_user((void __user *)arg, &dump_arg, sizeof(dump_arg)))
			ret = -EFAULT;
		else
			ret = 0;
		break;

	default:
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&data->lock);
	return ret;
}

static int efr32_open(struct inode *inode, struct file *file)
{
	struct efr32_data *data =
		container_of(file->private_data,
			     struct efr32_data, miscdev);
	file->private_data = data;
	return 0;
}

static const struct file_operations efr32_fops = {
	.owner		= THIS_MODULE,
	.open		= efr32_open,
	.unlocked_ioctl	= efr32_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= efr32_ioctl,
#endif
};

/* ================================================================
 *              Probe / Remove - 驱动生命周期
 * ================================================================ */

/**
 * efr32_probe() - I2C 设备匹配成功后被调用
 *
 * 流程:
 *   1. 分配私有数据
 *   2. 读取 device_id 验证设备存在且正确
 *   3. 创建 sysfs 属性节点
 *   4. 注册 /dev/efr32_i2c misc device
 */
static int efr32_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct efr32_data *data;
	uint8_t devid = 0;
	int ret;

	dev_info(&client->dev,
		 "=== probing EFR32 at 0x%02X on bus %s ===\n",
		 client->addr, dev_name(&client->dev.parent));

	/* ---- 步骤1: 分配私有数据 ---- */
	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	mutex_init(&data->lock);
	data->reg_echo    = 0;
	data->tx_fail_cnt = 0;
	data->rx_total_cnt = 0;

	i2c_set_clientdata(client, data);

	/* ---- 步骤2: 快速探测 ---- */
	mutex_lock(&data->lock);
	ret = efr32_reg_read(data, REG_DEVICE_ID, &devid);
	mutex_unlock(&data->lock);

	if (ret < 0) {
		dev_err(&client->dev,
			"probe failed: device not responding! (err=%d)\n", ret);
		return -ENODEV;
	}

	if (devid != DEVICE_ID_MAGIC) {
		dev_err(&client->dev,
			"probe failed: bad device ID! got=0x%02X expected=0x%02X\n",
			devid, DEVICE_ID_MAGIC);
		return -ENODEV;
	}

	dev_info(&client->dev,
		 "EFR32 BG22 detected successfully (ID=0x%02X)\n", devid);

	/* ---- 步骤3: 创建 sysfs 属性组 ---- */
	ret = sysfs_create_group(&client->dev.kobj, &efr32_attr_group);
	if (ret < 0) {
		dev_err(&client->dev,
			"failed to create sysfs attributes: %d\n", ret);
		return ret;
	}

	/* ---- 步骤4: 注册 misc device (/dev/efr32_i2c) ---- */
	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.name  = "efr32_i2c";
	data->miscdev.fops  = &efr32_fops;
	data->miscdev.mode  = 0664;  /* rw-rw-r-- */

	ret = misc_register(&data->miscdev);
	if (ret < 0) {
		dev_err(&client->dev,
			"failed to register misc device: %d\n", ret);
		sysfs_remove_group(&client->dev.kobj, &efr32_attr_group);
		return ret;
	}

	dev_info(&client->dev,
		 "EFR32 driver loaded OK | sysfs + /dev/efr32_i2c ready\n");
	return 0;
}

/**
 * efr32_remove() - 设备移除或模块卸载时调用
 */
static int efr32_remove(struct i2c_client *client)
{
	struct efr32_data *data = i2c_get_clientdata(client);

	dev_info(&client->dev, "removing EFR32 driver...\n");

	misc_deregister(&data->miscdev);
	sysfs_remove_group(&client->dev.kobj, &efr32_attr_group);

	dev_info(&client->dev, "EFR32 driver removed.\n");
	return 0;
}

/* ================================================================
 *                 Driver 注册 & 匹配表
 * ================================================================ */

/*
 * 传统 ID 表 (用于非 DT 平台, 或通过 i2c_new_device 手动创建设备)
 */
static const struct i2c_device_id efr32_idtable[] = {
	{ "efr32_i2c", 0 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, efr32_idtable);

/*
 * Device Tree 匹配表 (MTK 平台主要使用此方式!)
 *
 * 在 DTS 中声明:
 *   efr32_i2c@53 {
 *       compatible = "silabs,efr32-i2c";
 *       reg = <0x53>;
 *   };
 */
#ifdef CONFIG_OF
static const struct of_device_id efr32_of_match[] = {
	{ .compatible = "silabs,efr32-i2c", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, efr32_of_match);
#endif

/*
 * i2c_driver 结构体 - 驱动的核心描述
 */
static struct i2c_driver efr32_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(efr32_of_match),
	},
	.probe		= efr32_probe,
	.remove		= efr32_remove,
	.id_table	= efr32_idtable,
};

module_i2c_driver(efr32_driver);

MODULE_AUTHOR("Auto-generated for EFR32 BG22 project");
MODULE_DESCRIPTION(
	"Linux Kernel I2C Client Driver for Silicon Labs EFR32 BG22 "
	"(target platform: MTK SoC)");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
