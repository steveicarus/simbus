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
 * cameralink_camera <flags> <images>...
 *
 * This program acts like a camera and transmits images over the
 * CameraLink bus. The <images>... is a list of PNG images that are
 * used as source images. The camera repeats the list in order until
 * the simulation ends.
 *
 * Command line flags:
 *
 *    -g <gap>
 *       Specify the minimum gap between image frames. The default is
 *       1, and must be greater then 0.
 *
 *    -s <server>
 *       Server string for connecting with the simbus server.
 *
 *    -w <width>+<margin>
 *       Specify the line width an line gap (margin) for the
 *       camera. The <margin> is optional and defaults to 1. It must
 *       be greater then 0.
 */

# include  <simbus_p2p.h>
# include  "camera_priv.h"
# include  <stddef.h>
# include  <stdio.h>
# include  <stdlib.h>
# include  <unistd.h>
# include  <string.h>
# include  <assert.h>

void camera_step(simbus_p2p_t bus, struct cameralink_master*cam)
{
      uint32_t val;

	/* Assemble the output bits */
      val = 0;
      val |= cam->red   <<  0;
      val |= cam->green <<  8;
      val |= cam->blue  << 16;
      val |= cam->VCE? 0x01000000 : 0;
      val |= cam->LVV? 0x02000000 : 0;
      val |= cam->FVV? 0x04000000 : 0;

      simbus_p2p_out(bus, &val);

      simbus_p2p_clock_posedge(bus, 1);

      val = 0;
      simbus_p2p_in(bus, &val);

	/* split out the input bits. Cameras do not normally have
	   inputs (other then light) but this simulation has a few
	   synchronous control bits. */
      cam->cam_enable  = (val & 0x01) ? 1 : 0;
      cam->cam_request = (val & 0x02) ? 1 : 0;


	/* Advance the video pointer. If there is no video, then
	   restrict the video_ptr to marking time within a blanking
	   line. */
      cam->video_ptr += 1;
      if (cam->video_data == 0) {
	    unsigned frame_width  = cam->video_wid + cam->video_mar;
	    assert(frame_width);
	    cam->video_ptr = cam->video_ptr % frame_width;
      }
}

struct image_file_cell {
      struct image_file_cell*next;
      char*path;
};
static struct image_file_cell*image_list = 0;
static struct image_file_cell*image_next = 0;

static int acquire_next_image(struct cameralink_master*cam)
{
      if (image_next == 0) {
	    assert(image_list);
	    image_next = image_list->next;
      }

      load_image_file(cam, image_next->path);
      image_next = image_next->next;

      return 0;
}

static int clear_image(struct cameralink_master*cam)
{
      cam->video_hei = 0;
      free(cam->video_data);
      cam->video_data = 0;
      return 0;
}

int main(int argc, char*argv[])
{
      const char*server = 0;
      unsigned use_wid = 0;
      unsigned use_mar = 1;
      unsigned use_gap = 1;
      int use_clock_when_disable = SIMBUS_P2P_CLOCK_RUN;
      int cur_clock = 0;

      int errors = 0;
      int arg;
      char*cp;
      while ( (arg = getopt(argc, argv, "c:g:s:w:")) != -1 ) {

	    switch (arg) {

		case 'c': /* -c run|stop0|stop1|stopz */
		  if (strcmp(optarg, "run") == 0)
			use_clock_when_disable = SIMBUS_P2P_CLOCK_RUN;
		  else if (strcmp(optarg,"stop0") == 0)
			use_clock_when_disable = SIMBUS_P2P_CLOCK_STOP0;
		  else if (strcmp(optarg,"stop1") == 0)
			use_clock_when_disable = SIMBUS_P2P_CLOCK_STOP1;
		  else if (strcmp(optarg,"stopZ") == 0)
			use_clock_when_disable = SIMBUS_P2P_CLOCK_STOPZ;
		  else
			use_clock_when_disable = SIMBUS_P2P_CLOCK_RUN;
		  break;

		case 'g': /* -g <gap> */
		  use_gap = strtoul(optarg, &cp, 10);
		  if (*cp != 0) {
			fprintf(stderr, "Invalid gap value: %s\n", optarg);
			errors += 1;
		  } else if (use_gap == 0) {
			fprintf(stderr, "Gap must be greater then zero.\n");
			use_gap = 0;
		  }
		  break;

		case 's': /* -s <server> */
		  server = optarg;
		  break;

		case 'w': /* -w <width>+<margin> */
		  use_wid = strtoul(optarg, &cp, 10);
		  if (use_wid == 0) {
			fprintf(stderr, "Invalid width geometry: %s\n", optarg);
			errors += 1;
			break;
		  }
		  if (*cp == '+') {
			use_mar = strtoul(cp+1, 0, 10);
			if (use_mar < 2) {
			      fprintf(stderr, "Invalid margin value: %s\n", optarg);
			      errors += 1;
			}
		  }
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

      if (use_wid == 0) {
	    fprintf(stderr, "No video width specified.\n");
	    return -1;
      }

      cur_clock = use_clock_when_disable;

	/* The remaining arguments are files to use as scan data. */
      for (arg = optind ; arg < argc ; arg += 1) {
	    struct image_file_cell*cell = calloc(1, sizeof (struct image_file_cell));
	    cell->path = strdup(argv[arg]);

	    if (image_list == 0) {
		  image_list = cell;
		  cell->next = cell;
	    } else {
		  cell->next = image_list->next;
		  image_list->next = cell;
		  image_list = cell;
	    }
      }

      if (errors) return -1;

	/* Connect to the SIMBUS server. */
      simbus_p2p_t bus = simbus_p2p_connect(server, "camera", 8, 27);
      if (bus == 0) {
	    fprintf(stderr, "Unable to connect to server %s\n", server);
	    return -1;
      }

	/* Create and initialize the camera information. */
      struct cameralink_master*camera = calloc(1, sizeof(struct cameralink_master));

      camera->video_wid = use_wid;
      camera->video_mar = use_mar;

	/* Make the clock visible to the capture board. */
      simbus_p2p_clock_mode(bus, cur_clock);

	/* Advance the clock an initial amount to get things going. */
      camera_step(bus, camera);

      for (;;) {
	      /* If we are receiving a video request from the client,
		 and we are not already in the midst of an image, then
		 acquire the next image to be transmitted. */
	    if (camera->video_data == 0 && camera->cam_request != 0 && camera->video_ptr==0)
		  acquire_next_image(camera);

	      /* Calculate the address of the pixel being
		 transmitted. The video_ptr member is the pixel
		 counter to use. */
	    unsigned frame_width  = camera->video_wid + camera->video_mar;
	    unsigned frame_height = camera->video_hei + camera->video_gap;
	    unsigned vpos = camera->video_ptr / frame_width;
	    unsigned hpos = camera->video_ptr % frame_width;

	      /* Generate the LVV based on the position in the
		 line. If the image is within the body, then make the
		 LVV active, if the image is beyond the body (in the
		 margin) then make the LVV inactive. */
	    camera->LVV = (hpos < camera->video_wid)? 1 : 0;
	    camera->VCE = 1;

	    if (camera->video_data == 0) {
		  camera->FVV = 0;

	    } else if (vpos >= frame_height) {
		  camera->FVV = 0;
		  clear_image(camera);

	    } else if (vpos >= camera->video_hei) {
		  camera->FVV = 0;

	    } else  {
		  uint8_t*pix = camera->video_data + 3*camera->video_wid*vpos + 3*hpos;
		  camera->FVV = 1;
		  camera->red   = pix[0];
		  camera->green = pix[1];
		  camera->blue  = pix[2];
	    }

	    int try_mode = camera->cam_enable? SIMBUS_P2P_CLOCK_RUN : use_clock_when_disable;
	    if (try_mode != cur_clock) {
		  cur_clock = try_mode;
		  simbus_p2p_clock_mode(bus, cur_clock);
	    }

	    camera_step(bus, camera);
      }

      simbus_p2p_disconnect(bus);

      return 0;
}
