/*
 * Copyright (c) 2008-2010 Stephen Williams (steve@icarus.com)
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

/*
 * The clock_phase_map describes the phases of the clock. The phase
 * states are chosen to give clients a chance to participate in the
 * protocol in time.
 *
 *   Phase:         C        D A B        C
 *                  |        | | |        |
 *                  V        V V V        V
 *       +----------+          +----------+
 *       |          |          |          |
 *       |          |          |          |
 *  -----+          +----------+          +----------
 */


const PciProtocol::clock_phase_map_t PciProtocol::clock_phase_map33[4] = {
      { BIT_1,   2000 }, // A  - Posedge
      { BIT_1,  13000 }, // B  - Hold
      { BIT_0,  13000 }, // C  - Negedge
      { BIT_0,   2000 }  // D  - Setup
};

const PciProtocol::clock_phase_map_t PciProtocol::clock_phase_map66[4] = {
      { BIT_1,   2000 }, // A  - Posedge
      { BIT_1,   5500 }, // B  - Hold
      { BIT_0,   5500 }, // C  - Negedge
      { BIT_0,   2000 }  // D  - Setup
};

PciProtocol::PciProtocol(struct bus_state*b)
: protocol_t(b), phase_(0), req_n_(16)
{
      granted_ = 0;
      clock_phase_map_ = clock_phase_map33;
      park_mode_ = GNT_PARK_NONE;
      gnt_linger_ = 16;

      string bus_speed = b->options["bus_speed"];
      if (bus_speed == "") {
	    clock_phase_map_ = clock_phase_map33;
      } else if (bus_speed == "33") {
	    clock_phase_map_ = clock_phase_map33;
      } else if (bus_speed == "66") {
	    clock_phase_map_ = clock_phase_map66;
      }

      string bus_park = b->options["bus_park"];
      if (bus_park == "") {
	    park_mode_ = GNT_PARK_NONE;
      } else if (bus_park == "none") {
	    park_mode_ = GNT_PARK_NONE;
      } else if (bus_park == "last") {
	    park_mode_ = GNT_PARK_LAST;
      }

      string gnt_linger = b->options["gnt_linger"];
      if (gnt_linger != "") {
	    gnt_linger_ = strtoul(gnt_linger.c_str(), 0, 0);
      } else {
	    gnt_linger_ = 16;
      }
}

PciProtocol::~PciProtocol()
{
}

void PciProtocol::trace_init()
{
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
      make_trace_("AD",      PT_BITS, 32);
      make_trace_("AD64",    PT_BITS, 32);
      make_trace_("C/BE#",   PT_BITS, 4);
      make_trace_("C/BE64#", PT_BITS, 4);
      make_trace_("INTA#",   PT_BITS, 16);
      make_trace_("INTB#",   PT_BITS, 16);
      make_trace_("INTC#",   PT_BITS, 16);
      make_trace_("INTD#",   PT_BITS, 16);
      make_trace_("REQ#",    PT_BITS, 16);
      make_trace_("Bus grant",PT_STRING);
      make_trace_("Bus master",PT_STRING);
}

void PciProtocol::run_init()
{
      granted_ = 0;
      master_ = 0;
      for (int idx = 0 ; idx < 16 ; idx += 1)
	    req_n_[idx] = BIT_1;

      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    struct bus_device_plug&curdev = *(dev->second);

	    curdev.send_signals["PCI_CLK"].resize(1);
	    curdev.send_signals["PCI_CLK"][0] = BIT_1;

	    curdev.send_signals["GNT#"].resize(1);
	    curdev.send_signals["GNT#"][0] = BIT_1;

	    curdev.send_signals["IDSEL"].resize(1);
	    curdev.send_signals["IDSEL"][0] = BIT_Z;

	    curdev.send_signals["FRAME#"].resize(1);
	    curdev.send_signals["FRAME#"][0] = BIT_Z;
	    curdev.client_signals["FRAME#"].resize(1);
	    curdev.client_signals["FRAME#"][0] = BIT_Z;

	    curdev.send_signals["REQ64#"].resize(1);
	    curdev.send_signals["REQ64#"][0] = BIT_Z;
	    curdev.client_signals["REQ64#"].resize(1);
	    curdev.client_signals["REQ64#"][0] = BIT_Z;

	    curdev.send_signals["IRDY#"].resize(1);
	    curdev.send_signals["IRDY#"][0] = BIT_Z;
	    curdev.client_signals["IRDY#"].resize(1);
	    curdev.client_signals["IRDY#"][0] = BIT_Z;

	    curdev.send_signals["TRDY#"].resize(1);
	    curdev.send_signals["TRDY#"][0] = BIT_Z;
	    curdev.client_signals["TRDY#"].resize(1);
	    curdev.client_signals["TRDY#"][0] = BIT_Z;

	    curdev.send_signals["STOP#"].resize(1);
	    curdev.send_signals["STOP#"][0] = BIT_Z;
	    curdev.client_signals["STOP#"].resize(1);
	    curdev.client_signals["STOP#"][0] = BIT_Z;

	    curdev.send_signals["DEVSEL#"].resize(1);
	    curdev.send_signals["DEVSEL#"][0] = BIT_Z;
	    curdev.client_signals["DEVSEL#"].resize(1);
	    curdev.client_signals["DEVSEL#"][0] = BIT_Z;

	    curdev.send_signals["ACK64#"].resize(1);
	    curdev.send_signals["ACK64#"][0] = BIT_Z;
	    curdev.client_signals["ACK64#"].resize(1);
	    curdev.client_signals["ACK64#"][0] = BIT_Z;

	    curdev.send_signals["PAR"].resize(1);
	    curdev.send_signals["PAR"][0] = BIT_Z;
	    curdev.client_signals["PAR"].resize(1);
	    curdev.client_signals["PAR"][0] = BIT_Z;

	    curdev.send_signals["PAR64"].resize(1);
	    curdev.send_signals["PAR64"][0] = BIT_Z;
	    curdev.client_signals["PAR64"].resize(1);
	    curdev.client_signals["PAR64"][0] = BIT_Z;

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

      bit_state_t pci_clk = clock_phase_map_[phase_].clk_val;

	// Track the REQ# inputs from the devices.
      track_req_n_();

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

	    struct bus_device_plug*curdev = dev->second;

	      // Common signals...

	    curdev->send_signals["PCI_CLK"][0] = pci_clk;

	    if (curdev->host_flag) {
		    // Outputs to host nodes...
	    } else {
		    // Outputs to device nodes...
		  curdev->send_signals["RESET#"][0] = reset_n;
	    }
      }

      set_trace_("PCI_CLK", pci_clk);
      set_trace_("RESET#",  reset_n);
}

void PciProtocol::advance_pci_clock_(void)
{
	// Advance the phase pointer
      phase_ = (phase_ + 1) % 4;

	// Advance time for the next phase
      advance_time_(clock_phase_map_[phase_].duration_ps, -12);
}

bit_state_t PciProtocol::calculate_reset_n_()
{
	// The RESET# signal is the wired AND of the RESET# outputs
	// from the host devices. If any of the hosts drives RESET#
	// low, then the output is low.
      bit_state_t reset_n = BIT_1;

      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {
	    if (! dev->second->host_flag)
		  continue;

	    valarray<bit_state_t>&tmp = dev->second->client_signals["RESET#"];

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

void PciProtocol::track_req_n_()
{
	// Only sample the requests on the rising edge of the PCI clock.
      if (phase_ != 0)
	    return;

	// Collect all the REQ# signals from all the attached devices.

      for (int idx = 0 ; idx < 16 ; idx += 1)
	    req_n_[idx] = BIT_1;

      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    struct bus_device_plug*curdev = dev->second;
	    valarray<bit_state_t>&tmp = curdev->client_signals["REQ#"];
	    if (tmp.size() == 0)
		  continue;
	    if (tmp[0] == BIT_Z)
		  continue;

	    req_n_[curdev->ident] = tmp[0];
      }

      set_trace_("REQ#", req_n_);

      if (master_ == 0 && granted_ == 0) {
	    set_trace_("Bus master", "<>");

      } else if (master_ == 0) {
	    assert(granted_);
	      // There is no other master, so give the bus over to the
	      // granted device. This may wind up being provisional,
	      // but is likely to be the case.
	    valarray<bit_state_t>&frame_g = granted_->client_signals["FRAME#"];
	    if (frame_g[0] == BIT_0) {
		  master_ = granted_;
		  set_trace_("Bus master", master_->name);
	    }

      } else if (master_ == granted_) {
	    assert(master_);
	      // The master is granted, so it holds the bus whether it
	      // is using it or not.

      } else if (granted_) {
	    assert(master_ && master_ != granted_);

	      // The current master is not the granted device. Make
	      // sure it is still holding the bus. If not, turn it
	      // over to the granted device.
	    valarray<bit_state_t>&frame_m = master_->client_signals["FRAME#"];
	    valarray<bit_state_t>&irdy_m  = master_->client_signals["IRDY#"];
	    valarray<bit_state_t>&frame_g = granted_->client_signals["FRAME#"];

	    if (frame_m[0] == BIT_0 || irdy_m[0] == BIT_0) {
		  ; // Current master holds on...
	    } else  {
		    // Give bus to grantee.
		  master_ = granted_;
		  set_trace_("Bus master", master_->name);
	    }

      } else {
	    assert(master_ && granted_ == 0);

	      // Nobody is granted at the moment. If the current
	      // master gives up the bus then clear the master.
	    valarray<bit_state_t>&frame_m = master_->client_signals["FRAME#"];
	    valarray<bit_state_t>&irdy_m  = master_->client_signals["IRDY#"];

	      // If master is no longer granted, and frame is not
	      // active, then it no longer owns the bus.
	    if (frame_m[0] != BIT_0 && irdy_m[0] != BIT_0) {
		  master_ = 0;
		  set_trace_("Bus master", "<>");
	    }
      }
}

void PciProtocol::arbitrate_()
{
	// Only arbitrate on the rising edge of the clock. So
	// arbitration results are sent out after the HOLD time.
      if (phase_ != 1)
	    return;

      int count_requests = 0;
      for (int idx = 0 ; idx < 16 ; idx += 1)
	    if (req_n_[idx] == BIT_0) count_requests += 1;

	// If there are no requests, then leave the GNT# signals as
	// they are. This has the effect of parking the GNT# at the
	// last device to request the bus.
      if (count_requests == 0) {
	    if (master_ && granted_
		&& park_mode_ != GNT_PARK_LAST
		&& (lrand_()%gnt_linger_ == 0)) {
		    // Nobody's requesting, but a master is holding
		    // the bus. Recall the grant, just to demonstrate
		    // the client's ability to handle that.
		  granted_->send_signals["GNT#"][0] = BIT_1;
		  granted_ = 0;
		  set_trace_("Bus grant", "<>");
	    }
	    return;
      }

	// If the current grantee is still requesting, then don't take
	// the bus away from it.
      if (granted_ && req_n_[granted_->ident] == BIT_0)
	    return;

      int old_grant = granted_? granted_->ident : -1;
      int new_grant = granted_? granted_->ident : (lrand_()%16);

      do {
	    new_grant = (new_grant+1) % 16;
      } while (req_n_[new_grant] != BIT_0);

      if (new_grant == old_grant)
	    return;

	// Get the device object for the new grantee.
      struct bus_device_plug*new_dev = 0;
      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev++) {
	    if (dev->second->ident == new_grant) {
		  new_dev = dev->second;
		  break;
	    }
      }
      if (new_dev == 0) {
	    cerr << "INTERNAL ERROR: new_grant=" << new_grant
		 << ", old_grant=" << old_grant
		 << ", new_dev=(nil)"
		 << ", count_requests=" << count_requests
		 << ", req_n_=" << req_n_ << endl;
      }
      assert(new_dev);

	// Set the GNT# for the new device and clear it for the old
	// device. This should always leave us with no more then 1
	// device granted.
      new_dev->send_signals["GNT#"][0] = BIT_0;
      if (granted_)
	    granted_->send_signals["GNT#"][0] = BIT_1;

      granted_ = new_dev;

	// Indicate where the grant is being sent. Note that exactly
	// one GNT# is ever active, so we can be clever and give the
	// client name here.
      set_trace_("Bus grant", granted_->name);
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

	    if (dev->second->host_flag)
		  continue;

	    bit_state_t tmp_bit;

	    valarray<bit_state_t>&tmpa = dev->second->client_signals["INTA#"];
	    if (tmpa.size() == 0)
		  tmp_bit = BIT_1;
	    else if (tmpa[0] == BIT_Z)
		  tmp_bit = BIT_1;
	    else
		  tmp_bit = tmpa[0];
	    inta[dev->second->ident] = tmp_bit;

	    valarray<bit_state_t>&tmpb = dev->second->client_signals["INTB#"];
	    if (tmpb.size() == 0)
		  tmp_bit = BIT_1;
	    else if (tmpb[0] == BIT_Z)
		  tmp_bit = BIT_1;
	    else
		  tmp_bit = tmpb[0];
	    intb[dev->second->ident] = tmp_bit;

	    valarray<bit_state_t>&tmpc = dev->second->client_signals["INTC#"];
	    if (tmpc.size() == 0)
		  tmp_bit = BIT_1;
	    else if (tmpc[0] == BIT_Z)
		  tmp_bit = BIT_1;
	    else
		  tmp_bit = tmpc[0];
	    intc[dev->second->ident] = tmp_bit;

	    valarray<bit_state_t>&tmpd = dev->second->client_signals["INTD#"];
	    if (tmpd.size() == 0)
		  tmp_bit = BIT_1;
	    else if (tmpd[0] == BIT_Z)
		  tmp_bit = BIT_1;
	    else
		  tmp_bit = tmpd[0];

	    intd[dev->second->ident] = tmp_bit;
      }

	// Send the collected interrupt values to the host devices.
      for (bus_device_map_t::iterator dev = device_map().begin()
		 ; dev != device_map().end() ; dev ++ ) {

	    if (!dev->second->host_flag)
		  continue;

	    struct bus_device_plug*curdev = dev->second;

	    for (map<unsigned, bit_state_t>::iterator cur = inta.begin()
		       ; cur != inta.end() ; cur ++ ) {
		  assert(cur->first < 16);
		  curdev->send_signals["INTA#"][cur->first] = cur->second;
	    }

	    for (map<unsigned, bit_state_t>::iterator cur = intb.begin()
		       ; cur != intb.end() ; cur ++ ) {
		  assert(cur->first < 16);
		  curdev->send_signals["INTB#"][cur->first] = cur->second;
	    }

	    for (map<unsigned, bit_state_t>::iterator cur = intc.begin()
		       ; cur != intc.end() ; cur ++ ) {
		  assert(cur->first < 16);
		  curdev->send_signals["INTC#"][cur->first] = cur->second;
	    }

	    for (map<unsigned, bit_state_t>::iterator cur = intd.begin()
		       ; cur != intd.end() ; cur ++ ) {
		  assert(cur->first < 16);
		  curdev->send_signals["INTD#"][cur->first] = cur->second;
	    }

	    set_trace_("INTA#", curdev->send_signals["INTA#"]);
	    set_trace_("INTB#", curdev->send_signals["INTB#"]);
	    set_trace_("INTC#", curdev->send_signals["INTC#"]);
	    set_trace_("INTD#", curdev->send_signals["INTD#"]);
      }
}

static bit_state_t blend_bits(bit_state_t a, bit_state_t b)
{
      assert(a < 4);
      assert(b < 4);

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

	    struct bus_device_plug&curdev = *(dev->second);
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

	    struct bus_device_plug&curdev = *(dev->second);

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

	    set_trace_("AD",   ad[slice( 0,32,1)]);
	    set_trace_("AD64", ad[slice(32,32,1)]);
	    set_trace_("C/BE#",   cbe[slice(0,4,1)]);
	    set_trace_("C/BE64#", cbe[slice(4,4,1)]);

	    curdev.send_signals["PAR"][0]   = subtract_feedback(par, curdev.client_signals["PAR"][0]);
	    curdev.send_signals["PAR64"][0] = subtract_feedback(par64, curdev.client_signals["PAR64"][0]);

	    set_trace_("PAR",    par);
	    set_trace_("PAR64",  par64);
      }
}
