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
# include  <stdlib.h>
# include  <string.h>
# include  <sys/types.h>
# include  <sys/stat.h>
# include  <sys/mman.h>
# include  <unistd.h>
# include  <fcntl.h>
# include  <errno.h>

# include  "priv.h"
# include  <assert.h>

# define CMD_FILL    0x0000
# define CMD_COMPARE 0x0001
# define CMD_LOAD    0x0002
# define CMD_SAVE    0x0003

# define CONTROL_COUNT (4096/4)
# define MEMORY_INSTANCES 4
static struct memory_map_t {
      int fd;
      char*path;
      unsigned char*base;
      size_t size;

      unsigned control_regs[CONTROL_COUNT];
} memory_map[MEMORY_INSTANCES] = {
      { -1, 0, 0 },
      { -1, 0, 0 },
      { -1, 0, 0 },
      { -1, 0, 0 }
};

static void cmd_fill(int id)
{
      unsigned base = memory_map[id].control_regs[1];
      unsigned size = memory_map[id].control_regs[2];
      unsigned fill = memory_map[id].control_regs[3];

      unsigned char fill0 = (fill >>  0) & 0xff;
      unsigned char fill1 = (fill >>  8) & 0xff;
      unsigned char fill2 = (fill >> 16) & 0xff;
      unsigned char fill3 = (fill >> 24) & 0xff;

      unsigned idx;
      unsigned char* ptr;

      if (base >= memory_map[id].size)
	    return;

      if (size == 0)
	    return;

      vpi_printf("MEM%d: fill 0x%x - 0x%x with 0x%x\n",
		 id, base, base+size*4-1, fill);

      if ((base + size*4) > memory_map[id].size)
	    size = (memory_map[id].size - base) / 4;

      ptr = memory_map[id].base + base;
      for (idx = 0 ;  idx < size ;  idx += 1) {
	    *ptr++ = fill0;
	    *ptr++ = fill1;
	    *ptr++ = fill2;
	    *ptr++ = fill3;
      }

}

static void cmd_compare(int id)
{
      unsigned base1 = memory_map[id].control_regs[1];
      unsigned size1 = memory_map[id].control_regs[2];
      unsigned base2 = memory_map[id].control_regs[3];
      unsigned idx, rc = 0;

      const unsigned char*ba, *bb;
      unsigned message_limit = 20;

      if (base1 >= memory_map[id].size)
	    return;

      if (base2 >= memory_map[id].size)
	    return;

      if (size1 == 0)
	    return;

      if ((base1 + size1) > memory_map[id].size)
	    size1 = memory_map[id].size - base1;

      memory_map[id].control_regs[0] = memcmp(memory_map[id].base+base1,
					      memory_map[id].base+base2,
					      size1);

      ba = memory_map[id].base+base1;
      bb = memory_map[id].base+base2;

      for (idx = 0 ;  idx < size1 ;  idx += 1) {

	    if (ba[idx] != bb[idx]) {
		  switch (message_limit) {
		      case 0:
			break;
		      case 1:
			vpi_printf("MEM%d: More differences...\n", id);
			message_limit = 0;
			break;
		      default:
			vpi_printf("MEM%d: %02x at 0x%x != %02x at 0x%x\n",
				   id, ba[idx], base1+idx, bb[idx], base2+idx);
			message_limit -= 1;
			break;
		  }

		  rc += 1;
	    }
      }

      memory_map[id].control_regs[0] = rc;

      vpi_printf("MEM%d: compare 0x%x-0x%x and 0x%x-0x%x returns %x\n", id,
		 base1, base1+size1-1, base2, base2+size1-1,
		 memory_map[id].control_regs[0]);

}

static void extract_path_from_load(char*path, int id)
{
      int idx;

      assert(id < MEMORY_INSTANCES);

      for (idx = 0 ;  idx < (CONTROL_COUNT-3) ;  idx += 1) {
	    char byte;
	    char*dp = path + 4*idx;
	    unsigned word = memory_map[id].control_regs[3+idx];

	    *dp++ = (byte = (word & 0xff));
	    if (byte == 0) break;

	    word >>= 8;
	    *dp++ = (byte = (word & 0xff));
	    if (byte == 0) break;

	    word >>= 8;
	    *dp++ = (byte = (word & 0xff));
	    if (byte == 0) break;

	    word >>= 8;
	    *dp++ = (byte = (word & 0xff));
	    if (byte == 0) break;
      }
}

static void cmd_load(int id)
{
      unsigned base1 = memory_map[id].control_regs[1];
      unsigned size1 = memory_map[id].control_regs[2];
      unsigned cnt;

      char path[4096];

      FILE*fd;

      extract_path_from_load(path, id);
      path[sizeof(path) - 1] = 0;

      fd = fopen(path, "rb");
      if (fd == NULL) {
	    vpi_printf("MEM%d: Error opening %s for read\n", id, path);
	    memory_map[id].control_regs[0] = -1;
	    return;
      }

      cnt = fread(memory_map[id].base + base1, 1, size1, fd);
      vpi_printf("MEM%d: Read %u bytes from %s to offset 0x%08x\n",
		 id, cnt, path, base1);
      memory_map[id].control_regs[0] = cnt;
}

static void cmd_save(int id)
{
      unsigned base1 = memory_map[id].control_regs[1];
      unsigned size1 = memory_map[id].control_regs[2];
      unsigned cnt;

      char path[sizeof(memory_map[id].control_regs)];

      FILE*fd;

      if (memory_map[id].fd < 0) {
	    vpi_printf("MEM%d: Error: Memory file is not opened in SAVE.\n", id);
	    memory_map[id].control_regs[0] = -1;
	    return;
      }

      extract_path_from_load(path, id);
      path[sizeof(path) - 1] = 0;

      fd = fopen(path, "wb");
      if (fd == NULL) {
	    vpi_printf("MEM%d: Error opening %s for write\n", id, path);
	    memory_map[id].control_regs[0] = -1;
	    return;
      }

      if (base1 + size1 > memory_map[id].size) {
	    vpi_printf("MEM%d: Error: SAVE region 0x%08x-0x%08x is past 0x%08zx\n",
		       id, base1, base1+size1-1, memory_map[id].size);
	    memory_map[id].control_regs[0] = -1;
	    return;
      }

      cnt = fwrite(memory_map[id].base + base1, 1, size1, fd);
      vpi_printf("MEM%d: Wrote %u bytes from offset 0x%08x to %s\n",
		 id, cnt, base1, path);
      memory_map[id].control_regs[0] = cnt;

      fclose(fd);
}

static void memdev_command(int id)
{
      if (id >= MEMORY_INSTANCES) {
	    vpi_printf("PCI: ERROR: Memory command to invalid id=%d\n", id);
	    assert(0);
      }

      assert(id < MEMORY_INSTANCES);
      switch (memory_map[id].control_regs[0]) {
	  case CMD_FILL:
	    cmd_fill(id);
	    break;

	  case CMD_COMPARE:
	    cmd_compare(id);
	    break;

	  case CMD_LOAD:
	    cmd_load(id);
	    break;

	  case CMD_SAVE:
	    cmd_save(id);
	    break;

	  default:
	    break;
      }
}

static PLI_INT32 memory_open_sizetf(char*x)
{
      return 32;
}

static int memory_open_compiletf(char*cd)
{
      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);
      vpiHandle path, size;

	/* Make sure the first argument exists. */
      path = vpi_scan(argv);
      assert(path);

	/* Make sure the second argument exists. */
      size = vpi_scan(argv);
      assert(size);

      return 0;
}

static int memory_open_calltf(char*cd)
{
      int id;
      s_vpi_value value;

      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);
      vpiHandle path, size;

	/* Make sure the first argument exists and is a reg. */
      path = vpi_scan(argv);
      assert(path);

	/* Make sure the second argument exists and is a memory. */
      size = vpi_scan(argv);
      assert(size);

      id = 0;
      while (memory_map[id].fd >= 0 && id < MEMORY_INSTANCES)
	    id += 1;

      assert(id < MEMORY_INSTANCES);

      value.format = vpiStringVal;
      vpi_get_value(path, &value);

      memory_map[id].path = strdup(value.value.str);

      value.format = vpiIntVal;
      vpi_get_value(size, &value);

      memory_map[id].fd = open(memory_map[id].path, O_RDWR|O_CREAT, 0666);
      if (memory_map[id].fd == -1) {
	    perror(memory_map[id].path);
      }

      memory_map[id].size = value.value.integer;
      ftruncate(memory_map[id].fd, value.value.integer);

      memory_map[id].base = mmap(0, value.value.integer,
				 PROT_READ|PROT_WRITE,
				 MAP_SHARED, memory_map[id].fd, 0);
      if (memory_map[id].base == MAP_FAILED) {
	    vpi_printf("pci: Failed to map %s: errno=%d\n",
		       memory_map[id].path, errno);
      }
      assert(memory_map[id].base != MAP_FAILED);

      value.format = vpiIntVal;
      value.value.integer = id;
      vpi_put_value(sys, &value, 0, vpiNoDelay);

      return 0;
}

static PLI_INT32 memory_peek_sizetf(char*x)
{
      return 32;
}

static int memory_peek_compiletf(char*cd)
{
      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);
      vpiHandle fd, addr, bar, junk;

      assert(argv);

      fd = vpi_scan(argv);
      assert(fd);

      addr = vpi_scan(argv);
      assert(addr);

      bar = vpi_scan(argv);
      assert(bar);

      junk = vpi_scan(argv);
      assert(junk == 0);

      return 0;
}

static int memory_peek_calltf(char*cd)
{
      int id;
      unsigned address, bar_select;
      s_vpi_value value;
      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);
      vpiHandle fd, addr, bar;

      assert(argv);

      fd = vpi_scan(argv);
      assert(fd);

      value.format = vpiIntVal;
      vpi_get_value(fd, &value);
      id = value.value.integer;

      addr = vpi_scan(argv);
      assert(addr);

      value.format = vpiIntVal;
      vpi_get_value(addr, &value);
      address = value.value.integer % memory_map[id].size;

      bar = vpi_scan(argv);
      assert(bar);

      value.format = vpiIntVal;
      vpi_get_value(bar, &value);
      bar_select = value.value.integer;

      switch (bar_select) {

	  case 0:
	    value.format = vpiIntVal;
	    value.value.integer =
		    (memory_map[id].base[address+0] <<  0)
		  | (memory_map[id].base[address+1] <<  8)
		  | (memory_map[id].base[address+2] << 16)
		  | (memory_map[id].base[address+3] << 24);
	    break;

	  case 1:
	    value.format = vpiIntVal;
	    value.value.integer = memory_map[id].control_regs[address/4];
	    break;

	  default:
	    value.format = vpiIntVal;
	    value.value.integer = 0xffffffff;
	    break;
      }

      vpi_put_value(sys, &value, 0, vpiNoDelay);

      return 0;
}

static int memory_poke_compiletf(char*cd)
{
      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);
      vpiHandle fd, addr, word, bar, junk;

      assert(argv);

      fd = vpi_scan(argv);
      assert(fd);

      addr = vpi_scan(argv);
      assert(addr);

      word = vpi_scan(argv);
      assert(word);

      bar = vpi_scan(argv);
      assert(bar);

      junk = vpi_scan(argv);
      assert(junk == 0);

      return 0;
}

static int memory_poke_calltf(char*cd)
{
      int id;
      unsigned address, bar_select;
      unsigned long word_val;
      s_vpi_value value;

      vpiHandle sys = vpi_handle(vpiSysTfCall, 0);
      vpiHandle argv = vpi_iterate(vpiArgument, sys);
      vpiHandle fd, addr, word, bar;

      assert(argv);

      fd = vpi_scan(argv);
      assert(fd);

      value.format = vpiIntVal;
      vpi_get_value(fd, &value);
      id = value.value.integer;

      addr = vpi_scan(argv);
      assert(addr);

      value.format = vpiIntVal;
      vpi_get_value(addr, &value);
      address = value.value.integer % memory_map[id].size;

      word = vpi_scan(argv);
      assert(word);

      value.format = vpiIntVal;
      vpi_get_value(word, &value);
      word_val = value.value.integer;

      bar = vpi_scan(argv);
      assert(bar);

      value.format = vpiIntVal;
      vpi_get_value(bar, &value);
      bar_select = value.value.integer;

      switch (bar_select) {
	  case 0:
	    memory_map[id].base[address+0] = word_val & 0xff;
	    word_val >>= 8;
	    memory_map[id].base[address+1] = word_val & 0xff;
	    word_val >>= 8;
	    memory_map[id].base[address+2] = word_val & 0xff;
	    word_val >>= 8;
	    memory_map[id].base[address+3] = word_val & 0xff;
	    break;

	  case 1:
	    memory_map[id].control_regs[address/4] = word_val;
	    if (address == 0)
		  memdev_command(id);
	    break;

	  default:
	    break;
      }

      return 0;
}

static struct t_vpi_systf_data simbus_mem_open_tf = {
      vpiSysFunc,
      vpiSysFuncInt,
      "$simbus_mem_open",
      memory_open_calltf,
      memory_open_compiletf,
      memory_open_sizetf,
      "$simbus_mem_open"
};

static struct t_vpi_systf_data simbus_mem_peek_tf = {
      vpiSysFunc,
      vpiSysFuncSized,
      "$simbus_mem_peek",
      memory_peek_calltf,
      memory_peek_compiletf,
      memory_peek_sizetf,
      "$simbus_mem_peel"
};

static struct t_vpi_systf_data simbus_mem_poke_tf = {
      vpiSysTask,
      0,
      "$simbus_mem_poke",
      memory_poke_calltf,
      memory_poke_compiletf,
      0 /* sizetf */,
      "$simbus_ready"
};

void simbus_mem_register(void)
{
      vpi_register_systf(&simbus_mem_open_tf);
      vpi_register_systf(&simbus_mem_peek_tf);
      vpi_register_systf(&simbus_mem_poke_tf);
}


/*
 * $Log: $
 */

