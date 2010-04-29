#ifndef __simbus_p2p_priv_H
#define __simbus_p2p_priv_H
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

# include  "simbus_priv.h"

struct simbus_p2p_s {
	/* The name given in the simbus_p2p_connect function. This is
	   also the name sent to the server in order to get my id. */
      char*name;
	/* POSIX fd for the socket used to connect with the server. */
      int fd;
	/* Identifier returned by the server during connect. */
      unsigned ident;

	/* Current simulation time. */
      uint64_t time_mant;
      int time_exp;

      bus_bitval_t clock;

      unsigned width_o;
      bus_bitval_t*data_o;

      unsigned width_i;
      bus_bitval_t*data_i;
};

#endif
