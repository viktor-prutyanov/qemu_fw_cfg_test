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

phys_addr_t fw_cfg_p_base;
resource_size_t fw_cfg_p_size;
void __iomem *fw_cfg_dev_base;
void __iomem *fw_cfg_reg_ctrl;
void __iomem *fw_cfg_reg_data;

static int fw_cfg_test_probe(struct platform_device *pdev)
{
	struct resource *range, *ctrl, *data;
   	
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
	
	return 0;
}

static int fw_cfg_test_remove(struct platform_device *pdev)
{
   	printk("fw_cfg_test: %s\n", __func__);
	ioport_unmap(fw_cfg_dev_base);
	release_region(fw_cfg_p_base, fw_cfg_p_size);
	
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
