PixelPacket server for Raspberry pi
===================================

A simple server to receive pixelpackets.
Pixelpackets are send from a Windows-client and can be a live feed of the desktop,
or anything else.

Uses the LED-matrix **library** by Henner Zeller <https://github.com/hzeller/rpi-rgb-led-matrix>

The LED-matrix **library** is (c) Henner Zeller <h.zeller@acm.org> with
GNU General Public License Version 2.0 <http://www.gnu.org/licenses/gpl-2.0.txt>

Instructions
------------
See the documentation of the library to wire your pi.

Make the project by typing:

     make

After this is done you can run the server by typing:

     ./pixelator-server
	 
By default it uses port 29960 to receive pixelpackets, you can specify another port by using -p.
If you have a matrix composed of 8 chained 32x16 led panels in a 4x2 arrangement you can use the -T parameter.

Pixelpackets are formed as such:
4-bytes with the length of the packet followed by the pixeldata (3-bytes each, 1 byte R, 1 byte G, 1 byte B),
the pixels are fed line by line starting from position [0,0].