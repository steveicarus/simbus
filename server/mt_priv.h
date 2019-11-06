#ifndef __mt_priv_H
#define __mt_priv_H
/*
 * Copyright (c) 2019 Stephen Williams (steve@icarus.com)
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

/*
 * Context structure for PRNG in mt19937int.c
 */
struct context_s {
      int               mti;            /* the array for the state vector */
      unsigned long     mt[1023];       /* mti==N+1 means mt[N] is not init */
};

void sgenrand(struct context_s *context, unsigned long seed);
unsigned long genrand(struct context_s *context);

#endif
