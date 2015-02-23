/*
 * Copyright (c) 2008 Stephen Williams (steve@picturel.com)
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

# include  <simbus_pci.h>
# include  <stdio.h>
# include  <assert.h>

int main(int argc, char*argv[])
{
      simbus_pci_t pci = simbus_pci_connect(argv[1], "host");
      assert(pci);

      FILE*debug = fopen("host-debug.log", "w");
      simbus_pci_debug(pci, debug);

      simbus_pci_wait(pci, 4, 0);
      simbus_pci_reset(pci, 8, 8);

      uint32_t value = simbus_pci_config_read(pci, SIMBUS_PCI_DFN(0,0), 0);
      printf("Read from config 0x00010000: %lx\n", value);

      simbus_pci_wait(pci, 4, 0);

      simbus_pci_config_write(pci, SIMBUS_PCI_DFN(0,0), 0x10, 0xaa55aa55, 0);

      value = simbus_pci_config_read(pci, SIMBUS_PCI_DFN(0,0), 0x10);
      printf("Read from config DFN(0,0), BAR0: %lx\n", value);

      simbus_pci_wait(pci, 4, 0);

      printf("Should read zeros from mem1...\n");
      unsigned long base1 = 0xaa550000;
      for (unsigned idx = 0 ;  idx < 512 ;  idx += 8) {
	    uint32_t value1 = simbus_pci_read32(pci, base1 + idx, 0);
	    uint32_t value2 = simbus_pci_read32(pci, base1 + idx + 4, 0);
	    printf("0x%08x: %08lx %08lx\n", base1+idx, value1, value2);
      }

      simbus_pci_write32(pci, base1+8, 0xaaaa5555, 0);
      simbus_pci_wait(pci, 4, 0);

      value = simbus_pci_read32(pci, base1 + 8, 0);
      printf("0x%08x: %lx (should be 0xaaaa5555)\n", base1+8, value);

      simbus_pci_wait(pci, 4, 0);

      simbus_pci_write32(pci, base1+8, 0x0055aa00, 0x09);
      simbus_pci_wait(pci, 4, 0);

      value = simbus_pci_read32(pci, base1+8, 0);
      printf("0x%08x: %lx (should be 0xaa55aa55)\n", base1+8, value);

      simbus_pci_wait(pci, 4, 0);
      simbus_pci_end_simulation(pci);
      return 0;
}


/*
 * $Log: $
 */

