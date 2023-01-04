this is the first program for the PCIe driver base on the RTL8168 chipset, It can query bar 0,2,4 addresses, and register a PCIe device to the Kernel PCI set.

if you feel interested in this program, you can try to load it into your system and use the RTL8168 network card.

Regarding the program, I set up all functions of the module_int, module_exit, and pci_device structure. these functions are necessary for the Kernel module. I also defined the Vendor ID and Device ID in the program, how to query the Device_ID and Vendor_ID? You can use lspci -nn command to query.

example: 
05:00.0 Ethernet controller [0200]: Realtek Semiconductor Co., Ltd. Device [10ec:8161] (rev 15)

As the result, the 10ec and 8161 are Vendor_ID and Device_ID.
