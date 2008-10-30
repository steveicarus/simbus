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
# include  <sys/types.h>
# include  <sys/socket.h>
# include  <netdb.h>
# include  <unistd.h>
# include  <stdlib.h>
# include  <string.h>
# include  "priv.h"
# include  <assert.h>

/*
 * Implement these system tasks/functions:
 *
 * $simbus_connect
 *
 * $simbus_ready
 *
 * $simbus_until
 */

# define MAX_INSTANCES 32
/* Largest message length, including the newline. */
# define MAX_MESSAGE 4096

struct port_instance {
	/* This is the name that I want to be. Use it for
	   human-readable messages, and also as a key when connecting
	   to the server. */
      char*name;

	/* This fd is the socket that is connected to the bus server. */
      int fd;

	/* this is the identifier that I get back from the bus when I
	   connect. This is used to select the correct instance of
	   non-shared bus signals. */
      unsigned ident;

	/* Use these buffers to manage data received from the server. */
      char   read_buf[MAX_MESSAGE+1];
      size_t read_fil;

} instance_table[MAX_INSTANCES];

/*
 * Read the next network message from the specified server
 * connection. This function will manage the read buffer to get text
 * until the message is complete.
 */
static int read_message(int idx, char*buf, size_t nbuf)
{
      assert(idx < MAX_INSTANCES);
      struct port_instance*inst = instance_table + idx;
      assert(inst->name != 0);

      for (;;) {
	    char*cp;
	      /* If there is a line in the buffer now, then pull that
		 line out of the read buffer and give it to the
		 caller. */
	    if ( (cp = strchr(inst->read_buf, '\n')) != 0 ) {
		  size_t len = cp - inst->read_buf;
		  assert(len < nbuf);

		  *cp++ = 0;
		  strcpy(buf, inst->read_buf);
		  assert(len < inst->read_fil);
		  inst->read_fil -= len+1;
		  if (inst->read_fil > 0)
			memmove(inst->read_buf, cp, inst->read_fil);

		  inst->read_buf[inst->read_fil] = 0;
		  return len;
	    }

	      /* Read more data from the remote. */
	    size_t trans = sizeof inst->read_buf - inst->read_fil - 1;
	    int rc = read(inst->fd, inst->read_buf+inst->read_fil, trans);
	    if (rc < 0) return rc;
	    assert(rc > 0);
	    inst->read_fil += rc;
	    inst->read_buf[inst->read_fil] = 0;
      }
}


/*
 * This routine returns 1 if the argument supports a valid string value,
 * otherwise it returns 0.
 */
static int is_string_obj(vpiHandle obj)
{
    int rtn = 0;

    assert(obj);

    switch(vpi_get(vpiType, obj)) {
      case vpiConstant:
      case vpiParameter: {
	  /* These must be a string or binary constant. */
	PLI_INT32 ctype = vpi_get(vpiConstType, obj);
	if (ctype == vpiStringConst || ctype == vpiBinaryConst) rtn = 1;
	break;
      }
    }

    return rtn;
}

static PLI_INT32 simbus_connect_compiletf(char*my_name)
{
      vpiHandle callh = vpi_handle(vpiSysTfCall, 0);
      vpiHandle argv = vpi_iterate(vpiArgument, callh);

      /* Check that there is an argument and that it is a string. */
      if (argv == 0) {
            vpi_printf("ERROR: %s line %d: ", vpi_get_str(vpiFile, callh),
                       (int)vpi_get(vpiLineNo, callh));
            vpi_printf("%s requires a single string argument.\n", my_name);
            vpi_control(vpiFinish, 1);
            return 0;
      }

      if (! is_string_obj(vpi_scan(argv))) {
            vpi_printf("ERROR: %s line %d: ", vpi_get_str(vpiFile, callh),
                       (int)vpi_get(vpiLineNo, callh));
            vpi_printf("%s's argument must be a constant string.\n", my_name);
            vpi_control(vpiFinish, 1);
      }

      /* Make sure there are no extra arguments. */
      if (vpi_scan(argv) != 0) {
	    char msg [64];
	    unsigned argc;

	    snprintf(msg, 64, "ERROR: %s line %d:",
	             vpi_get_str(vpiFile, callh),
	             (int)vpi_get(vpiLineNo, callh));

	    argc = 1;
	    while (vpi_scan(argv)) argc += 1;

            vpi_printf("%s %s takes a single string argument.\n", msg, my_name);
            vpi_printf("%*s Found %u extra argument%s.\n",
	               (int) strlen(msg), " ", argc, argc == 1 ? "" : "s");
            vpi_control(vpiFinish, 1);
      }

      return 0;
}

static PLI_INT32 simbus_connect_calltf(char*my_name)
{
      int idx;
      char buf[1024];
      s_vpi_value value;
      s_vpi_vlog_info vlog;

      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);
      vpiHandle arg;
      assert(argv);

	/* Get the one and only argument, and get its string value. */
      arg = vpi_scan(argv);
      assert(arg);

      value.format = vpiStringVal;
      vpi_get_value(arg, &value);
      char*dev_name = strdup(value.value.str);
      vpi_free_object(argv);

	/* Synthesize a bus server argument string and search for it
	   on the command line. That string will have the host and
	   port number (or just port number) for the bus that we are
	   supposed to connect to. */
      snprintf(buf, sizeof buf,
	       "+simbus-%s-bus=", dev_name);

      vpi_get_vlog_info(&vlog);
      char*host_string = 0;
      for (idx = 0 ; idx < vlog.argc ; idx += 1) {
	    if (strncmp(vlog.argv[idx], buf, strlen(buf)) == 0) {
		  host_string = strdup(vlog.argv[idx]+strlen(buf));
		  break;
	    }
      }

      if (host_string == 0) {
	    vpi_printf("%s:%d: %s(%s) cannot find %s<server> on command line\n",
		       vpi_get_str(vpiFile, sys),
		       (int)vpi_get(vpiLineNo, sys),
		       my_name, dev_name, buf);

	    free(dev_name);
	    value.format = vpiIntVal;
	    value.value.integer = -1;
	    vpi_put_value(sys, &value, 0, vpiNoDelay);
	    return 0;
      }

	/* Split the string into a host name and a port number. If
	   there is no host name, then use "localhost". */
      char*host_name = 0;
      char*host_port = 0;
      char*cp = strchr(host_string, ':');
      if (cp != 0) {
	    *cp++ = 0;
	    host_name = host_string;
	    host_port = cp;
      } else {
	    host_name = "localhost";
	    host_port = host_string;
      }

	/* Given the host string from the command line, look it up and
	   get the address and port numbers. */
      struct addrinfo hints, *res;
      memset(&hints, 0, sizeof(struct addrinfo));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      int rc = getaddrinfo(host_name, host_port, &hints, &res);
      if (rc != 0) {
	    vpi_printf("%s:%d: %s(%s) cannot find host %s:%s\n",
		       vpi_get_str(vpiFile, sys), (int)vpi_get(vpiLineNo, sys),
		       my_name, dev_name, host_name, host_port);

	    free(dev_name);
	    free(host_string);
	    value.format = vpiIntVal;
	    value.value.integer = -1;
	    vpi_put_value(sys, &value, 0, vpiNoDelay);
	    return 0;
      }

	/* Connect to the server. */
      int server_fd = -1;
      struct addrinfo *rp;
      for (rp = res ; rp != 0 ; rp = rp->ai_next) {
	    server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
	    if (server_fd < 0)
		  continue;

	    if (connect(server_fd, rp->ai_addr, rp->ai_addrlen) < 0) {
		  close(server_fd);
		  server_fd = -1;
		  continue;
	    }

	    break;
      }

      freeaddrinfo(res);

      if (server_fd == -1) {
	    vpi_printf("%s:%d: %s(%s) cannot connect to server %s:%s\n",
		       vpi_get_str(vpiFile, sys), (int)vpi_get(vpiLineNo, sys),
		       my_name, dev_name, host_name, host_port);

	    free(dev_name);
	    free(host_string);
	    value.format = vpiIntVal;
	    value.value.integer = -1;
	    vpi_put_value(sys, &value, 0, vpiNoDelay);
	    return 0;
      }

	/* Send HELLO message to the server. */
      snprintf(buf, sizeof buf, "HELLO %s\n", dev_name);

      rc = write(server_fd, buf, strlen(buf));
      assert(rc == strlen(buf));

	/* Read response from server. */
      rc = read(server_fd, buf, sizeof buf);
      assert(rc > 0);
      assert(strchr(buf, '\n'));

      if (strcmp(buf, "NAK\n") == 0) {
	    vpi_printf("%s:%d: %s(%s) Server %s:%s doesn't want me.\n",
		       vpi_get_str(vpiFile, sys), (int)vpi_get(vpiLineNo, sys),
		       my_name, host_name, host_port, dev_name);

	    free(dev_name);
	    free(host_string);
	    close(server_fd);
	    value.format = vpiIntVal;
	    value.value.integer = -1;
	    vpi_put_value(sys, &value, 0, vpiNoDelay);
	    return 0;
      }

      unsigned ident = 0;
      if (strncmp(buf, "YOU-ARE ", 8) == 0) {
	    sscanf(buf, "YOU-ARE %u", &ident);
      } else {
	    vpi_printf("%s:%d: %s(%s) Server %s:%s protocol error.\n",
		       vpi_get_str(vpiFile, sys), (int)vpi_get(vpiLineNo, sys),
		       my_name, dev_name, host_name, host_port);

	    free(dev_name);
	    free(host_string);
	    close(server_fd);
	    value.format = vpiIntVal;
	    value.value.integer = -1;
	    vpi_put_value(sys, &value, 0, vpiNoDelay);
	    return 0;
      }

	/* Create an instance for this connection. */
      idx = 0;
      while (instance_table[idx].name && idx < MAX_INSTANCES) {
	    idx += 1;
      }

      assert(idx < MAX_INSTANCES);
      instance_table[idx].name = dev_name;
      instance_table[idx].fd = server_fd;
	/* Empty the read buffer. */
      instance_table[idx].read_buf[0] = 0;
      instance_table[idx].read_fil = 0;

      vpi_printf("%s:%d: %s(%s) Bus server %s:%s ready.\n",
		 vpi_get_str(vpiFile, sys), (int)vpi_get(vpiLineNo, sys),
		 my_name, dev_name, host_name, host_port);

      value.format = vpiIntVal;
      value.value.integer = idx;
      vpi_put_value(sys, &value, 0, vpiNoDelay);
      return 0;
}

static PLI_INT32 simbus_ready_compiletf(char*my_name)
{
      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);

      vpi_printf("%s:%d: %s() STUB compiletf\n",
		 vpi_get_str(vpiFile, sys), (int)vpi_get(vpiLineNo, sys),
		 my_name);
      return 0;
}

static PLI_INT32 simbus_ready_calltf(char*my_name)
{
      s_vpi_value value;
      s_vpi_time now;

      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle scope = vpi_handle(vpiScope, sys);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);
      vpiHandle arg;
      assert(argv);

      arg = vpi_scan(argv);
      assert(arg);

	/* Get the BUS identifier to use. */
      value.format = vpiIntVal;
      vpi_get_value(arg, &value);
      int bus_id = value.value.integer;
      assert(bus_id < MAX_INSTANCES);
      assert(instance_table[bus_id].fd >= 0);

	/* Get the simulation time. */
      now.type = vpiSimTime;
      vpi_get_time(0, &now);
      uint64_t now_int = ((uint64_t)now.high) << 32;
      now_int += (uint64_t) now.low;

	/* Get the units for the scope */
      int units = vpi_get(vpiTimeUnit, scope);
	/* Get the units for the simulation */
      int prec  = vpi_get(vpiTimePrecision, 0);

	/* Figure the scale (power of 10) needed to convert the
	   simulation time to ns. */
      int scale = prec - units;

      char message[MAX_MESSAGE+1];
      snprintf(message, sizeof message, "READY %" PRIu64 "e%d", now_int, scale);

      char*cp = message + strlen(message);

	/* Send the current state of all the named signals. The format
	   passed in to the argument list is "name", value. Write
	   these values in the proper message format. */
      for (arg = vpi_scan(argv) ; arg ; arg = vpi_scan(argv)) {
	    *cp++ = ' ';

	    value.format = vpiStringVal;
	    vpi_get_value(arg, &value);
	    strcpy(cp, value.value.str);
	    cp += strlen(cp);

	    *cp++ = '=';

	    arg = vpi_scan(argv);
	    assert(arg);

	    value.format = vpiBinStrVal;
	    vpi_get_value(arg, &value);
	    strcpy(cp, value.value.str);
	    cp += strlen(cp);
      }

      *cp++ = '\n';
      *cp = 0;

      int rc = write(instance_table[bus_id].fd, message, strlen(message));
      assert(rc == strlen(message));

      return 0;
}

static PLI_INT32 simbus_until_compiletf(char*my_name)
{
      vpiHandle sys  = vpi_handle(vpiSysTfCall, 0);

      vpi_printf("%s:%d: %s() STUB compiletf\n",
		 vpi_get_str(vpiFile, sys), (int)vpi_get(vpiLineNo, sys),
		 my_name);
      return 0;
}

static PLI_INT32 simbus_until_calltf(char*my_name)
{
      s_vpi_value value;

      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);

      vpiHandle bus_h = vpi_scan(argv);
      assert(bus_h);

      value.format = vpiIntVal;
      vpi_get_value(bus_h, &value);

      int bus = value.value.integer;
      assert(bus >= 0 && bus < MAX_INSTANCES);

      char buf[MAX_MESSAGE+1];
      int rc = read_message(bus, buf, sizeof buf);
      assert(rc > 0);

      vpi_printf("%s:%d: %s() STUB calltf message:%s\n",
		 vpi_get_str(vpiFile, sys), (int)vpi_get(vpiLineNo, sys),
		 my_name, buf);

      uint64_t deltatime = 1; /* XXXX STUB */

	/* Set the return value and return. */
      value.format = vpiIntVal;
      value.value.integer = deltatime;
      vpi_put_value(sys, &value, 0, vpiNoDelay);

      return 0;
}


static struct t_vpi_systf_data simbus_connect_tf = {
      vpiSysFunc,
      vpiSysFuncInt,
      "$simbus_connect",
      simbus_connect_calltf,
      simbus_connect_compiletf,
      0 /* sizetf */,
      "$simbus_connect"
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
      vpiSysFunc,
      vpiSysFuncSized,
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
      simbus_mem_register,
      0
};

/*
 * $Log: $
 */

