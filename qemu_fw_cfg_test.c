#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Viktor Prutyanov");
MODULE_DESCRIPTION("A simple Linux driver for test QEMU fw-cfg device.");
MODULE_VERSION("0.1");
 
#define FW_CFG_CTRL_OFF 0x00
#define FW_CFG_DATA_OFF 0x01
#define FW_CFG_FILE_DIR 0x19

#define FW_CFG_SIG_SIZE 4
#define FW_CFG_MAX_FILE_PATH 56

#define MY_PATH "opt/qemu/fwcfg_test"

phys_addr_t fw_cfg_p_base;
resource_size_t fw_cfg_p_size;
void __iomem *fw_cfg_dev_base;
void __iomem *fw_cfg_reg_ctrl;
void __iomem *fw_cfg_reg_data;

typedef struct FWCfgFile {
    uint32_t  size;        /* file size */
    uint16_t  select;      /* write this to 0x510 to read it */
    uint16_t  reserved;
    char      name[FW_CFG_MAX_FILE_PATH];
} FWCfgFile_t;

/* atomic access to fw_cfg device (potentially slow i/o, so using mutex) */
static DEFINE_MUTEX(fw_cfg_dev_lock);

/* read chunk of given fw_cfg blob (caller responsible for sanity-check) */
static inline void fw_cfg_read_blob(u16 key, void *buf, loff_t pos, size_t count)
{
	u32 glk;
	acpi_status status;

	/* If we have ACPI, ensure mutual exclusion against any potential
	 * device access by the firmware, via e.g. AML methods:
	 */
	status = acpi_acquire_global_lock(ACPI_WAIT_FOREVER, &glk);
	if (ACPI_FAILURE(status) && status != AE_NOT_CONFIGURED) {
		/* Should never get here */
		WARN(1, "fw_cfg_read_blob: Failed to lock ACPI!\n");
		memset(buf, 0, count);
		return;
	}

	mutex_lock(&fw_cfg_dev_lock);
	iowrite16(cpu_to_le16(key), fw_cfg_reg_ctrl);
	while (pos-- > 0)
		ioread8(fw_cfg_reg_data);
	ioread8_rep(fw_cfg_reg_data, buf, count);
	mutex_unlock(&fw_cfg_dev_lock);

	acpi_release_global_lock(glk);
}

static void fw_cfg_test_cleanup(void)
{
   	printk("fw_cfg_test: %s\n", __func__);
	ioport_unmap(fw_cfg_dev_base);
	release_region(fw_cfg_p_base, fw_cfg_p_size);
}

static void fw_cfg_test_show(void)
{
	u32 num;
	FWCfgFile_t file;
	u32 i;
	u32 size = 0;
	u16 select;
	void *buf;

	fw_cfg_read_blob(FW_CFG_FILE_DIR, &num, 0, sizeof(num));
	num = be32_to_cpu(num);
	printk("fw_cfg_test: num = %u\n", num);
	for (i = 0; i < num; i++) {
		ioread8_rep(fw_cfg_reg_data, &file, sizeof(file));
		if (!strcmp(MY_PATH, file.name)) {
			size = be32_to_cpu(file.size);
			select = be16_to_cpu(file.select);
		}
		printk("fw_cfg_test: [%.56s] size = %u, select = 0x%x\n", file.name, be32_to_cpu(file.size), be16_to_cpu(file.select));
	}

	if (size)
		printk("fw_cfg_test: size = %u, select = 0x%x\n", size, select);
	else {
		printk("fw_cfg_test: No such path!\n");
		return;
	}
	
	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return;
	fw_cfg_read_blob(select, buf, 0, size);
	printk("fw_cfg_test: %.*s\n", size, (char *)buf);
	kfree(buf);
}

static int fw_cfg_test_probe(struct platform_device *pdev)
{
	struct resource *range;
	char sig[FW_CFG_SIG_SIZE];
   	
	printk("fw_cfg_test: %s name=%s\n", __func__, pdev->name);
	range = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (!range)
		return -EINVAL;
	
	fw_cfg_p_base = range->start;
	fw_cfg_p_size = resource_size(range);
	printk("fw_cfg_test: base=0x%llx size=%llu\n", fw_cfg_p_base, fw_cfg_p_size);

	if (!request_region(fw_cfg_p_base, fw_cfg_p_size, "fw_cfg_test"))
		return -EBUSY;

	fw_cfg_dev_base = ioport_map(fw_cfg_p_base, fw_cfg_p_size);
	if (!fw_cfg_dev_base) {
		release_region(fw_cfg_p_base, fw_cfg_p_size);
		return -EFAULT;
	}
	
	fw_cfg_reg_ctrl = fw_cfg_dev_base + FW_CFG_CTRL_OFF;
	fw_cfg_reg_data = fw_cfg_dev_base + FW_CFG_DATA_OFF;
	
	/* verify fw_cfg device signature */
	fw_cfg_read_blob(0, sig, 0, FW_CFG_SIG_SIZE);
	if (memcmp(sig, "QEMU", FW_CFG_SIG_SIZE) != 0) {
		printk("fw_cfg_test: signature check FAILED\n");
		fw_cfg_test_cleanup();
		return -ENODEV;
	} else
		printk("fw_cfg_test: signature check OK\n");
	
	fw_cfg_test_show();

	return 0;
}

static int fw_cfg_test_remove(struct platform_device *pdev)
{
   	printk("fw_cfg_test: %s\n", __func__);
	fw_cfg_test_cleanup();
	
	return 0;
}

#ifdef CONFIG_ACPI
static const struct acpi_device_id fw_cfg_test_acpi_match[] = {
	{ "QEMU0002", },
	{},
};
MODULE_DEVICE_TABLE(acpi, fw_cfg_test_acpi_match);
#endif

static struct platform_driver fw_cfg_test_driver = {
	.probe = fw_cfg_test_probe,
	.remove = fw_cfg_test_remove,
	.driver = {
		.name = "fw_cfg",
		.acpi_match_table = ACPI_PTR(fw_cfg_test_acpi_match),
	},
};

static int __init fw_cfg_test_init(void)
{
	return platform_driver_register(&fw_cfg_test_driver);
}
 
static void __exit fw_cfg_test_exit(void)
{
	platform_driver_unregister(&fw_cfg_test_driver);
}
 
module_init(fw_cfg_test_init);
module_exit(fw_cfg_test_exit);
