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

#include <linux/kvm_host.h>
#include <asm/cpufeature.h>	/* boot_cpu_has */
#include <asm/processor.h>	/* cpuid */
#include <linux/smp.h>
#include <linux/module.h>
#include "sgx.h"

/* Debug helpers... */
#define	sgx_debug(fmt, ...)	\
	printk(KERN_DEBUG "KVM: SGX: %s: "fmt, __func__, ## __VA_ARGS__)
#define	sgx_info(fmt, ...)	\
	printk(KERN_INFO "KVM: SGX: "fmt, ## __VA_ARGS__)
#define	sgx_err(fmt, ...)	\
	printk(KERN_ERR "KVM: SGX: "fmt, ## __VA_ARGS__)

/*
 * EPC pages are managed by SGX driver. KVM needs to call SGX driver's APIs
 * to allocate/free EPC page, etc.
 *
 * However KVM cannot call SGX driver's APIs directly. As on machine without
 * SGX support, SGX driver cannot be loaded, therefore if KVM calls driver's
 * APIs directly, KVM won't be able to be loaded either, which is not
 * acceptable. Instead, KVM uses symbol_get{put} pair to get driver's APIs
 * at runtime and simply disable SGX if those symbols cannot be found.
 */
struct required_sgx_driver_symbols {
	struct sgx_epc_page *(*alloc_epc_page)(unsigned int flags);
	/*
	 * Currently SGX driver's sgx_free_page has 'struct sgx_encl *encl'
	 * as parameter. We need to honor that.
	 */
	int (*free_epc_page)(struct sgx_epc_page *epg, struct sgx_encl *encl);
	/*
	 * get/put (map/unmap) kernel virtual address of given EPC page.
	 * The namings are aligned to SGX driver's APIs.
	 */
	void *(*get_epc_page)(struct sgx_epc_page *epg);
	void (*put_epc_page)(void *epc_page_vaddr);
};

static struct required_sgx_driver_symbols sgx_driver_symbols = {
	.alloc_epc_page = NULL,
	.free_epc_page = NULL,
	.get_epc_page = NULL,
	.put_epc_page = NULL,
};

static inline struct sgx_epc_page *sgx_alloc_epc_page(unsigned int flags)
{
	struct sgx_epc_page *epg;

	BUG_ON(!sgx_driver_symbols.alloc_epc_page);

	epg = sgx_driver_symbols.alloc_epc_page(flags);

	/* sgx_alloc_page returns ERR_PTR(error_code) instead of NULL */
	return IS_ERR_OR_NULL(epg) ? NULL : epg;
}

static inline void sgx_free_epc_page(struct sgx_epc_page *epg)
{
	BUG_ON(!sgx_driver_symbols.free_epc_page);

	sgx_driver_symbols.free_epc_page(epg, NULL);
}

static inline void *sgx_kmap_epc_page(struct sgx_epc_page *epg)
{
	BUG_ON(!sgx_driver_symbols.get_epc_page);

	return sgx_driver_symbols.get_epc_page(epg);
}

static inline void sgx_kunmap_epc_page(void *addr)
{
	BUG_ON(!sgx_driver_symbols.put_epc_page);

	sgx_driver_symbols.put_epc_page(addr);
}

static inline u64 sgx_epc_page_to_pfn(struct sgx_epc_page *epg)
{
	return (u64)(epg->pa >> PAGE_SHIFT);
}

static void put_sgx_driver_symbols(void);

static int get_sgx_driver_symbols(void)
{
	sgx_driver_symbols.alloc_epc_page = symbol_get(sgx_alloc_page);
	if (!sgx_driver_symbols.alloc_epc_page)
		goto error;
	sgx_driver_symbols.free_epc_page = symbol_get(sgx_free_page);
	if (!sgx_driver_symbols.free_epc_page)
		goto error;
	sgx_driver_symbols.get_epc_page = symbol_get(sgx_get_page);
	if (!sgx_driver_symbols.get_epc_page)
		goto error;
	sgx_driver_symbols.put_epc_page = symbol_get(sgx_put_page);
	if (!sgx_driver_symbols.put_epc_page)
		goto error;

	return 0;

error:
	put_sgx_driver_symbols();
	return -EFAULT;
}

static void put_sgx_driver_symbols(void)
{
	if (sgx_driver_symbols.alloc_epc_page)
		symbol_put(sgx_alloc_page);
	if (sgx_driver_symbols.free_epc_page)
		symbol_put(sgx_free_page);
	if (sgx_driver_symbols.get_epc_page)
		symbol_put(sgx_get_page);
	if (sgx_driver_symbols.put_epc_page)
		symbol_put(sgx_put_page);

	memset(&sgx_driver_symbols, 0, sizeof (sgx_driver_symbols));
}

int sgx_init(void)
{
	int r;

	r = get_sgx_driver_symbols();
	if (r) {
		sgx_err("SGX driver is not loaded.\n");
		return r;
	}

	sgx_info("SGX virtualization supported.\n");

	return 0;
}

void sgx_destroy(void)
{
	put_sgx_driver_symbols();
}
