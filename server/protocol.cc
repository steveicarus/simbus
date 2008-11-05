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
: protocol_t(b)
{
      pci_clk_ = BIT_1;
}

PciProtocol::~PciProtocol()
{
}

void PciProtocol::run_run()
{
      advance_time_(16500, -12);

      if (pci_clk_ == BIT_1) {
	    pci_clk_ = BIT_0;
      } else {
	    pci_clk_ = BIT_1;
      }

      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++) {
	    dev->second.send_signals["PCI_CLK"].resize(1);
	    dev->second.send_signals["PCI_CLK"][0] = pci_clk_;
      }
}
