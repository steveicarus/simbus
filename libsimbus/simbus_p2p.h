#ifndef __simbus_p2p_H
#define __simbus_p2p_H
/*
 * Copyright (c) 2010 Stephen Williams (steve@icarus.com)
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

# include  <inttypes.h>

#ifdef __cplusplus
# define EXTERN extern "C"
#else
# define EXTERN extern
#endif

typedef struct simbus_p2p_s* simbus_p2p_t;

/*
 * Open the connection to the bus server for a point-to-point bus. The
 * name is the devname to use. The width_i and width_o arguments
 * describe the size of the data paths, with width_o for the data path
 * toward the client, and width_i the data path toward the host.
 */
EXTERN simbus_p2p_t simbus_p2p_connect(const char*server, const char*name,
				       unsigned width_i, unsigned width_o);

/*
 * Whether this device is a host or device depends on the ident that
 * was configured in the server. The point-to-point protocol requires
 * that hosts have ident of zero so that this library can detect which
 * it is and act accordingly.
 */
EXTERN int simbus_p2p_is_host(simbus_p2p_t bus);

/*
 * Read the current input (from the client) data values, or set the
 * output values. These functions do not move the clock, so time is
 * not advanced and output data is not sent until the clock is
 * advanced.
 *
 * The data is arranged is little-endian order in the data_i/o
 * array. The first 32bits are in data_i[0]/data_o[0] and so
 * on. Within words, the first bit is the least significant bit.
 *
 * The simbus_p2p_in() function should only be used after a
 * clock_posedge. The data_i is what the client transmitted for the
 * setup time.
 *
 * If simbus_p2p_out() function is called after a clock_posedge, the
 * output data is changed (as the client sees it) early, after the
 * hold time. If simbus_p2p_out() is called after a clock_negedge, the
 * output data is changed late, at the setup time. To simulate
 * worst-case timing, it is usual to change the output data late.
 *
 * The simbus_p2p_in will return <0 if there are X or Z bits in the
 * input value, and will replace the XZ bits with 0.
 */
EXTERN int simbus_p2p_in(simbus_p2p_t bus, uint32_t*data_i);
EXTERN void simbus_p2p_out(simbus_p2p_t bus, const uint32_t*data_o);

/*
 * These are versions of the in/out functions for bus clients. Whereas
 * the host writes to the bus output and reads from the bus input,
 * clients write to the bus input and read from the bus output.
 */
EXTERN void simbus_p2p_in_poke(simbus_p2p_t bus, const uint32_t*data);
EXTERN int simbus_p2p_out_peek(simbus_p2p_t bus, uint32_t*data);

/*
 * The master can control the clock for the slave. Use this function
 * to set the clock running for the slave, or to stop it at 1, 0 or
 * HiZ. This works as a signal that the server intercepts, so it will
 * take effect at the next clock phase.
 */
EXTERN void simbus_p2p_clock_mode(simbus_p2p_t bus, int mode);
# define SIMBUS_P2P_CLOCK_RUN   0
# define SIMBUS_P2P_CLOCK_STOP0 1
# define SIMBUS_P2P_CLOCK_STOP1 2
# define SIMBUS_P2P_CLOCK_STOPZ 3

/*
 * Advance the point-to-point bus clock for a number of cycles. When
 * this function is done, the clock has just risen/fallen. The
 * "cycles" count must be >= 1. If 1, then the clock is advanced to
 * the next edge.
 */
EXTERN int simbus_p2p_clock_posedge(simbus_p2p_t bus, unsigned cycles);
EXTERN int simbus_p2p_clock_negedge(simbus_p2p_t bus, unsigned cycles);

/*
 * Send an end-of-simulation message to the simulator, then dosconnect
 * and close the bus object. Only HOST devices should call the
 * simbus_p2p_end_simulation function. Other devices should call the
 * simbus_p2p_disconnect function instead.
 */
EXTERN void simbus_p2p_end_simulation(simbus_p2p_t bus);
EXTERN void simbus_p2p_disconnect(simbus_p2p_t bus);

# undef EXTERN

#endif
