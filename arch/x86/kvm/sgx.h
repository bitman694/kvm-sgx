/*
 * KVM SGX Virtualization support.
 *
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Author:	Kai Huang <kai.huang@linux.intel.com>
 */

#ifndef	ARCH_X86_KVM_SGX_H
#define	ARCH_X86_KVM_SGX_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/kvm_host.h>
#include <asm/sgx.h>

int sgx_init(void);
void sgx_destroy(void);

#endif
