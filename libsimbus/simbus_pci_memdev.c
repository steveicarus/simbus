/*
 * Copyright (c) 2008 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

# include  "simbus_pci.h"
# include  "simbus_pci_memdev.h"
# include  <stdlib.h>
# include  <assert.h>


# define FILL_CMD    0x0000
# define COMPARE_CMD 0x0001
# define LOAD_CMD    0x0002
# define SAVE_CMD    0x0003

struct simbus_pci_memdev_s {
      simbus_pci_t bus;
	/* Config space address for the device */
      uint64_t config;
	/* PCI Device ID */
      uint32_t devid;
      uint64_t base_memory;
      uint64_t base_cntrl;
};

static void m1writel(struct simbus_pci_memdev_s*xsp, uint64_t off, uint32_t val)
{
      simbus_pci_write32(xsp->bus, xsp->base_cntrl + off, val, 0);
}

static uint32_t m1readl(struct simbus_pci_memdev_s*xsp, uint64_t off)
{
      return simbus_pci_read32(xsp->bus, xsp->base_cntrl + off, 0);
}

simbus_pci_memdev_t simbus_pci_memdev_init(simbus_pci_t bus, uint64_t config)
{
      uint32_t val;
      struct simbus_pci_memdev_s*xsp = calloc(1, sizeof(struct simbus_pci_memdev_s));

      xsp->bus = bus;
      xsp->config = config;

	/* We need the device ID to distinguish variants. */
      xsp->devid = simbus_pci_config_read(xsp->bus, xsp->config + 0);

      switch (xsp->devid) {

	  case 0xffc112c5:
	      /* BAR0 has low address bits */
	    val = simbus_pci_config_read(xsp->bus, xsp->config+16);
	    xsp->base_memory = val & ~15;
	      /* BAR1 has high address bits */
	    val = simbus_pci_config_read(xsp->bus, xsp->config + 20);
	    xsp->base_memory = val;
	    xsp->base_memory |= ((uint64_t)val) << 32;
	      /* BAR2 has base of control registers */
	    val = simbus_pci_config_read(xsp->bus, xsp->config + 24);
	    xsp->base_cntrl = val & ~15;
	    break;

	  default:
	    assert(0);
	    break;
      }

      return xsp;
}

uint64_t simbus_pci_memdev_start(simbus_pci_memdev_t xsp)
{
      return xsp->base_memory;
}

void simbus_pci_memdev_fill(simbus_pci_memdev_t xsp,
			    uint64_t off, unsigned cnt,
			    uint32_t fill)
{
      m1writel(xsp,  4, off);
      m1writel(xsp,  8, cnt);
      m1writel(xsp, 12, fill);
	/* This starts the command going. */
      m1writel(xsp,  0, FILL_CMD);
}

int simbus_pci_memdev_compare(simbus_pci_memdev_t xsp,
			      uint64_t off0, uint64_t off1,
			      unsigned cnt0)
{
      unsigned long rc;
      m1writel(xsp,  4, off0);
      m1writel(xsp,  8, cnt0);
      m1writel(xsp, 12, off1);
      m1writel(xsp,  0, COMPARE_CMD);

      rc = m1readl(xsp, 0);
      return (rc == 0)? 0 : 1;
}

int simbus_pci_memdev_load(simbus_pci_memdev_t xsp,
			   uint64_t offset, unsigned  count,
			   const char*file)
{
      int rc;

      m1writel(xsp,  4, offset);
      m1writel(xsp,  8, count);

      rc = 12;
      while (file[0] != 0) {
	    uint32_t word = 0;

	    word |= ((uint32_t)file[0] <<  0) & 0x000000ffUL;
	    if (file[0] != 0) file += 1;
	    word |= ((uint32_t)file[0] <<  8) & 0x0000ff00UL;
	    if (file[0] != 0) file += 1;
	    word |= ((uint32_t)file[0] << 16) & 0x00ff0000UL;
	    if (file[0] != 0) file += 1;
	    word |= ((uint32_t)file[0] << 24) & 0xff000000UL;
	    if (file[0] != 0) file += 1;

	    m1writel(xsp, rc, word);
	    rc += 4;
      }

      m1writel(xsp, rc, 0);

      m1writel(xsp, 0, LOAD_CMD);

      rc = m1readl(xsp, 0);
      return rc;
}

int simbus_pci_memdev_save(simbus_pci_memdev_t xsp,
			   uint64_t offset, unsigned count,
			   const char*file)
{
      int rc;

      m1writel(xsp,  4, offset);
      m1writel(xsp,  8, count);

      rc = 12;
      while (file[0] != 0) {
	    uint32_t word = 0;

	    word |= (file[0] <<  0) & 0x000000ffUL;
	    if (file[0] != 0) file += 1;
	    word |= (file[0] <<  8) & 0x0000ff00UL;
	    if (file[0] != 0) file += 1;
	    word |= (file[0] << 16) & 0x00ff0000UL;
	    if (file[0] != 0) file += 1;
	    word |= (file[0] << 24) & 0xff000000UL;
	    if (file[0] != 0) file += 1;

	    m1writel(xsp, rc, word);
	    rc += 4;
      }

      m1writel(xsp, rc, 0);

      m1writel(xsp, 0, SAVE_CMD);

      rc = m1readl(xsp, 0);
      return rc;
}
