#ifndef __protocol_H
#define __protocol_H
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

# include  "priv.h"
struct bus_state;

class protocol_t  {

    public:
      explicit protocol_t(struct bus_state&);
      virtual ~protocol_t();

	// Call this once before the first call to bus_ready.
      virtual void run_init();

	// This method is called by the server when all the clients
	// for a bus are marked as ready.
      void bus_ready();

    protected:
      bus_device_map_t& device_map();

      void advance_time_(uint64_t mant, int exp);

    private:
      virtual void run_run() =0;
      struct bus_state&bus_;

      uint64_t time_mant_;
      int time_exp_;

    private: // Not implemented
      protocol_t(const protocol_t&);
      protocol_t& operator= (const protocol_t&);
};

class PciProtocol  : public protocol_t {

    public:
      PciProtocol(struct bus_state&);
      ~PciProtocol();

      void run_init();
      void run_run();

    private:
      void advance_pci_clock_(void);
      bit_state_t calculate_reset_n_(void);
      void arbitrate_(void);
      void route_interrupts_(void);

    private:
	// Current state of the PCI clock. (It toggles.)
      bit_state_t pci_clk_;
	// Device that is currently granted, or -1 if none.
      int granted_;
};

#endif
