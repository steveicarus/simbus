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
	    if (rc <= 0) return rc;

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

	    vpiHandle sig = vpi_scan(argv);
	    assert(sig);

	    value.format = vpiVectorVal;
	    vpi_get_value(sig, &value);
	    int bit;
	    char*sig_string = cp;
	    for (bit = vpi_get(vpiSize, sig) ; bit > 0 ; bit -= 1) {
		  int word = (bit-1) / 32;
		  int mask = 1 << ((bit-1) % 32);
		  if (value.value.vector[word].aval & mask)
			if (value.value.vector[word].bval & mask)
			      *cp++ = 'x';
			else
			      *cp++ = '1';
		  else
			if (value.value.vector[word].bval & mask)
			      *cp++ = 'z';
			else
			      *cp++ = '0';
	    }

	      /* The second value after the signal name is the drive
		 reference. It is the value that the server is driving
		 (or 'bz if this is output-only). Look at the driver
		 value, and if it is non-z and equal to the value that
		 I see in the verilog, then assume that this is the
		 driver driving the value and subtract it. */
	    vpiHandle drv = vpi_scan(argv);
	    assert(drv);
	    assert(vpi_get(vpiSize,drv) == vpi_get(vpiSize,sig));

	    value.format = vpiVectorVal;
	    vpi_get_value(drv, &value);

	    char*drv_reference = malloc(vpi_get(vpiSize,drv));
	    for (bit = vpi_get(vpiSize,drv) ; bit > 0 ; bit -= 1) {
		  int word = (bit-1) / 32;
		  int mask = 1 << ((bit-1) % 32);
		  if (value.value.vector[word].aval & mask)
			if (value.value.vector[word].bval & mask)
			      drv_reference[bit] = 'x';
			else
			      drv_reference[bit] = '1';
		  else
			if (value.value.vector[word].bval & mask)
			      drv_reference[bit] = 'z';
			else
			      drv_reference[bit] = '0';

	    }

	      /* Get the strength values from the signal. */
	    value.format = vpiStrengthVal;
	    vpi_get_value(sig, &value);

	      /* Now given the sig_string that is the current result,
		 and the drive reference that is the value that is
		 being driven by the server, subtract out from the
		 sig_string the server driver, and any pullups from
		 the port. */
	    for (bit = vpi_get(vpiSize,drv); bit > 0; bit -= 1, sig_string+=1) {

		  if (drv_reference[bit] == *sig_string) {
			*sig_string = 'z';
			continue;
		  }

		  if (*sig_string == 'z')
			continue;
		  if (*sig_string == 'x')
			continue;

		    /* Do not pass pullup/pulldown values to the
		       server. If the strength of the net is less then
		       a strong drive, then clear it to z. */
		  struct t_vpi_strengthval*str = value.value.strength + bit - 1;
		  if (str->s0 < vpiStrongDrive && str->s1 < vpiStrongDrive)
			*sig_string = 'z';
	    }

	    free(drv_reference);
	    assert(sig_string == cp);
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

struct signal_list_cell {
      struct signal_list_cell*next;
      char*key;
      vpiHandle sig;
};

static struct signal_list_cell* find_key_in_list(struct signal_list_cell*ll, const char*key)
{
      while (ll && strcmp(ll->key,key)!=0)
	    ll = ll->next;

      return ll;
}

static void free_signal_list(struct signal_list_cell*ll)
{
      while (ll) {
	    struct signal_list_cell*tmp = ll->next;
	    free(ll->key);
	    free(ll);
	    ll = tmp;
      }
}

static void set_handle_to_value(vpiHandle sig, const char*val)
{
      size_t width = strlen(val);
      size_t vv_count = (width+31)/32;

      s_vpi_value value;

      value.value.vector = calloc(vv_count, sizeof(s_vpi_vecval));
      int idx;
      for (idx = 0 ; idx < width ; idx += 1) {
	    int word = idx / 32;
	    int bit = idx % 32;
	    char src = val[width-idx-1];
	    PLI_INT32 amask = 0;
	    PLI_INT32 bmask = 0;
	    switch (src) {
		case '0':
		  continue;
		case '1':
		  amask = 1;
		  bmask = 0;
		  break;
		case 'x':
		  amask = 1;
		  bmask = 1;
		  break;
		case 'z':
		  amask = 0;
		  bmask = 1;
		  break;
	    }

	    s_vpi_vecval*vp = value.value.vector+word;

	    vp->aval |= amask << bit;
	    vp->bval |= bmask << bit;
      }

      if (vpi_get(vpiSize, sig) != width) {
	    vpi_printf("ERROR: %s is %d bits, got %zu from server\n",
		       vpi_get_str(vpiName, sig), vpi_get(vpiSize, sig), width);
      }

      assert(vpi_get(vpiSize, sig) == width);
      assert(vpi_get(vpiType, sig) == vpiReg);

      value.format = vpiVectorVal;
      vpi_put_value(sig, &value, 0, vpiNoDelay);

      free(value.value.vector);
}

static PLI_INT32 simbus_until_calltf(char*my_name)
{
      s_vpi_time now;
      s_vpi_value value;

      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle scope = vpi_handle(vpiScope, sys);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);

      vpiHandle bus_h = vpi_scan(argv);
      assert(bus_h);

      value.format = vpiIntVal;
      vpi_get_value(bus_h, &value);

      int bus = value.value.integer;
      assert(bus >= 0 && bus < MAX_INSTANCES);

	/* Get a list of the signals and their mapping to a handle. We
	   will use list list to map names from the UNTIL command back
	   to the handle. */
      struct signal_list_cell*signal_list = 0;
      vpiHandle key, sig;
      for (key = vpi_scan(argv) ; key ; key = vpi_scan(argv)) {
	    sig = vpi_scan(argv);
	    assert(sig);

	    struct signal_list_cell*tmp = calloc(1, sizeof(struct signal_list_cell));
	    value.format = vpiStringVal;
	    vpi_get_value(key, &value);
	    assert(value.format == vpiStringVal);
	    assert(value.value.str);
	    tmp->key = strdup(value.value.str);
	    tmp->sig = sig;
	    tmp->next = signal_list;
	    signal_list = tmp;
      }

	/* Now read the command from the server. This will block until
	   the server data actually arrives. */
      char buf[MAX_MESSAGE+1];
      int rc = read_message(bus, buf, sizeof buf);

      if (rc <= 0) {
	    vpi_printf("%s:%d: %s() read from server failed\n",
		       vpi_get_str(vpiFile, sys), (int)vpi_get(vpiLineNo, sys),
		       my_name);

	    vpi_control(vpiStop);

	    free_signal_list(signal_list);

	      /* Set the return value and return. */
	    value.format = vpiIntVal;
	    value.value.integer = 0;
	    vpi_put_value(sys, &value, 0, vpiNoDelay);
	    return 0;
      }

	/* Chop the message into tokens. */
      int   msg_argc = 0;
      char* msg_argv[MAX_MESSAGE/2];

      char*cp = buf;
      while (*cp != 0) {
	    msg_argv[msg_argc++] = cp;
	    cp += strcspn(cp, " ");
	    if (*cp) {
		  *cp++ = 0;
		  cp += strspn(cp, " ");
	    }
      }
      msg_argv[msg_argc] = 0;

      assert(strcmp(msg_argv[0],"UNTIL") == 0);

      assert(msg_argc >= 2);

      uint64_t until_mant = strtoull(msg_argv[1],&cp,10);
      assert(cp && *cp=='e');
      cp += 1;
      int until_exp = strtol(cp,0,0);

	/* Get the units for the scope */
      int units = vpi_get(vpiTimeUnit, scope);

	/* Put the until time into units of the scope. */
      while (units < until_exp) {
	    until_mant *= 10;
	    until_exp -= 1;
      }
      while (units > until_exp) {
	    until_mant = (until_mant + 5)/10;
	    until_exp += 1;
      }

      	/* Get the simulation time and put it into scope units. */
      now.type = vpiSimTime;
      vpi_get_time(0, &now);
      uint64_t deltatime = ((uint64_t)now.high) << 32;
      deltatime += (uint64_t) now.low;

      int prec  = vpi_get(vpiTimePrecision, 0);
      while (prec < units) {
	    prec += 1;
	    deltatime = (until_mant + 5)/10;
      }

	/* Now we can calculate the delta time. */
      if (deltatime > until_mant)
	    deltatime = 0;
      else
	    deltatime = until_mant - deltatime;

	/* Set the return value and return it. */
      value.format = vpiIntVal;
      value.value.integer = deltatime;
      vpi_put_value(sys, &value, 0, vpiNoDelay);

	/* Process the signal values. */
      int idx;
      for (idx = 2 ; idx < msg_argc ; idx += 1) {

	    char*key = msg_argv[idx];
	    char*val = strchr(key, '=');
	    assert(val && *val=='=');
	    *val++ = 0;

	    struct signal_list_cell*cur = find_key_in_list(signal_list, key);
	    if (cur == 0) {
		  vpi_printf("%s:%d: %s() Unexpected signal %s from bus.\n",
			     vpi_get_str(vpiFile, sys),
			     (int)vpi_get(vpiLineNo, sys),
			     my_name, key);
		  continue;
	    }

	    set_handle_to_value(cur->sig, val);
      }

      free_signal_list(signal_list);
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

