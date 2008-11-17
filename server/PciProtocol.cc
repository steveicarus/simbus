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

# include  "PciProtocol.h"
# include  <iostream>
# include  <cassert>

using namespace std;

PciProtocol::PciProtocol(struct bus_state&b)
: protocol_t(b), pci_clk_(BIT_1)
{
      granted_ = -1;
}

PciProtocol::~PciProtocol()
{
}

void PciProtocol::run_init()
{
      granted_ = -1;

      if (service_lxt) {
	    make_trace_("PCI_CLK", PT_BITS);
	    make_trace_("RESET#",  PT_BITS);
	    make_trace_("FRAME#",  PT_BITS);
	    make_trace_("REQ64#",  PT_BITS);
	    make_trace_("IRDY#",   PT_BITS);
	    make_trace_("TRDY#",   PT_BITS);
	    make_trace_("STOP#",   PT_BITS);
	    make_trace_("DEVSEL#", PT_BITS);
	    make_trace_("ACK64#",  PT_BITS);
	    make_trace_("PAR",     PT_BITS);
	    make_trace_("PAR64",   PT_BITS);
	    make_trace_("AD",      PT_BITS, 64);
	    make_trace_("C/BE#",   PT_BITS, 8);
	    make_trace_("Bus owner",PT_STRING);
      }

      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    struct bus_device_plug&curdev = dev->second;

	    curdev.send_signals["PCI_CLK"].resize(1);
	    curdev.send_signals["PCI_CLK"][0] = BIT_1;

	    curdev.send_signals["GNT#"].resize(1);
	    curdev.send_signals["GNT#"][0] = BIT_1;

	    curdev.send_signals["IDSEL"].resize(1);
	    curdev.send_signals["IDSEL"][0] = BIT_Z;

	    curdev.send_signals["FRAME#"].resize(1);
	    curdev.send_signals["FRAME#"][0] = BIT_Z;

	    curdev.send_signals["REQ64#"].resize(1);
	    curdev.send_signals["REQ64#"][0] = BIT_Z;

	    curdev.send_signals["IRDY#"].resize(1);
	    curdev.send_signals["IRDY#"][0] = BIT_Z;

	    curdev.send_signals["TRDY#"].resize(1);
	    curdev.send_signals["TRDY#"][0] = BIT_Z;

	    curdev.send_signals["STOP#"].resize(1);
	    curdev.send_signals["STOP#"][0] = BIT_Z;

	    curdev.send_signals["DEVSEL#"].resize(1);
	    curdev.send_signals["DEVSEL#"][0] = BIT_Z;

	    curdev.send_signals["ACK64#"].resize(1);
	    curdev.send_signals["ACK64#"][0] = BIT_Z;

	    curdev.send_signals["PAR"].resize(1);
	    curdev.send_signals["PAR"][0] = BIT_Z;

	    curdev.send_signals["PAR64"].resize(1);
	    curdev.send_signals["PAR64"][0] = BIT_Z;

	    curdev.send_signals["C/BE#"].resize(8);
	    curdev.client_signals["C/BE#"].resize(8);
	    for (int idx = 0 ; idx < 8 ; idx += 1) {
		  curdev.send_signals["C/BE#"][idx] = BIT_Z;
		  curdev.client_signals["C/BE#"][idx] = BIT_Z;
	    }

	    curdev.send_signals["AD"].resize(64);
	    curdev.client_signals["AD"].resize(64);
	    for (int idx = 0 ; idx < 64 ; idx += 1) {
		  curdev.send_signals["AD"][idx] = BIT_Z;
		  curdev.client_signals["AD"][idx] = BIT_Z;
	    }

	    if (curdev.host_flag) {
		  valarray<bit_state_t>tmp16 (16);
		  for (int idx = 0 ; idx < 16 ; idx += 1)
			tmp16[idx] = BIT_1;

		  curdev.send_signals["INTA#"].resize(16);
		  curdev.send_signals["INTB#"].resize(16);
		  curdev.send_signals["INTC#"].resize(16);
		  curdev.send_signals["INTD#"].resize(16);
		  curdev.send_signals["INTA#"] = tmp16;
		  curdev.send_signals["INTB#"] = tmp16;
		  curdev.send_signals["INTC#"] = tmp16;
		  curdev.send_signals["INTD#"] = tmp16;

	    } else {
		  curdev.send_signals["RESET#"].resize(1);
		  curdev.send_signals["RESET#"][0] = BIT_1;
	    }
      }

}

void PciProtocol::run_run()
{
	// Step the PCI clock.
      advance_pci_clock_();

	// Calculate the RESET# signal.
      bit_state_t reset_n = calculate_reset_n_();

	// Run the arbitration state machine.
      arbitrate_();

	// Route interrupts from the devices to the hosts.
      route_interrupts_();

	// Blend all the bi-directional signals.
      blend_bi_signals_();

	// Assign the output results back to the devices. These take
	// care of the signals that are not handled otherwise.
      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++) {

	    struct bus_device_plug&curdev = dev->second;

	      // Common signals...

	    curdev.send_signals["PCI_CLK"][0] = pci_clk_;

	    if (curdev.host_flag) {
		    // Outputs to host nodes...
	    } else {
		    // Outputs to device nodes...
		  curdev.send_signals["RESET#"][0] = reset_n;
	    }
      }

      set_trace_("PCI_CLK", pci_clk_);
      set_trace_("RESET#",  reset_n);
}

void PciProtocol::advance_pci_clock_(void)
{
	// Advance time 1/2 a PCI clock.
      advance_time_(16500, -12);

	// Toggle the PCI clock
      if (pci_clk_ == BIT_1) {
	    pci_clk_ = BIT_0;
      } else {
	    pci_clk_ = BIT_1;
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

void PciProtocol::arbitrate_()
{
	// Only arbitrate on the rising edge of the clock.
      if (pci_clk_ != BIT_1)
	    return;

	// Collect all the REQ# signals from all the attached devices.
      bit_state_t req_n[16];
      for (int idx = 0 ; idx < 16 ; idx += 1)
	    req_n[idx] = BIT_1;

      int count_requests = 0;
      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    struct bus_device_plug&curdev = dev->second;
	    valarray<bit_state_t>&tmp = curdev.client_signals["REQ#"];
	    if (tmp.size() == 0)
		  continue;
	    if (tmp[0] == BIT_Z)
		  continue;

	    req_n[curdev.ident] = tmp[0];
	    if (req_n[curdev.ident] == BIT_0)
		  count_requests += 1;
      }

	// If there are no requests, then leave the GNT# signals as
	// they are. This has the effect of parking the GNT# at the
	// last device to request the bus.
      if (count_requests == 0)
	    return;

      int old_grant = granted_;

      if (granted_ < 0)
	    granted_ = 0;

      do {
	    granted_ = (granted_+1) % 16;
      } while (req_n[granted_] != BIT_0);

      if (granted_ == old_grant)
	    return;

      string master_name = "?";
      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    struct bus_device_plug&curdev = dev->second;
	    if (old_grant >= 0 && curdev.ident == old_grant) {
		  curdev.send_signals["GNT#"][0] = BIT_1;
	    } else if (curdev.ident == granted_) {
		  curdev.send_signals["GNT#"][0] = BIT_0;
		  master_name = dev->first;
	    }
      }

      cout << "Arbiter: Grant bus to " << granted_ << endl;
      set_trace_("Bus owner", master_name);
}

void PciProtocol::route_interrupts_()
{
      map<unsigned, bit_state_t> inta;
      map<unsigned, bit_state_t> intb;
      map<unsigned, bit_state_t> intc;
      map<unsigned, bit_state_t> intd;

	// Collect all the interrupt sources from the non-host devices.
      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    if (dev->second.host_flag)
		  continue;

	    bit_state_t tmp_bit;

	    valarray<bit_state_t>&tmpa = dev->second.client_signals["INTA#"];
	    if (tmpa.size() == 0)
		  tmp_bit = BIT_1;
	    else if (tmpa[0] == BIT_Z)
		  tmp_bit = BIT_1;
	    else
		  tmp_bit = tmpa[0];
	    inta[dev->second.ident] = tmp_bit;

	    valarray<bit_state_t>&tmpb = dev->second.client_signals["INTB#"];
	    if (tmpb.size() == 0)
		  tmp_bit = BIT_1;
	    else if (tmpb[0] == BIT_Z)
		  tmp_bit = BIT_1;
	    else
		  tmp_bit = tmpb[0];
	    intb[dev->second.ident] = tmp_bit;

	    valarray<bit_state_t>&tmpc = dev->second.client_signals["INTC#"];
	    if (tmpc.size() == 0)
		  tmp_bit = BIT_1;
	    else if (tmpc[0] == BIT_Z)
		  tmp_bit = BIT_1;
	    else
		  tmp_bit = tmpc[0];
	    intc[dev->second.ident] = tmp_bit;

	    valarray<bit_state_t>&tmpd = dev->second.client_signals["INTD#"];
	    if (tmpd.size() == 0)
		  tmp_bit = BIT_1;
	    else if (tmpd[0] == BIT_Z)
		  tmp_bit = BIT_1;
	    else
		  tmp_bit = tmpd[0];

	    intd[dev->second.ident] = tmp_bit;
      }

	// Send the collected interrupt values to the host devices.
      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    if (!dev->second.host_flag)
		  continue;

	    struct bus_device_plug&curdev = dev->second;

	    for (map<unsigned, bit_state_t>::iterator cur = inta.begin()
		       ; cur != inta.end() ; cur ++ ) {
		  assert(cur->first < 16);
		  curdev.send_signals["INTA#"][cur->first] = cur->second;
	    }

	    for (map<unsigned, bit_state_t>::iterator cur = intb.begin()
		       ; cur != intb.end() ; cur ++ ) {
		  assert(cur->first < 16);
		  curdev.send_signals["INTB#"][cur->first] = cur->second;
	    }

	    for (map<unsigned, bit_state_t>::iterator cur = intc.begin()
		       ; cur != intc.end() ; cur ++ ) {
		  assert(cur->first < 16);
		  curdev.send_signals["INTC#"][cur->first] = cur->second;
	    }

	    for (map<unsigned, bit_state_t>::iterator cur = intd.begin()
		       ; cur != intd.end() ; cur ++ ) {
		  assert(cur->first < 16);
		  curdev.send_signals["INTD#"][cur->first] = cur->second;
	    }
      }
}

static bit_state_t blend_bits(bit_state_t a, bit_state_t b)
{
      if (a == BIT_Z)
	    return b;
      if (b == BIT_Z)
	    return a;
      if (a == b)
	    return a;
      else
	    return BIT_X;
}

static bit_state_t subtract_feedback(bit_state_t sig, bit_state_t ref)
{
      if (ref == BIT_Z)
	    return sig;
      if (sig == ref)
	    return BIT_Z;
      return sig;
}

void PciProtocol::blend_bi_signals_(void)
{
      bit_state_t frame_n = BIT_Z;
      bit_state_t req64_n = BIT_Z;
      bit_state_t devsel_n= BIT_Z;
      bit_state_t ack64_n = BIT_Z;
      bit_state_t irdy_n  = BIT_Z;
      bit_state_t trdy_n  = BIT_Z;
      bit_state_t stop_n  = BIT_Z;
      valarray<bit_state_t> ad (64);
      valarray<bit_state_t> cbe(8);
      bit_state_t par = BIT_Z;
      bit_state_t par64 = BIT_Z;

      for (int idx = 0 ; idx < 64 ; idx += 1)
	    ad[idx] = BIT_Z;
      for (int idx = 0 ; idx < 8 ; idx += 1)
	    cbe[idx] = BIT_Z;

      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    struct bus_device_plug&curdev = dev->second;
	    bit_state_t tmp;

	    tmp = curdev.client_signals["FRAME#"][0];
	    frame_n = blend_bits(frame_n, tmp);

	    tmp = curdev.client_signals["REQ64#"][0];
	    req64_n = blend_bits(req64_n, tmp);

	    tmp = curdev.client_signals["DEVSEL#"][0];
	    devsel_n = blend_bits(devsel_n, tmp);

	    tmp = curdev.client_signals["ACK64#"][0];
	    ack64_n = blend_bits(ack64_n, tmp);

	    tmp = curdev.client_signals["IRDY#"][0];
	    irdy_n = blend_bits(irdy_n, tmp);

	    tmp = curdev.client_signals["TRDY#"][0];
	    trdy_n = blend_bits(trdy_n, tmp);

	    tmp = curdev.client_signals["STOP#"][0];
	    stop_n = blend_bits(stop_n, tmp);

	    valarray<bit_state_t>&tmp_ad = curdev.client_signals["AD"];
	    for (int idx = 0 ; idx < 64 ; idx += 1)
		  ad[idx] = blend_bits(ad[idx], tmp_ad[idx]);

	    valarray<bit_state_t>&tmp_cbe = curdev.client_signals["C/BE#"];
	    for (int idx = 0 ; idx < 8 ; idx += 1)
		  cbe[idx] = blend_bits(cbe[idx], tmp_cbe[idx]);

	    tmp = curdev.client_signals["PAR"][0];
	    par = blend_bits(par, tmp);

	    tmp = curdev.client_signals["PAR64"][0];
	    par64 = blend_bits(par64, tmp);
      }

      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    struct bus_device_plug&curdev = dev->second;

	    set_trace_("FRAME#", frame_n);
	    set_trace_("REQ64#", req64_n);
	    set_trace_("IRDY#",  irdy_n);
	    set_trace_("TRDY#",  trdy_n);
	    set_trace_("STOP#",  stop_n);
	    set_trace_("DEVSEL#",devsel_n);
	    set_trace_("ACK64#", ack64_n);

	    curdev.send_signals["FRAME#"][0] = subtract_feedback(frame_n, curdev.client_signals["FRAME#"][0]);
	    curdev.send_signals["REQ64#"][0] = subtract_feedback(req64_n, curdev.client_signals["REQ64#"][0]);
	    curdev.send_signals["IRDY#"][0]  = subtract_feedback(irdy_n, curdev.client_signals["IRDY#"][0]);
	    curdev.send_signals["TRDY#"][0]  = subtract_feedback(trdy_n, curdev.client_signals["TRDY#"][0]);
	    curdev.send_signals["STOP#"][0]  = subtract_feedback(stop_n, curdev.client_signals["STOP#"][0]);
	    curdev.send_signals["DEVSEL#"][0]= subtract_feedback(devsel_n, curdev.client_signals["DEVSEL#"][0]);
	    curdev.send_signals["ACK64#"][0] = subtract_feedback(ack64_n, curdev.client_signals["ACK64#"][0]);

	    curdev.send_signals["IDSEL"][0]  = ad[curdev.ident+16];

	    valarray<bit_state_t>&tmp_ad = curdev.send_signals["AD"];
	    valarray<bit_state_t>&cli_ad = curdev.client_signals["AD"];
	    for (int idx = 0 ; idx < 64 ; idx += 1)
		  tmp_ad[idx] = subtract_feedback(ad[idx], cli_ad[idx]);

	    valarray<bit_state_t>&tmp_cbe = curdev.send_signals["C/BE#"];
	    valarray<bit_state_t>&cli_cbe = curdev.client_signals["C/BE#"];
	    for (int idx = 0 ; idx < 8 ; idx += 1)
		  tmp_cbe[idx] = subtract_feedback(cbe[idx], cli_cbe[idx]);

	    set_trace_("AD", ad);
	    set_trace_("C/BE#", cbe);

	    curdev.send_signals["PAR"][0]   = subtract_feedback(par, curdev.client_signals["PAR"][0]);
	    curdev.send_signals["PAR64"][0] = subtract_feedback(par64, curdev.client_signals["PAR64"][0]);

	    set_trace_("PAR",    par);
	    set_trace_("PAR64",  par64);
      }
}
