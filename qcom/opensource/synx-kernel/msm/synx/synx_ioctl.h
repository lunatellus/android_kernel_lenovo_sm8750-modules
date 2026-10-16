/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __SYNX_IOCTL_H__
#define __SYNX_IOCTL_H__
#include <linux/file.h>
#include <linux/fs.h>

long synx_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
int synx_open(struct inode *inode, struct file *filep);
int synx_close(struct inode *inode, struct file *filep);
#endif /* __SYNX_IOCTL_H__ */
