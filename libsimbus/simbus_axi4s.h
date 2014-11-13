#ifndef __simbus_axi4_H
#define __simbus_axi4_H
/*
 * Copyright (c) 2014 Stephen Williams (steve@icarus.com)
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
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

# include  "simbus_axi4_common.h"
# include  <stddef.h>
# include  <inttypes.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

struct simbus_axi4s_slave_s {
      simbus_axi4_resp_t (*write64)(simbus_axi4_t bus, uint64_t addr, int prot, uint64_t data);
      simbus_axi4_resp_t (*write32)(simbus_axi4_t bus, uint64_t addr, int prot, uint32_t data);
      simbus_axi4_resp_t (*write16)(simbus_axi4_t bus, uint64_t addr, int prot, uint16_t data);
      simbus_axi4_resp_t (*write8) (simbus_axi4_t bus, uint64_t addr, int prot, uint8_t data);

      simbus_axi4_resp_t (*read64)(simbus_axi4_t bus, uint64_t addr, int prot, uint64_t*data);
      simbus_axi4_resp_t (*read32)(simbus_axi4_t bus, uint64_t addr, int prot, uint32_t*data);
      simbus_axi4_resp_t (*read16)(simbus_axi4_t bus, uint64_t addr, int prot, uint16_t*data);
      simbus_axi4_resp_t (*read8) (simbus_axi4_t bus, uint64_t addr, int prot, uint8_t*data);
};

/*
 * The simbus_axi4s_slave function blocks forever, handling requests
 * from the AXI4 master. Whenever a function happens, one of the
 * appropriate functions is called from the slave_s struct to complete
 * the operation locally.
 *
 * If the function returns because if an error, the function will
 * return an error code.
 *
 *   SIMBUS_AXI4_ERROR
 *     Some error occurred on the AXI4 bus.
 *
 *   SIMBUS_AIX4_FINISH
 *     The simulation terminated by a FINISH command by the host. The
 *     proper response to this is to simbus_pci_disconnect().
 */
EXTERN int simbus_axi4_slave(simbus_axi4_t bus, const struct simbus_axi4s_slave_s*dev);

#undef EXTERN
#endif
