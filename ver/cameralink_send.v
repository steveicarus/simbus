
/*
 * Copyright 2011 Stephen Williams <steve@icarus.com>
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
 * The cameralink_send module is a wrapper around the point_master_io
 * module that understands the CameraLink protocol as it is carried
 * over the SIMBUS point-to-point protocol. This is the camera side
 * of the protocol.
 *
 * **
 *
 * parameter name = "string"
 * 
 *    This parameter is the logical name for this device. The default
 *    value ("capture") is suitable if there is only 1 instance of this
 *    module in a simulation. If there are multiple instances, each
 *    must be given a unique name which is used to find the port string
 *    on the command line.
 *
 * +simbus-<name>-bus=<port>
 *
 *    This command line plus-arg is used by the SIMBUS vpi to locate
 *    the server for this bus. The <name> is the value of the name
 *    parameter, and is also the device name that must show up in the
 *    bus configuration file.
 */
module cameralink_send
  (// The bus supplies the clock
   output wire CLOCK,
   // The simulation supplies the video data
   input wire VCE, LVV, FVV,
   input wire [7:0] red,
   input wire [7:0] green,
   input wire [7:0] blue,
   // The camera may generate these...
   output wire cam_enable,
   output wire cam_request,
   // sideband bits from the camera receiver to the camera. These
   // are not part of CameraLink, but may be handy for simulations.
   output wire [5:0] sideband_to_camera
   /* */);

   parameter name = "camera";

   wire [26:0] data_o;
   wire [7:0]  data_i;

   assign {sideband_to_camera, cam_request, cam_enable} = data_i;

   // The CameraLink is really just a variation of the point-to-point
   // bus protocol. Connect my details nets to the generic ports.
   point_master_io #(.name(name), .WIDTH_O(27), .WIDTH_I(8)) port
     (.clock(CLOCK), .data_o(data_o), .data_i(data_i));

   // Break out the CameraLink signals from the p2p data_o.
   assign data_o = {FVV, LVV, VCE, blue, green, red};

endmodule // cameralink_recv
