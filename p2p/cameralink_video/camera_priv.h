#ifndef __camera_priv_H
#define __camera_priv_H
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

# include  <stdint.h>

struct cameralink_master {
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
	/* Keep track of a video stream with these members. */
      uint8_t*video_data;
      unsigned video_wid;
      unsigned video_mar;
      unsigned video_hei;
      unsigned video_gap;

      unsigned video_ptr;
};

#endif
