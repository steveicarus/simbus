#ifndef __simbus_priv_H
#define __simbus_priv_H
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

# include  <stddef.h>
# include  <inttypes.h>
# include  <stdio.h>

typedef enum { BIT_0, BIT_1, BIT_X, BIT_Z } bus_bitval_t;

extern char __bitval_to_char(bus_bitval_t val);
extern bus_bitval_t __char_to_bitval(char val);

/*
 * Use an abstract server descriptor string to open the socket
 * connection to the server. The result is the posix fd for the
 * socket, or <0 if there was an error.
 */
extern int __simbus_server_socket(const char*addr);

/*
 * Send to the server a HELLO record with the name of this device. The
 * server is expected to respond with the "YOU-ARE" response that
 * contains the ident. The ident is written to the *ident value and
 * this function returns 0. If there was an error, this function
 * returns <0.
 *
 * The argv is an array of arguments sent with the HELLO
 * command. These should already be formatted as <key>=<value> tokens.
 */
extern int __simbus_server_hello(int server_fd, const char*name, unsigned*ident,
				 int argc, char**argv);

/*
 * Set to the server a FINISH record and wait for the FINISH response.
 */
extern int __simbus_server_finish(int server_fd);

/*
 * Send a preformatted command to the server, then receive the
 * response back.
 *
 * The outgoing command is in the "buf" buffer terminated by a nul.
 *
 * The response is placed in the buf, assuming it is "buf_size"
 * bytes. The response is also chopped up into tokens and the argv
 * array is filled in with pointers to each token.
 *
 * The return value is the number of tokens in the response, or <0 if
 * there is an error.
 */
extern int __simbus_server_send_recv(int server_fd, char*buf, size_t buf_size,
				     int max_argc, char*argv[], FILE*debug);


/*
 * Simbus times as understood by the server are (mant * (10**(texp))) seconds.
 */
struct simbus_time_s {
      uint64_t time_mant;
      int      time_exp;
};

static inline void init_simbus_time(struct simbus_time_s*timp)
{
      timp->time_mant = 0;
      timp->time_exp = 0;
}

extern void __parse_time_token(const char*token, struct simbus_time_s*timp);
extern double __time_as_double(const struct simbus_time_s*timp, int scale);

/*
 * Draw out the signal values in the format for the "READY..."
 * message. Include a leading SP so that this effectively appends the
 * token to an existing READY message line.
 *
 * Vectors are written out MSB first (the simbus interchange
 * standard), but are assumed to be stored in the vector LSB
 * first. Thus, the bits of the val array are reversed on output.
 */
extern size_t __ready_signal(char*dst, const char*name, const bus_bitval_t*val, size_t nval);

extern void __until_signal(const char*src, bus_bitval_t*val, size_t nval);

#endif
