# PCIe_NIC_Driver_rtl8168
this is a sample driver for rtl_8168 nic card.

add kernel moudle to blacklist: sudo echo "blacklist r8169" >> /etc/modprobe.d/blacklist.conf 
update initramfs: update-initramfs -u

related datas:
https://people.freebsd.org/~wpaul/RealTek/RTL8111B_8168B_Registers_DataSheet_1.0.pdf
reference driver code:
https://github.com/mtorromeo/r8168
