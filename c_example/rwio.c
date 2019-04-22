/*
 * rwio.c
 * SeaMAX for Linux Example Code
 *
 * This C code demonstrates the reading and writing of SeaDAC Lite 8111
 * digital inputs and outputs.
 *
 * Sealevel and SeaMAX are registered trademarks of Sealevel Systems
 * Incorporated.
 *
 * © 2009-2017 Sealevel Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Inclusion of the SeaMAX library header is required
#include <seamaxlin.h>

int main(int argc, char * argv[])
{
	// Data for this SeaDAC module is eight bits or one byte.
	// the first four bits are read only and the high order four bits
	// are used for writing.
	unsigned char data = 0x00;

	// Create the SeaMAX object for accessing SeaDAC modules
	// NOTE the difference in the OOP class name (CSeaMaxLin) and the
	// nonOOP struct pointer (SeaMaxLin)
	SeaMaxLin *myModule = SeaMaxLinCreate();

	// Create the module string for use with SeaMaxLinOpen(...)
	// APPLICATION NOTE:
	//  "sealevel_d2x://Product Name"
	char *portString = "sealevel_d2x://8111";

	// Open the SeaDAC module by calling SeaMaxLinOpen() from the library
	int result = SeaMaxLinOpen(myModule, portString);
	if (result != 0)
	{
		printf("ERROR: Open failed, Returned %d\n", result);
		return -1;
	}

	// Read the state of the inputs (lower nibble)
	result = SeaDacLinRead(myModule, &data, 1);
	if (result < 0)
	{
		printf("ERROR: Error reading inputs, Returned %d\n", result);
		return -1;
	}
	else
	{
		printf("Read Inputs: %01X\n", (data & 0x0F));
	}

	// Write the value 0xA out to the outputs (upper nibble), effectively 
	// turning them on and off in an alternating pattern.
	data = 0xA0;
	result = SeaDacLinWrite(myModule, &data, 1);
	if (result < 0)
	{
		printf("ERROR: Error writing outputs, Returned %d\n", result);
		return -1;
	}
	else
	{
		printf("Write Outputs: %01X\n", ((data >> 4) & 0x0F));
	}

	sleep(1);
	
	// Lastly, read in the current state of the inputs and outputs.  
	// We should see the 0xA pattern that we have recently written out (upper nibble)
	// and the current state of the inputs (lower nibble)
	data = 0x00;
	result = SeaDacLinRead(myModule, &data, 1);
	if (result < 0)
	{
		printf("ERROR: Error writing outputs, Returned %d\n", result);
		return -1;
	}
	else
	{
		printf("Read Inputs: %01X\n", (data & 0x0F));
		printf("Read Outputs: %01X\n", ((data >> 4) & 0x0F));
	}

	// Close the connection and clean up the memory used by SeaMAX
	SeaMaxLinClose(myModule);
	SeaMaxLinDestroy(myModule);
	
	return 0;
}
