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

/*create proc interface*/
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

//this is net device
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ryan Hsu");
MODULE_DESCRIPTION("Driver for the RTL 8168 NIC");

/* if you want to query the device id and verdor id, you can use lspci -nn command to query it.
example: 05:00.0 Ethernet controller [0200]: Realtek Semiconductor Co., Ltd. Device [10ec:8161] (rev 15)
*/
#define VENDOR_ID	0x10ec
#define DEVICE_ID   0x8161 

/*
proc descriptor
*/
static struct proc_dir_entry *my_rtl8168_proc;

/* Added Device capabilities Array*/
/** Array contains all PCI Capabilities */
const char *pci_caps[] = {
		"Null Capability",						/* ID 0x0 */
		"PCI Power Management Interface",
		"Accelerated Graphics Port",
		"Vital Product Data",
		"Slot Identification", 
		"Message Signaled Interrupt",
		"Compact PCI Hot Swap",
		"PCI-X",
		"Hyper Transport",
		"Vendor-Specific",
		"Debug Port",
		"CompactPCI",
		"PCI Hot-Plug",
		"PCI Bridge Subsystem Vendor ID",
		"AGP 8x",
		"Secure Device",
		"PCI Express",
		"MSI-X",
		"Serial ATA Data",
		"Advanced Features",
		"Enhanced Allocation",
		"Flattening Portal Bridge",				/* ID: 0x15 */
		"Reserved"
};

/** Array contains all extended PCIe Capabilities */
const char *pcie_caps[] = {
	"Null Capability",
	"Advanced Error Reporting",
	"Virtual Channel",
	"Device Serial Number",
	"Power Budgeting",
	"Root Complex Link Declaration",
	"Root Complex Internal Link control",
	"Root Complex Event Collector Endpoint Association",
	"Multi-Function Virtual Channel",
	"Virtual Channel",
	"Vendor Specific Extended Capability",
	"Configuration Access Correlation",
	"Access Control Services",
	"Alternative Routing-ID Interpretation",
	"Address Translation Services",
	"Single Root I/O Virtualization",
	"Multi-Root I/O Virtualization",
	"Multicast",
	"Page Request Interface",
	"Reserved for AMD",
	"Resizable BAR",
	"Dynamic Power Allocation",
	"TPH, Requester",
	"Latency Tolerance Reporting",
	"Secondary PCI Express",
	"Protocol Mulitplexing",
	"Process Address Space",
	"LN Requester",
	"Downstream Port Containment",
	"L1 PM Substates",
	"Precision Time Measurement",
	"PCI Express over M-PHY",
	"FRS Queueing",
	"Readiness Time Reporting",
	"Designated Vendor-Specific Extended Capability",
	"VF Resizable BAR",
	"Data Link Feature",
	"Physical Layer 16.0 GT/s",
	"Lane Margining at the Receiver",
	"Hierarchy ID",
	"Native PCIe Enclosure Management",
	"Physical Layer 32.0 GT/s",
	"Alternate Protocol",
	"System Firmware Intermediary",
	"Reserved"
};




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
 * @brief searches a PCI(e) device for all possible capabilities 
 *
 * @param dev			PCI Device for checking capabilites
 */
void print_all_capabilities(struct pci_dev *dev) {
	u16 next_ptr = 0, cap_id = 0;
	u32 pcie_cap_id;
	u8 is_pcie = 0;
	bool result;
	if(dev == NULL)
			return;

	/* Check if the device has any PCI capabilites */
	result = pci_read_config_word(dev, PCI_CAPABILITY_LIST,&next_ptr);
	printk("capabilities address %x\n",next_ptr);
	if(result != 0) {
		printk("PCI(e) device doesn't have any capabilities!\n");
		return;
	}

	/* Search for all PCI extended capabilites */

	while(next_ptr) {
		pci_read_config_word(dev, next_ptr,&cap_id);
		if(cap_id == 0xff){
			printk("Error reading Capability ID... Try again as root!\n");
			return;
		}

		if(cap_id > 0x15) 
			cap_id = 0x16;
		if(cap_id!=0x00 && cap_id != 0x16 )
			printk("ID:next_ptr %x  0x%x - %s\n", next_ptr, cap_id, pci_caps[cap_id]);
		//pci_read_config_word(dev, next_ptr+0x1,&cap_id);
		if(cap_id == 0x10)
		{
			printk("this is support PCIe express capabilites %x next_ptr %x\n",cap_id,next_ptr);
			is_pcie = 1;
		}
		next_ptr=next_ptr+0x1;
	}
	printk("next_ptr adderss %x\n",next_ptr);
	/* If device is PCI Express device, read its PCIe capabilities */
	if(!is_pcie)
		return;
	printk("Printing PCI Express Capabilities.\n");

	next_ptr = 0x100;
	do {
		/* But now the capability header looks different:
		 * Bit 15:0 PCI Express Capability ID
		 * Bit 19:16 Capability Version Number
		 * Bit 31:20 Next Capability Offset
		 */
		pci_read_config_dword(dev, next_ptr,&pcie_cap_id);
		next_ptr = pcie_cap_id >> 20;
		cap_id = pcie_cap_id & 0xff;
		if(cap_id > 0x2c)
			cap_id = 0x2d;
		printk("ID: 0x%x - %s\n", cap_id, pcie_caps[cap_id]);
	} while(next_ptr);
}

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
	/*read datas from configure space */
	u16 vid, did;
	u8 capability_ptr;
	u32 bar0;//, //saved_bar0;


	printk("myrtl8168 - Now I am in the probe function.\n");
	/* read DID VID from the configure space */
	if(0 != pci_read_config_word(dev, 0x0, &vid)) {
		printk("myrtl8168 - Error reading from config space\n");
		return -1;
	}
	printk("myrtl8168 - VID; 0x%x\n", vid);
	if(0 != pci_read_config_word(dev, 0x2, &did)) {
		printk("myrtl8168 - Error reading from config space\n");
		return -1;
	}
	printk("myrtl8168 - DID; 0x%x\n", did);

	/* Read the pci capability pointer for PCI extenal capability(detail about PCIe capability) */
	printk("myrtl8168 - VID; 0x%x\n", vid);
	if(0 != pci_read_config_byte(dev, PCI_CAPABILITY_LIST, &capability_ptr)) {
		printk("myrtl8168 - Error reading from config space\n");
		return -1;
	}
	if(capability_ptr)
	{ 
		printk("myrtl8168 - PCI card has a PCIe capabilities!\n");
	}
	else
		printk("myrtl8168 - PCI card doesn't have a PCIe capabilities!\n");

	print_all_capabilities(dev);

	if(0 != pci_read_config_dword(dev, 0x10, &bar0)) {
		printk("myrtl8168 - Error reading from config space\n");
		return -1;
	}
	status = pci_resource_len(dev, 0);
	printk("myrtl8168 - BAR0 is %d bytes in size\n", status);
	if(status != 256) {
		printk("myrtl8168 - Wrong size of BAR0!\n");
		return -1;
	}
	printk("myrtl8168 - BAR0 is mapped to 0x%llx\n", pci_resource_start(dev, 0));
	printk("myrtl8168 - BAR1 is mapped to 0x%llx\n", pci_resource_start(dev, 1));
    printk("myrtl8168 - BAR2 is mapped to 0x%llx\n", pci_resource_start(dev, 2));
	printk("myrtl8168 - BAR3 is mapped to 0x%llx\n", pci_resource_start(dev, 3));
	printk("myrtl8168 - BAR4 is mapped to 0x%llx\n", pci_resource_start(dev, 4));
	printk("myrtl8168 - BAR5 is mapped to 0x%llx\n", pci_resource_start(dev, 5));
    printk("myrtl8168 - BAR6 is mapped to 0x%llx\n", pci_resource_start(dev, 6));

// Assuming you have a valid pci_dev structure named 'pdev'

	int bar_index;
	struct resource *bar;

	for (bar_index = 0; bar_index < PCI_NUM_RESOURCES; bar_index++) {
		bar = &dev->resource[bar_index];
		
		if (!bar->flags) {
			// Skip uninitialized BARs
			continue;
		}
		
		if (bar->flags & IORESOURCE_MEM) {
			printk("BAR %d: Memory Address\n", bar_index);
		} else if (bar->flags & IORESOURCE_IO) {
			printk("BAR %d: I/O Address\n", bar_index);
		} else {
			printk("BAR %d: Unknown Address Type\n", bar_index);
		}
	}
	// Assuming you have a valid pci_dev structure named 'pdev'
	// Find the MSI-X capability in the PCI configuration space
	int msix_cap_offset = pci_find_capability(dev, PCI_CAP_ID_MSIX);

	if (msix_cap_offset) {
		u16 msix_ctrl, msix_table_offset, msix_pba_offset;

		// Read MSI-X Control Register
		pci_read_config_word(dev, msix_cap_offset + PCI_MSIX_FLAGS, &msix_ctrl);

		// Read Table Offset and PBA Offset from MSI-X Capability
		pci_read_config_word(dev, msix_cap_offset + PCI_MSIX_TABLE, &msix_table_offset);
		pci_read_config_word(dev, msix_cap_offset + PCI_MSIX_PBA, &msix_pba_offset);

		printk("Found MSI-X Capability at offset %x\n", msix_cap_offset);
		printk("MSI-X Control Register: %x\n", msix_ctrl);
		printk("MSI-X Table Offset: %x\n", msix_table_offset);
		printk("MSI-X PBA Offset: %x\n", msix_pba_offset);
	} else {
		printk("No MSI-X Capability found.\n");
	}

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
	//init proc interface, create /proc/net/r8168
	my_rtl8168_proc = proc_mkdir("myrtl8168", init_net.proc_net);
	if(!my_rtl8168_proc)
		printk("cannot create myrtl8168 proc entry \n");
	printk("myrtl8168 - Registering the PCI device\n");
	return pci_register_driver(&myrtl8168_driver);
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void) {

	printk("myrtl8168 - Unregistering the PCI device\n");
	pci_unregister_driver(&myrtl8168_driver);
	if(my_rtl8168_proc)
	{
		remove_proc_subtree("myrtl8168",init_net.proc_net);
		my_rtl8168_proc=NULL;
	}

}

module_init(my_init);
module_exit(my_exit);

/*
if you want to find the capability, you can reference the below link.
https://elixir.bootlin.com/linux/v5.15/source/include/uapi/linux/pci_regs.h#L218
*/
