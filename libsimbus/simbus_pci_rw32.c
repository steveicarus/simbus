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


uint32_t simbus_pci_read32(simbus_pci_t pci, uint64_t addr, int BEn)
{
      uint32_t val = 0xffffffff;
      int retry = 1;
      while (retry) {
	    int rc = __generic_pci_read32(pci, addr, 0xf6, BEn, &val);

	    if (rc == GPCI_TARGET_RETRY) {
		    /* Automatically retry until the target responds
		       with something other then retry. */
		  continue;
	    }

	    if (rc >= 0) {
		  retry = 0;
		  break;
	    }

	    if (rc < 0) {
		  fprintf(stderr, "simbus_pci_read32: "
			  "No response from addr=0x%" PRIx64 ", rc=%d\n", addr, rc);
		  return 0xffffffff;
	    }
      }

      return val;
}

void simbus_pci_write32(simbus_pci_t pci, uint64_t addr, uint32_t val, int BEn)
{
      int rc;

      rc = __generic_pci_write32(pci, addr, 0xf7, val, BEn);
      if (rc < 0) {
	    fprintf(stderr, "simbus_pci_write32: "
		    "No response to addr=0x%" PRIx64 "\n", addr);
	    return ;
      }
}

int simbus_pci_write32b(simbus_pci_t pci, uint64_t addr,
			 const uint32_t*val, int words,
			 int BEFn, int BELn)
{
      assert(words > 0);
      assert(words > 1 || BEFn==BELn);

      int idx;
      for (idx = 0 ; idx < words ; idx += 1, addr += 4) {
	    int use_BEn = 0;
	    if (idx == 0) use_BEn = BEFn;
	    else if (idx+1 == words) use_BEn = BELn;

	    simbus_pci_write32(pci, addr, val[idx], use_BEn);
      }

      return words;
}
