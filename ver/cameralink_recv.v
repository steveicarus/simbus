
/*
 * Copyright 2010 Stephen Williams <steve@icarus.com>
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
 * The cameralink_recv module is a wrapper around the point_slave_io
 * module that understands the CameraLink protocol as it is carried
 * over the SIMBUS point-to-point protocol. This is the receiver
 * (frame grabber) side of the protocol.
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
 * parameter sideband_pull = 6'b000000
 *
 *    This parameter is the value to pull unconnected input pins. The
 *    module will use a weak assignment to pull these bits, and will
 *    by default pull these to zero. If you set the value to 6'bz,
 *    the pull will be removed. Otherwise, you can set it to any mix
 *    of bits.
 *
 * +simbus-<name>-bus=<port>
 *
 *    This command line plus-arg is used by the SIMBUS vpi to locate
 *    the server for this bus. The <name> is the value of the name
 *    parameter, and is also the device name that must show up in the
 *    bus configuration file.
 */
module cameralink_recv
  (// The bus supplies the clock
   output wire CLOCK,
   output wire VCE, LVV, FVV,
   output wire [7:0] red,
   output wire [7:0] green,
   output wire [7:0] blue,
   input wire cam_enable,
   input wire cam_request,
   // sideband bits from the camera receiver to the camera. These
   // are not part of CameraLink, but may be handy for simulations.
   inout wire[5:0] sideband_to_camera
   /* */);

   parameter name = "capture";

   parameter sideband_pull = 0;

   generate
      if (sideband_pull !== 06'bzzzzzz) begin
	 assign (weak0, weak1) sideband_to_camera = sideband_pull;
      end
   endgenerate

   wire [26:0] data_o;
   wire [7:0]  data_i = {sideband_to_camera, cam_request, cam_enable};

   // The CameraLink is really just a variation of the point-to-point
   // bus protocol. Connect my details nets to the generic ports.
   point_slave_io #(.name(name), .WIDTH_O(27), .WIDTH_I(8)) port
     (.clock(CLOCK), .data_o(data_o), .data_i(data_i));

   // Break out the CameraLink signals from the p2p data_o.
   assign red   = data_o[7:0];
   assign green = data_o[15:8];
   assign blue  = data_o[23:16];
   assign VCE   = data_o[24];
   assign LVV   = data_o[25];
   assign FVV   = data_o[26];

endmodule // cameralink_recv
