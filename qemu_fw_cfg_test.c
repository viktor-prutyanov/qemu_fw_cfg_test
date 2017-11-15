#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Viktor Prutyanov");
MODULE_DESCRIPTION("A simple Linux driver for test QEMU fw-cfg device.");
MODULE_VERSION("0.1");
 
static int __init qemu_fw_cfg_test_init(void){
   printk("qemu_fw_cfg_test: loaded\n");
   return 0;
}
 
static void __exit qemu_fw_cfg_test_exit(void){
   printk(KERN_INFO "qemu_fw_cfg_test: unloaded\n");
}
 
module_init(qemu_fw_cfg_test_init);
module_exit(qemu_fw_cfg_test_exit);
