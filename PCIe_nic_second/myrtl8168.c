/*
reference page : https://docs.kernel.org/PCI/pci.html
Author : Ryan Hsu 
Github : https://github.com/betterman5240
topic : Enable the device
add an init and an exit function, also add a probe function.
*/



#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ryan Hsu");
MODULE_DESCRIPTION("Driver for the RTL 8168 NIC");

/* if you want to query the device id and verdor id, you can use lspci -nn command to query it.
example: 05:00.0 Ethernet controller [0200]: Realtek Semiconductor Co., Ltd. Device [10ec:8161] (rev 15)
*/
#define VENDOR_ID	0x10ec
#define DEVICE_ID   0x8161 

static struct pci_device_id myrtl8168_ids[] = {
	{ PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, myrtl8168_ids);


struct myrtl8168_nic {
	void __iomem *ptr_bar0;
	void __iomem *ptr_bar2;
    void __iomem *ptr_bar4;
};

/**
 * @brief Function is called, when a PCI device is registered
 *
 * @param dev   pointer to the PCI device
 * @param id    pointer to the corresponding id table's entry
 *
 * @return      0 on success
 *              negative error code on failure
 */
static int myrtl8168_probe(struct pci_dev *dev, const struct pci_device_id *id) {
	struct myrtl8168_nic *my_data;
	int status;

	printk("myrtl8168 - Now I am in the probe function.\n");

	status = pci_resource_len(dev, 0);
	printk("myrtl8168 - BAR0 is %d bytes in size\n", status);
	if(status != 256) {
		printk("myrtl8168 - Wrong size of BAR0!\n");
		return -1;
	}

	printk("myrtl8168 - BAR0 is mapped to 0x%llx\n", pci_resource_start(dev, 0));
    printk("myrtl8168 - BAR2 is mapped to 0x%llx\n", pci_resource_start(dev, 2));
    printk("myrtl8168 - BAR4 is mapped to 0x%llx\n", pci_resource_start(dev, 4));

	status = pcim_enable_device(dev);
	if(status < 0) {
		printk("myrtl8168- Could not enable device\n");
		return status;
	}

	status = pcim_iomap_regions(dev, BIT(0), KBUILD_MODNAME);
	if(status < 0) {
		printk("myrtl8168 - BAR0 is already in use!\n");
		return status;
	}

	my_data = devm_kzalloc(&dev->dev, sizeof(struct myrtl8168_nic), GFP_KERNEL);
	if(my_data == NULL) {
		printk("myrtl8168 - Error! Out of memory\n");
		return -ENOMEM;
	}

	my_data->ptr_bar0 = pcim_iomap_table(dev)[0];
	if(my_data->ptr_bar0 == NULL) {
		printk("myrtl8168 - BAR0 pointer is invalid\n");
		return -1;
	}


	pci_set_drvdata(dev, my_data);

	return 0;
}

/**
 * @brief Function is called, when a PCI device is unregistered
 *
 * @param dev   pointer to the PCI device
 */
static void myrtl8168_remove(struct pci_dev *dev) {
    struct myrtl8168_nic * my_data = pci_get_drvdata(dev);
    if(my_data)
    {
        //devm_kfree(my_data);
        printk("myrtl8168 - release data structure.\n");
    }
	printk("myrtl8168 - Now I am in the remove function.\n");
}

/* PCI driver struct */
static struct pci_driver myrtl8168_driver = {
	.name = "myrtl8168",
	.id_table = myrtl8168_ids,
	.probe = myrtl8168_probe,
	.remove = myrtl8168_remove,
};

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init my_init(void) {
	printk("myrtl8168 - Registering the PCI device\n");
	return pci_register_driver(&myrtl8168_driver);
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void) {
	printk("myrtl8168 - Unregistering the PCI device\n");
	pci_unregister_driver(&myrtl8168_driver);
}

module_init(my_init);
module_exit(my_exit);
