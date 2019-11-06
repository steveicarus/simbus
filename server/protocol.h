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
# include  "simtime.h"
# include  <stdlib.h>
struct bus_state;

extern "C" {
# include  "mt_priv.h"
}

class protocol_t  {

    public:
      explicit protocol_t(struct bus_state*);
      virtual ~protocol_t();

	// Return the current protocol/bus time for this protocol
	// instance.
      const simtime_t& peek_time() const { return time_; }

	// Wrap up the collection of configuration options, and check
	// that all is OK. Return false if there is a problem.
      virtual bool wrap_up_configuration();

	// Called by the server to set up LXT traces.
      virtual void trace_init();

	// Call this called by the server once before the first call
	// to bus_ready. The derived class uses this opportunity to
	// configure the clients (and traces) with all the various
	// signals of the protocol.
      virtual void run_init();

	// This method is called by the server when all the clients
	// for a bus are marked as ready.
      void bus_ready();

    protected:
	// Access the devices of the bus. The derived class mostly is
	// interested in the signals to and from the client.
      bus_device_map_t& device_map();

      inline std::string get_option(const std::string&key)
      { return bus_->options[key]; }

	// Functions to facilitate server-side tracing of the bus. The
	// protocol creates the traces and sets their value with these
	// functions.
      enum trace_type_t { PT_BITS, PT_STRING };
      void make_trace_(const char*lab, trace_type_t lt_type, int wid=1);
      void set_trace_(const char*lab, bit_state_t bit);
      void set_trace_(const char*lab, const std::valarray<bit_state_t>&bit);
      void set_trace_(const char*lab, const std::string&bit);

	// The derived protocol knows when the next interesting event
	// will be, and it uses this method to advance the clock to
	// that time.
      void advance_time_(uint64_t mant, int exp);

      inline long lrand_(void) {
	    unsigned long tmp = genrand(&rand_state_);
	    return tmp;
      }

    private:
	// The derived class implements this method to process its
	// signals at the current synchronization point. The base
	// class does the basic signal collection, then calls this
	// method to give the derived class a chance to set the new
	// signal values.
      virtual void run_run() =0;

    private:
      struct context_s rand_state_;
      struct bus_state*bus_;

      simtime_t time_;

      std::map<std::string,struct lxt2_wr_symbol*>signal_trace_map;

    private: // Not implemented
      protocol_t(const protocol_t&);
      protocol_t& operator= (const protocol_t&);
};

#endif
