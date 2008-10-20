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
#ident "$Id:$"

# include  <vpi_user.h>

static struct t_vpi_systf_data simbus_connect_tf = {
      vpiSysFunc,
      vpiSysFuncInt,
      "$simbus_connect",
      simbus_hello_calltf,
      simbus_hello_compiletf,
      0 /* sizetf */,
      "$simbus_hello"
};

static struct t_vpi_systf_data simbus_ready_tf = {
      vpiSysTask,
      0,
      "$simbus_ready",
      simbus_ready_calltf,
      simbus_ready_compiletf,
      0 /* sizetf */,
      "$simbus_ready"
};

static struct t_vpi_systf_data simbus_until_tf = {
      vpiSysTask,
      0,
      "$simbus_until",
      simbus_until_calltf,
      simbus_until_compiletf,
      0 /* sizetf */,
      "$simbus_until"
};

static void simbus_register(void)
{
      vpi_register_systf(&simbus_connect_tf);
      vpi_register_systf(&simbus_ready_tf);
      vpi_register_systf(&simbus_until_tf);
}

void (*vlog_startup_routines[])(void) = {
      simbus_register,
      0
};

/*
 * $Log: $
 */

