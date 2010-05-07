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


# include  "camera_priv.h"
# include  <stddef.h>
# include  <stdio.h>
# include  <stdlib.h>
# include  <unistd.h>
# include  <string.h>
# include  <png.h>
# include  <assert.h>

int load_image_file(struct cameralink_master*cam, const char*path)
{
      FILE*fd = fopen(path, "rb");
      if (fd == 0)
	    return -1;

      png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
      assert(png);

      png_infop pinfo = png_create_info_struct(png);
      assert(pinfo);

      png_infop end_info = png_create_info_struct(png);
      assert(end_info);

      png_init_io(png, fd);

      png_read_png(png, pinfo,
		     PNG_TRANSFORM_STRIP_16
		    |PNG_TRANSFORM_STRIP_ALPHA
		    |PNG_TRANSFORM_PACKING,
		   0);

      png_bytep*row_pointers = png_get_rows(png, pinfo);

      cam->video_hei = png_get_image_height(png, pinfo);
      cam->video_wid = png_get_image_width(png, pinfo);

      cam->video_data = malloc(3 * cam->video_wid * cam->video_hei);
      assert(cam->video_data);

      uint8_t*pix = cam->video_data;
      unsigned ydx;

      switch (png_get_color_type(png, pinfo)) {
	  case PNG_COLOR_TYPE_GRAY:
	  case PNG_COLOR_TYPE_GRAY_ALPHA:
	    for (ydx = 0 ; ydx < cam->video_hei ; ydx += 1) {
		  unsigned xdx;
		  for (xdx = 0 ; xdx < cam->video_wid ; xdx += 1) {
			*pix++ = row_pointers[ydx][xdx];
			*pix++ = row_pointers[ydx][xdx];
			*pix++ = row_pointers[ydx][xdx];
		  }
	    }
	    break;
	  case PNG_COLOR_TYPE_RGB:
	  case PNG_COLOR_TYPE_RGB_ALPHA:
	    for (ydx = 0 ; ydx < cam->video_hei ; ydx += 1) {
		  unsigned xdx;
		  for (xdx = 0 ; xdx < cam->video_wid ; xdx += 1) {
			*pix++ = row_pointers[ydx][3*xdx+0];
			*pix++ = row_pointers[ydx][3*xdx+1];
			*pix++ = row_pointers[ydx][3*xdx+2];
		  }
	    }
	    break;
	  default:
	    assert(0);
	    break;
      }

      png_destroy_read_struct(&png, &pinfo, &end_info);

      return 0;
}
