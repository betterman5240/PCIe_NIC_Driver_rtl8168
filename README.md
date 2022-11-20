# PCIe_NIC_Driver_rtl8168
this is a sample driver for rtl_8168 nic card.

add kernel moudle to blacklist: sudo echo "blacklist r8169" >> /etc/modprobe.d/blacklist.conf 
update initramfs: update-initramfs -u
