#
# Makefile for the kernel multimedia device drivers.
#

obj-y += common/ IR/ video/

obj-$(CONFIG_VIDEO_DEV) += radio/
obj-$(CONFIG_DVB_CORE)  += dvb/
obj-$(CONFIG_IMAP_DECODE)	+= decode/
obj-$(CONFIG_IMAP_ENCODE)	+= encode/
obj-$(CONFIG_IMAP_CAMIF)        += camif/

# Added by Sololz.
obj-$(CONFIG_MACH_IMAPX200) += imapx/
