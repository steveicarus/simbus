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
# include  "simbus_pci_priv.h"
# include  <stdio.h>
# include  <assert.h>

uint32_t simbus_pci_config_read(simbus_pci_t pci, uint64_t addr)
{
      uint32_t val = 0xffffffff;
      int rc = __generic_pci_read32(pci, addr, 0xfa, 0xf0, &val);
      if (rc < 0) {
	    fprintf(stderr, "simbus_pci_config_read: "
		    "No response to addr=0x%x, rc=%d\n", addr, rc);
	    return 0xffffffff;
      }

      return val;
}


void simbus_pci_config_write(simbus_pci_t pci, uint64_t addr, uint32_t val, int BEn)
{
      int rc;

      rc = __generic_pci_write32(pci, addr, 0xfb, val, BEn);
      if (rc < 0) {
	    fprintf(stderr, "simbus_pci_config_write: "
		    "No response to addr=0x%x\n", addr);
	    return ;
      }
}
