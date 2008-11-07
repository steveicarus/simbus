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

# include  "protocol.h"
# include  "priv.h"
# include  <iostream>
# include  <assert.h>

using namespace std;

protocol_t::protocol_t(struct bus_state&b)
: bus_(b)
{
}

protocol_t::~protocol_t()
{
}

bus_device_map_t& protocol_t::device_map()
{
      return bus_.device_map;
}

void protocol_t::advance_time_(uint64_t mant, int exp)
{
      while (exp < time_exp_) {
	    time_mant_ *= 10;
	    time_exp_ -= 1;
      }
      while (time_exp_ < exp) {
	    mant *= 10;
	    exp -= 1;
      }

      time_mant_ += mant;
}

void protocol_t::bus_ready()
{
	// First, clear the ready flags for all the devices. This will
	// force the expectation of a new READY message from all the
	// devices.
      for (bus_device_map_t::iterator dev = bus_.device_map.begin()
		 ; dev != bus_.device_map.end() ;  dev ++) {
	    dev->second.ready_flag = false;
      }

	// Call the protocol engine.
      run_run();

	// Send the new signal state to the client.
      for (bus_device_map_t::iterator dev = bus_.device_map.begin()
		 ; dev != bus_.device_map.end() ;  dev ++) {
	    int fd = dev->second.fd;
	    signal_state_map_t&sigs = dev->second.send_signals;

	    char buf[4097];
	    snprintf(buf, sizeof buf, "UNTIL %lue%d", time_mant_, time_exp_);

	    char*cp = buf + strlen(buf);
	    for (signal_state_map_t::iterator cur_sig = sigs.begin()
		       ; cur_sig != sigs.end() ; cur_sig ++) {

		  int width = cur_sig->second.size();

		  *cp++ = ' ';
		  strcpy(cp, cur_sig->first.c_str());
		  cp += strlen(cp);
		  *cp++ = '=';
		  for (int idx = 0 ; idx < width ; idx+=1) {
			switch (cur_sig->second[width-idx-1]) {
			    case BIT_0:
			      *cp++ = '0';
			      break;
			    case BIT_1:
			      *cp++ = '1';
			      break;
			    case BIT_Z:
			      *cp++ = 'z';
			      break;
			    case BIT_X:
			      *cp++ = 'x';
			      break;
			    default:
			      assert(0);
			}
		  }

	    }

	    *cp++ = '\n';
	    int rc = write(fd, buf, cp-buf);
	    assert(rc == (cp-buf));
      }
}

void protocol_t::run_run()
{
      assert(0);
}


PciProtocol::PciProtocol(struct bus_state&b)
: protocol_t(b), pci_clk_(1)
{
      pci_clk_ = BIT_1;
}

PciProtocol::~PciProtocol()
{
}

void PciProtocol::run_run()
{
	// Step the PCI clock.
      advance_pci_clock_();

	// Calculate the RESET# signal.
      valarray<bit_state_t> reset_n(1);
      reset_n[0] = calculate_reset_n_();


	// Assign the output results back to the devices.
      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++) {

	    struct bus_device_plug&curdev = dev->second;

	      // Common signals...

	    curdev.send_signals["PCI_CLK"].resize(pci_clk_.size());
	    curdev.send_signals["PCI_CLK"] = pci_clk_;

	    if (curdev.host_flag) {
		    // Outputs to host nodes...
	    } else {
		    // Outputs to device nodes...
		  curdev.send_signals["RESET#"].resize(reset_n.size());
		  curdev.send_signals["RESET#"] = reset_n;
	    }
      }
}

void PciProtocol::advance_pci_clock_(void)
{
	// Advance time 1/2 a PCI clock.
      advance_time_(16500, -12);

	// Toggle the PCI clock
      if (pci_clk_[0] == BIT_1) {
	    pci_clk_[0] = BIT_0;
      } else {
	    pci_clk_[0] = BIT_1;
      }
}

bit_state_t PciProtocol::calculate_reset_n_()
{
	// The RESET# signal is the wired AND of the RESET# outputs
	// from the host devices. If any of the hosts drives RESET#
	// low, then the output is low.
      bit_state_t reset_n = BIT_1;

      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {
	    if (! dev->second.host_flag)
		  continue;

	    valarray<bit_state_t>&tmp = dev->second.client_signals["RESET#"];

	      // Skip if not driving RESET#
	    if (tmp.size() == 0)
		  continue;
	      // Skip if driving RESET# high.
	    if (tmp[0] == BIT_1)
		  continue;
	      // If driving to X or Z, reset output may be unknown.
	    if (tmp[0] != BIT_0) {
		  reset_n = BIT_X;
		  continue;
	    }
	      // If any host is driving low, the RESET# is low.
	    reset_n = BIT_0;
	    break;
      }

      return reset_n;
}
