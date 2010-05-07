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

/*
 * Command line flags:
 *
 *    -a <area>
 *       Video area to use as a capture buffer. Incoming images will
 *       be truncated to fit in this area.
 *
 *    -c <count>
 *       Capture this many images before terminating the simulation.
 *
 *    -s <server>
 *       Server string for connecting with the simbus server.
 */

# include  <simbus_p2p.h>
# include  <stdint.h>
# include  <stddef.h>
# include  <stdio.h>
# include  <stdlib.h>
# include  <unistd.h>
# include  <string.h>
# include  <assert.h>

struct cameralink_capture {
	/* Pixel Value */
      uint8_t red;
      uint8_t green;
      uint8_t blue;
	/* Video Control lines */
      int VCE;
      int LVV;
      int FVV;
	/* Camera command lines */
      int cam_enable;
      int cam_request;
	/* Video state */
      uint8_t*video_data;
      size_t  video_area;
      unsigned video_ptr;
      unsigned video_wid;
};

void capture_step(simbus_p2p_t bus, struct cameralink_capture*cap)
{
      uint32_t val;
	/* Assemble the output bits */
      val =  0;
      val |= cap->cam_enable ? 0x01 : 0;
      val |= cap->cam_request? 0x02 : 0;

      simbus_p2p_in_poke(bus, &val);

      simbus_p2p_clock_posedge(bus, 1);

      val = 0;
      simbus_p2p_out_peek(bus, &val);

      cap->red   = (val >>  0) & 0xff;
      cap->green = (val >>  8) & 0xff;
      cap->blue  = (val >> 16) & 0xff;
      cap->VCE   = (val >> 24) & 0x01;
      cap->LVV   = (val >> 25) & 0x01;
      cap->FVV   = (val >> 26) & 0x01;
}

int main(int argc, char*argv[])
{
      const char*server = 0;

      int capture_count = 1000000;
      size_t use_video_area = 1024*1024;
      int errors = 0;
      int arg;
      char*cp;
      while ( (arg = getopt(argc, argv, "a:c:s:")) != -1 ) {

	    switch (arg) {

		case 'a':
		  use_video_area = strtoul(optarg, &cp, 0);
		  break;

		case 'c': /* -c <count> */
		  capture_count = strtoul(optarg, &cp, 0);
		  break;

		case 's': /* -s <server> */
		  server = optarg;
		  break;

		default:
		  assert(0);
		  break;
	    }
      }

      if (server == 0) {
	    fprintf(stderr, "No server specified.\n");
	    return -1;
      }

      if (errors) return -1;

	/* Connect to the SIMBUS server. */
      simbus_p2p_t bus = simbus_p2p_connect(server, "capture", 8, 27);
      if (bus == 0) {
	    fprintf(stderr, "Unable to connect to server %s\n", server);
	    return -1;
      }

      struct cameralink_capture*cap = calloc(1, sizeof(struct cameralink_capture));
      assert(cap);

      cap->video_area = use_video_area;
      cap->video_data = malloc(3 * use_video_area);
      assert(cap->video_data);

      cap->cam_enable = 1;
      int capture_seq = 0;
      while (cap->FVV || capture_seq < capture_count) {
	    capture_step(bus, cap);

	      /* Request a capture whenever one is not busy. */
	    if (cap->FVV == 0) {
		  if (cap->cam_request == 0) {
			cap->cam_request = 1;
		  }
	    } else {
		  cap->cam_request = 0;
	    }

	    if (cap->FVV && cap->LVV && cap->VCE) {
		  uint8_t*pix = cap->video_data + 3*cap->video_ptr;
		  *pix++ = cap->red;
		  *pix++ = cap->green;
		  *pix++ = cap->blue;
		  cap->video_ptr += 1;
	    } else if (cap->FVV && cap->LVV==0) {
		    /* Get a measurement of the width from the first line. */
		  if (cap->video_wid == 0)
			cap->video_wid = cap->video_ptr;

	    } else if (cap->FVV==0 && cap->video_ptr > 0) {

		  if (cap->video_wid == 0)
			cap->video_wid = cap->video_ptr;

		  unsigned got_hei = cap->video_ptr / cap->video_wid;

		  char path[64];
		  snprintf(path, sizeof path, "img%05d.ppm", capture_seq);
		  FILE*fd = fopen(path, "wb");
		  assert(fd);
		  fprintf(fd, "P6 %u %u 255\n", cap->video_wid, got_hei);
		  fwrite(cap->video_data, 1, 3*cap->video_wid*got_hei, fd);
		  fclose(fd);

		  cap->video_ptr = 0;
		  cap->video_wid = 0;
		  capture_seq += 1;
	    }
      }

      simbus_p2p_end_simulation(bus);
      return 0;
}
