/*
 * seadaclite.c
 * SeaMAX for Linux
 *
 * This code implements SeaDAC Lite specific functionality.  These hardware
 * devices do not implement the same abrstracted IO protocol (Modbus) as the
 * rest of the family and require special chipset access methods.
 *
 * Sealevel and SeaMAX are registered trademarks of Sealevel Systems
 * Incorporated.
 *
 * � 2009-2017 Sealevel Systems, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the Lesser GNU General Public License
 * as published by the Free Software Foundation; either version
 * 3 of the License, or (at your option) any later version.
 * LGPL v3
 */

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "seamaxlin.h"
#include "ftdi.h"

// Sealevel vendor ID number
#define VENDOR	0x0C52

#define MAXIMUM_COMMANDS	255
#define MAXIMUM_COMMAND_BYTES	4096

// ----------------------------------------------------------------------------
// SeaDAC Lite range configuration type.
// This is the range of available SeaDAC products
// ----------------------------------------------------------------------------
typedef enum
{
	SDL_8111 = 0x8111,	///<  4 inputs and 4 reed outputs
	SDL_8112,		///<  4 inputs and 4 form-c outputs
	SDL_8113,		///<  4 inputs
	SDL_8114,		///<  4 reed outputs
	SDL_8115,		///<  4 form-c outputs
	SDL_8126 = 0x8126	///<  32 TTL I/O
} sdl_range_type;

// ----------------------------------------------------------------------------
// SeaDAC Lite i2c data enumeration type.
// The data mask types used in I2C SeaDAC Lite communciations.
// ----------------------------------------------------------------------------
typedef enum
{
	SCL = 0x01, SDA = 0x02, TDO = 0x04, CS = 0x08,
	GPIO_0 = 0x01, GPIO_1 = 0x02, GPIO_2 = 0x04, GPIO_3 = 0x08,
	GPIO_4 = 0x10, GPIO_5 = 0x20, GPIO_6 = 0x40, GPIO_7 = 0x80
} sdl_i2c_type;

static unsigned char	i2cMPSSEValue;
static unsigned char	i2cMPSSEDirection;

static unsigned char MPSSECommand[MAXIMUM_COMMAND_BYTES];
static int byteIndex;
static int bytesToRead;
static int responseCount;
static int responseOffsets[MAXIMUM_COMMANDS];
static void* variableCallbacks[MAXIMUM_COMMANDS];


//  --------------------------------------------------------------------------
// ( Private function check for supported hardware ID.                        )
//  --------------------------------------------------------------------------
int SDL_IsSupported(int pid)
{
	switch (pid)
	{
		default:	return 0;

		case SDL_8111:
		case SDL_8112:
		case SDL_8113:
		case SDL_8114:
		case SDL_8115:
		case SDL_8126:	return 1;
	}
}


//  --------------------------------------------------------------------------
// ( Private function set the chipset into bit bang mode for I2C emulation.   )
//  --------------------------------------------------------------------------
int I2C_InitializeI2C(seaMaxModule* in)
{
	unsigned char InitCommand[32];
	pf_ftdi_write_data ftdi_write_data = dlsym(in->libftdi, "ftdi_write_data");
	pf_ftdi_read_data ftdi_read_data = dlsym(in->libftdi, "ftdi_read_data");

	if (!ftdi_write_data || !ftdi_read_data)
	{
		return -EIO;
	}

	if (in != NULL)
	{
		// The chip is now in MPSSE Mode and uses MPSSE commands (see FTDI Application Notes)		
		//
		// Read in the state of the GPIO
		InitCommand[0] = 0x81;
		ftdi_write_data(in->ftdic, InitCommand, 1);
		ftdi_read_data(in->ftdic, (unsigned char*)&i2cMPSSEValue, 1);

		// Mask out anything but the GPIO, then ...
		//
		// Set the SCL and SDA as outputs (high), set TDO/DI and TMS/CS as inputs
		// Everything else is set as a low output
		i2cMPSSEValue &= 0xF0;
		i2cMPSSEValue |= (SCL | SDA);
		i2cMPSSEDirection = 0xF3;

		// Set the I/O Direction for the first 8 ADBUS lines (Command 0x80)
		// All are outputs (excluding TMS/CS - we don't use it anyway)
		InitCommand[0] = 0x80;
		InitCommand[1] = i2cMPSSEValue;
		InitCommand[2] = i2cMPSSEDirection;
		ftdi_write_data(in->ftdic, InitCommand, 3);

		// Set the clock divisor to product approximate a 45 KHz (Command 0x86)
		InitCommand[0] = 0x86;
		InitCommand[1] = 0x0D;
		InitCommand[2] = 0x00;
		ftdi_write_data(in->ftdic, InitCommand, 3);

		// Disable loopback (local echo)
		InitCommand[0] = 0x85;
		ftdi_write_data(in->ftdic, InitCommand, 1);

		return 0;
	}
	else return -1;

}


//  --------------------------------------------------------------------------
// ( Private function set the I2C start condition.                            )
//  --------------------------------------------------------------------------
void I2C_Start()
{
	// Set the data line as a high output
	i2cMPSSEDirection |= (SDA | SCL);
	i2cMPSSEValue |= SDA;
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Set the clock and data lines high
	i2cMPSSEValue |= (SCL | SDA);
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Set the clock high and data line low
	i2cMPSSEValue &= ~SDA;
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Set the clock and data lines low
	i2cMPSSEValue &= ~(SCL | SDA);
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;
}

//  --------------------------------------------------------------------------
// ( Private function set the I2C stop condition and prepare to listen.       )
//  --------------------------------------------------------------------------
void I2C_Stop()
{
	// Set the data line low
	i2cMPSSEDirection |= (SCL | SDA);
	i2cMPSSEValue &= ~SDA;
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Set the clock high and data line low
	i2cMPSSEValue |= SCL;
	i2cMPSSEValue &= ~SDA;
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Set the clock and data lines high
	i2cMPSSEValue |= (SCL | SDA);
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Set the clock and data lines as inputs
	i2cMPSSEDirection &= ~(SCL | SDA);
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;
}


//  --------------------------------------------------------------------------
// ( Private function write a 7-bit address to SDA line and check the ack.    )
// The parameter address is the left-justified (MSB) address of the I2C slave to 
// operate.  The last bit (LSB or right-most) is masked out and replaced with the
// read/write option.  To perform an I2C read of device with address 1010001, the
// function would be called with address = 0xA2, rw = 0x01.  Likewise, to perform
// a write to the same address, the rw parameter would be set to zero.
//  --------------------------------------------------------------------------
void I2C_WriteAddress(unsigned char address, int rw)
{
	// Write the address
	MPSSECommand[byteIndex++] = 0x13;
	MPSSECommand[byteIndex++] = 7;
	MPSSECommand[byteIndex++] = (address & 0xFE) | (rw & 0x01);

	// Read the acknowledgement
	// Configure the clock as an output and data line as an input
	i2cMPSSEDirection |= SCL;
	i2cMPSSEDirection &= ~SDA;
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Check the ACK bit
	MPSSECommand[byteIndex++] = 0x26;
	MPSSECommand[byteIndex++] = 0;

	// Configure the clock and data lines as outputs (both low) again
	i2cMPSSEDirection |= (SCL | SDA);
	i2cMPSSEValue &= ~(SCL | SDA);
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;
}


//  --------------------------------------------------------------------------
// ( Private function write a byte to SDA and check ack.                      )
//  --------------------------------------------------------------------------
void I2C_WriteByte(unsigned char byte)
{

	// Configure the lines as an output again
	i2cMPSSEDirection |= (SCL | SDA);
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Write out the byte
	MPSSECommand[byteIndex++] = 0x13;
	MPSSECommand[byteIndex++] = 7;
	MPSSECommand[byteIndex++] = byte;

	// Configure the clock as an output and data line as an input
	i2cMPSSEDirection |= SCL;
	i2cMPSSEDirection &= ~SDA;
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Check the ACK bit
	MPSSECommand[byteIndex++] = 0x26;
	MPSSECommand[byteIndex++] = 0;

	// Configure the clock and data lines as outputs (both low) again
	i2cMPSSEDirection |= (SCL | SDA);
	i2cMPSSEValue &= ~(SCL | SDA);
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;
}


//  --------------------------------------------------------------------------
// ( Private function read a byte from SDA and check ack.                     )
//  --------------------------------------------------------------------------
void I2C_ReadByte()
{
	// Configure the clock as an output and data line as an input
	i2cMPSSEDirection |= SCL;
	i2cMPSSEDirection &= ~SDA;
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Read the slave's data byte
	MPSSECommand[byteIndex++] = 0x26;
	MPSSECommand[byteIndex++] = 7;

	// Set the clock and data lines as outputs (both low) again
	// Configure the clock and data lines as outputs (both low) again
	i2cMPSSEDirection |= (SCL | SDA);
	i2cMPSSEValue &= ~(SCL | SDA);
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;

	// Write out our acknowledgement
	MPSSECommand[byteIndex++] = 0x13;
	MPSSECommand[byteIndex++] = 1;
	MPSSECommand[byteIndex++] = 0x80;
}


//  --------------------------------------------------------------------------
// ( Private function reads an I2C register.                                  )
//  --------------------------------------------------------------------------
void I2C_ReadRegister(unsigned char address, unsigned char reg, unsigned char* data)
{
	I2C_Start();
	I2C_WriteAddress(address, 0);
	I2C_WriteByte(reg);
	I2C_Start();
	I2C_WriteAddress(address, 1);
	I2C_ReadByte();
	I2C_Stop();

	bytesToRead += 4;

	responseOffsets[responseCount] = bytesToRead - 1;
	variableCallbacks[responseCount++] = data;
}


//  --------------------------------------------------------------------------
// ( Private function write to an I2C register.                               )
//  --------------------------------------------------------------------------
void I2C_WriteRegister(unsigned char address, unsigned char reg, unsigned char data)
{
	I2C_Start();
	I2C_WriteAddress(address, 0);
	I2C_WriteByte(reg);
	I2C_WriteByte(data);
	I2C_Stop();

	bytesToRead += 3;

}


//  --------------------------------------------------------------------------
// ( Private function initializes I2C queue.                                  )
//  --------------------------------------------------------------------------
void I2C_InitializeQueue(seaMaxModule* in)
{
	pf_ftdi_usb_purge_buffers ftdi_usb_purge_buffers = dlsym(in->libftdi, "ftdi_usb_purge_buffers");

	byteIndex = 0;
	bytesToRead = 0;
	responseCount = 0;

	memset(responseOffsets, 0, sizeof(responseOffsets));
	memset(variableCallbacks, 0, sizeof(variableCallbacks));

	if (ftdi_usb_purge_buffers) ftdi_usb_purge_buffers(in->ftdic);
}


//  --------------------------------------------------------------------------
// ( Private function executes I2C queue.                                     )
//  --------------------------------------------------------------------------
void I2C_ExecuteQueue(seaMaxModule* in)
{
	pf_ftdi_write_data ftdi_write_data = dlsym(in->libftdi, "ftdi_write_data");
	pf_ftdi_read_data ftdi_read_data = dlsym(in->libftdi, "ftdi_read_data");

	if (ftdi_write_data) ftdi_write_data(in->ftdic, MPSSECommand, byteIndex);
	memset(MPSSECommand, 0, sizeof(MPSSECommand));
	if (ftdi_read_data) ftdi_read_data(in->ftdic, MPSSECommand, bytesToRead);

	for (int i = 0; i < responseCount; i++)
	{
		*((unsigned char*)variableCallbacks[i]) = MPSSECommand[responseOffsets[i]];
	}
}


//  --------------------------------------------------------------------------
// ( Private function sets or clears a GPIO state and direction.              )
// Direction can be a logical OR'ing of any of the GPIO values and their state.  For
// instance, I2C_SetGPIO( (GPIO_0 | GPIO_1) , GPIO_1 ) sets the GPIO_0 and GPIO_1 pins
// as outputs, with the GPIO_0 pin low and the GPIO_1 pin high.  All other GPIOs (2 & 3)
// are configured as inputs.
//  --------------------------------------------------------------------------
void I2C_SetGPIO(unsigned char direction, unsigned char state)
{
	// Clear the stored state of the GPIO
	i2cMPSSEValue &= 0x0F;
	i2cMPSSEDirection &= 0x0F;

	// Set the state and direction
	i2cMPSSEValue |= (state << 4);
	i2cMPSSEDirection |= (direction << 4);
	MPSSECommand[byteIndex++] = 0x80;
	MPSSECommand[byteIndex++] = i2cMPSSEValue;
	MPSSECommand[byteIndex++] = i2cMPSSEDirection;
	MPSSECommand[byteIndex++] = 0x82;
	MPSSECommand[byteIndex++] = (state >> 4);
	MPSSECommand[byteIndex++] = (direction >> 4);
}



//  --------------------------------------------------------------------------
// ( Private function to open a SeaDAC Lite                                   )
//  --------------------------------------------------------------------------
int openD2X(SeaMaxLin *SeaMaxPointer, char *devName)
{
	int ret, pid;
	char *ptr;
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	//Quick test to make sure nothing is already opened
	if (in->hDevice > 0) return -EBUSY;

	//Check user input device string
	pid = strtol(devName, &ptr, 16);
	if (!SDL_IsSupported(pid))
	{
		fprintf(stderr, "Device type is not supported\n");
		return -ERANGE;
	}
	else if (*ptr != '\0')
	{
		fprintf(stderr, "Bad device type argument\n");
		return -EINVAL;
	}

	//If necessary, open the dynamic library
	if (!in->libftdi)
	{
		in->libftdi = dlopen("libftdi.so", RTLD_LAZY);
	}
	// Check for failure to load library
	if (!in->libftdi)
	{
		fprintf(stderr, "failed to load libftdi.so (%s)\n", dlerror());
		return -EAGAIN;
	}

	// Allocate FTDI context stucture
	pf_ftdi_new ftdi_new = dlsym(in->libftdi, "ftdi_new");
	if (!ftdi_new || !(in->ftdic = ftdi_new()))
	{
		fprintf(stderr, "ftdi_new failed\n");
		return -EAGAIN;
	}

	//Initialize the ftdi context structure
	pf_ftdi_init ftdi_init = dlsym(in->libftdi, "ftdi_init");
	if (!ftdi_init || ftdi_init(in->ftdic) < 0)
	{
		fprintf(stderr, "ftdi_init failed\n");
		return -EAGAIN;
	}

	//Open ftdi device based on connection string passed in
	pf_ftdi_usb_open ftdi_usb_open = dlsym(in->libftdi, "ftdi_usb_open");
	if (!ftdi_usb_open)
	{
		fprintf(stderr, "ftdi_usb_open failed\n");
		return -EAGAIN;
	}

	pf_ftdi_get_error_string ftdi_get_error_string = dlsym(in->libftdi, "ftdi_get_error_string");
	ret = ftdi_usb_open(in->ftdic, VENDOR, pid);
	if (ret < 0 && ret != -5)
	{
		fprintf(stderr, "unable to open ftdi device: %d %d (%s)\n",
			pid, ret, 
			ftdi_get_error_string ? ftdi_get_error_string(in->ftdic) : "ERROR");
		return -EEXIST;
	}

	//Pre-map the set_bitmode and enable_bitbang functions
	pf_ftdi_set_bitmode ftdi_set_bitmode = dlsym(in->libftdi, "ftdi_set_bitmode");
	if (!ftdi_set_bitmode)
	{
		fprintf(stderr, "ftdi_set_bitmode failed\n");
		return -EAGAIN;
	}
	pf_ftdi_enable_bitbang ftdi_enable_bitbang = dlsym(in->libftdi, "ftdi_enable_bitbang");
	if (!ftdi_enable_bitbang)
	{
		fprintf(stderr, "ftdi_enable_bitbang failed\n");
		return -EAGAIN;
	}

	//Use to determine device type; bitbang or SPI
	in->deviceType = pid;

	unsigned char direction[4] = { 0, 0, 0, 0 };
	switch (in->deviceType)
	{
	case SDL_8126:
		ftdi_set_bitmode(in->ftdic, 0xf0, BITMODE_MPSSE);
		//Set ftdi chip in SPI mode
		I2C_InitializeI2C(in);
		SeaDacGetPIODirection(SeaMaxPointer, direction);
		SeaDacSetPIODirection(SeaMaxPointer, direction);
		break;
	case SDL_8111:
	case SDL_8112:
		//Set ftdi chip in bit bang mode
		ftdi_enable_bitbang(in->ftdic, 0xF0);
		break;
	case SDL_8113:
		//Set ftdi chip in bit bang mode
		ftdi_enable_bitbang(in->ftdic, 0x00);
		break;
	case SDL_8114:
	case SDL_8115:
		//Set ftdi chip in bit bang mode
		ftdi_enable_bitbang(in->ftdic, 0xFF);
		break;
	}

	//Short delay after setting to bit bang mode
	sleep(1);

	//if we got this far without any errors, it's ok to update local data
	in->commMode = FTDI_DIRECT;

	return 0;
}


//  --------------------------------------------------------------------------
// ( Private function to close a SeaDAC Lite                                   )
//  --------------------------------------------------------------------------
void closeD2X(SeaMaxLin *SeaMaxPointer)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;
	if (SeaMaxPointer == NULL) return; 

	pf_ftdi_disable_bitbang ftdi_disable_bitbang = dlsym(in->libftdi, "ftdi_disable_bitbang");
	pf_ftdi_usb_close ftdi_usb_close = dlsym(in->libftdi, "ftdi_usb_close");
	pf_ftdi_deinit ftdi_deinit = dlsym(in->libftdi, "ftdi_deinit");
	pf_ftdi_free ftdi_free = dlsym(in->libftdi, "ftdi_free");

	if (ftdi_disable_bitbang) ftdi_disable_bitbang(in->ftdic);
	if (ftdi_usb_close) ftdi_usb_close(in->ftdic);
	if (ftdi_deinit) ftdi_deinit(in->ftdic);
	if (ftdi_free) ftdi_free(in->ftdic);
}


// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief	Read the entire PIO space of a SeaDAC Lite module
///
/// \param[in] *SeaMaxPointer    Pointer to an open seaMaxModule.
/// \param[out]	data
///
/// \retval		0	Success.
/// \retval		-1	Invalid model number.
/// \retval		-2	Unknown connection type.
///
/// Reads the entire PIO space, inputs and outputs alike.
///
/// \warning	Sufficient data must be allocated for data before calling
///	this function.  For instance, the 8126 has 32 bits of PIO, therefore
/// data should be allocated with 4 bytes of data before calling this function.
///
/// \note	Only available for SeaDAC 8126.
// ----------------------------------------------------------------------------
int SeaDacGetPIO(SeaMaxLin *SeaMaxPointer, unsigned char* data)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	if (in->commMode == FTDI_DIRECT)
	{
		if (in->deviceType == SDL_8126)
		{
			int index = 0;
			unsigned char /*portdata,*/ direction[4], inputState[4], outputState[4];

			I2C_InitializeQueue(in);

			// Read the direction ports (command registers 6 & 7)
			I2C_ReadRegister(0xE8, 6, &direction[0]);
			I2C_ReadRegister(0xE8, 7, &direction[1]);
			I2C_ReadRegister(0xEA, 6, &direction[2]);
			I2C_ReadRegister(0xEA, 7, &direction[3]);

			// Read the input port states (command registers 0 & 1)
			I2C_ReadRegister(0xE8, 0, &inputState[0]);
			I2C_ReadRegister(0xE8, 1, &inputState[1]);
			I2C_ReadRegister(0xEA, 0, &inputState[2]);
			I2C_ReadRegister(0xEA, 1, &inputState[3]);

			// Read the output port states (command registers 2 & 3)
			I2C_ReadRegister(0xE8, 2, &outputState[0]);
			I2C_ReadRegister(0xE8, 3, &outputState[1]);
			I2C_ReadRegister(0xEA, 2, &outputState[2]);
			I2C_ReadRegister(0xEA, 3, &outputState[3]);

			I2C_ExecuteQueue(in);

			for (; index < 4; index++)
			{
				data[index] = inputState[index] & direction[index];
				data[index] |= (outputState[index] & ~direction[index]);
			}


			return 4;
		}
		else return -1;
	}
	else return -2;
}


// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief	Write the entire PIO space of a SeaDAC Lite module
///
/// \param[in] *SeaMaxPointer    Pointer to an open seaMaxModule.
/// \param[in]	data
///
/// \retval		0	Success.
/// \retval		-1	Invalid model number.
/// \retval		-2	Unknown connection type.
///
/// Writes the entire PIO space
///
/// \note	PIO space writes will not affect those pins marked as inputs.
/// \note	Only available for SeaDAC 8126.
// ----------------------------------------------------------------------------
int SeaDacSetPIO(SeaMaxLin *SeaMaxPointer, unsigned char* data)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	if (in->commMode == FTDI_DIRECT)
	{
		if (in->deviceType == SDL_8126)
		{
			int index = 0;

			I2C_InitializeQueue(in);

			// Write the output ports (command registers 2 & 3)
			I2C_WriteRegister(0xE8, 2, data[index++]);
			I2C_WriteRegister(0xE8, 3, data[index++]);
			I2C_WriteRegister(0xEA, 2, data[index++]);
			I2C_WriteRegister(0xEA, 3, data[index++]);

			I2C_ExecuteQueue(in);

			return 0;
		}
		else return -1;
	}
	else return -2;
}


// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief	Sets the PIO direction 
///
/// \param[in] *SeaMaxPointer    Pointer to an open seaMaxModule.
/// \param[in]	data
///
/// \retval		0	Success.
/// \retval		-1	Invalid model number.
/// \retval		-2	Unknown connection type.
///
/// \note	Only available for SeaDAC 8126.
// ----------------------------------------------------------------------------
int SeaDacSetPIODirection(SeaMaxLin *SeaMaxPointer, unsigned char* data)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	if (in->commMode == FTDI_DIRECT)
	{
		if (in->deviceType == SDL_8126)
		{
			unsigned char enable = 0x00, ON = 0xFF, OFF = 0x00;

			I2C_InitializeQueue(in);

			// Write entire banks as either outputs or inputs to command registers 6 & 7
			// on both Philips PCA9535 chips
			I2C_WriteRegister(0xE8, 6, (data[0] == 0) ? OFF : ON);
			I2C_WriteRegister(0xE8, 7, (data[1] == 0) ? OFF : ON);
			I2C_WriteRegister(0xEA, 6, (data[2] == 0) ? OFF : ON);
			I2C_WriteRegister(0xEA, 7, (data[3] == 0) ? OFF : ON);

			// Enable the line driver directions
			if (data[0] == 0) enable |= GPIO_0;
			if (data[1] == 0) enable |= GPIO_1;
			if (data[2] == 0) enable |= GPIO_2;
			if (data[3] == 0) enable |= GPIO_3;

			I2C_SetGPIO(0xFF, ~enable);

			I2C_ExecuteQueue(in);

			return 0;
		}
		else return -1;
	}
	else return -2;
}


// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief	Reads the PIO direction 
///
/// \param[in] *SeaMaxPointer    Pointer to an open seaMaxModule.
/// \param[in]	data
///
/// \retval		0	Success.
/// \retval		-1	Invalid model number.
/// \retval		-2	Unknown connection type.
///
/// \note	Only available for SeaDAC 8126.
// ----------------------------------------------------------------------------
int SeaDacGetPIODirection(SeaMaxLin *SeaMaxPointer, unsigned char* data)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	if (in->commMode == FTDI_DIRECT)
	{
		if (in->deviceType == SDL_8126)
		{
			I2C_InitializeQueue(in);

			// Reads bank direction as either outputs or inputs to command registers 6 & 7
			// on both Philips PCA9535 chips
			I2C_ReadRegister(0xE8, 6, &data[0]);
			I2C_ReadRegister(0xE8, 7, &data[1]);
			I2C_ReadRegister(0xEA, 6, &data[2]);
			I2C_ReadRegister(0xEA, 7, &data[3]);

			I2C_ExecuteQueue(in);

			return 0;
		}
		else return -1;
	}
	else return -2;
}


// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Read from a SeaDAC Lite module.
/// This is a multifunction read that can read from a module previously opened.
/// Any SeaIO devices attached to that module can then be read from in one of 
/// the following ways.
///
/// \param[in] *SeaMaxPointer    Pointer to an open seaMaxModule.
/// \param[out] *data            Pointer to data storage buffer.
/// \param[in] numBytes               Length of data to read.
///
/// \return int      Error code.
/// \retval >0       Number of bytes of data in the buffer.
/// \retval -ERANGE  Data size too large.
/// \retval -EIO  	 Read I/O error.
// ----------------------------------------------------------------------------
int SeaDacLinRead(SeaMaxLin *SeaMaxPointer, unsigned char *data, int numBytes)
{
	int ret = 0;
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;
	pf_ftdi_read_pins ftdi_read_pins = dlsym(in->libftdi, "ftdi_read_pins");
	pf_ftdi_get_error_string ftdi_get_error_string = dlsym(in->libftdi, "ftdi_get_error_string");

	//Ensure no more than two bytes are requested
	if (numBytes > 2)
		return -ERANGE;

	if (!ftdi_read_pins)
	{
		fprintf(stderr, "read failed, error (cannot find function)\n");
		return -EIO;
	}

	//Read data from device
	ret = ftdi_read_pins(in->ftdic, data);
	if (ret < 0)
	{
		fprintf(stderr, "read failed, error %d (%s)\n", ret,
			ftdi_get_error_string ? ftdi_get_error_string(&in->ftdic) : "ERROR");
		return -EIO;
	}

	return ret;
}


// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Write to a SeaDAC Lite module.
/// This is a multifunction write that can write to a module previously opened.
/// Any SeaIO devices attached to that module can then be written to in one of 
/// the following ways.
///
/// \param[in] *SeaMaxPointer    Pointer to an open seaMaxModule.
/// \param[out] *data            Pointer to data storage buffer.
/// \param[in] numBytes              Length of data to read.
///
/// \return int      Error code.
/// \retval >0       Number of bytes of data in the buffer.
/// \retval -ERANGE  Data size too large.
/// \retval -EIO  	 Read I/O error.
// ----------------------------------------------------------------------------
int SeaDacLinWrite(SeaMaxLin *SeaMaxPointer, unsigned char *data, int numBytes)
{
	int ret = 0;
	unsigned char buf[1];

	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;
	pf_ftdi_write_data ftdi_write_data = dlsym(in->libftdi, "ftdi_write_data");
	pf_ftdi_get_error_string ftdi_get_error_string = dlsym(in->libftdi, "ftdi_get_error_string");

	//Ensure we copy at most two bytes
	if (numBytes > 2)
		return -ERANGE;

	memcpy(buf, data, numBytes);

	if (!ftdi_write_data)
	{
		fprintf(stderr, "write failed, error (cannot find function)\n");
		return -EIO;
	}

	//Write data to device
	ret = ftdi_write_data(in->ftdic, buf, numBytes);
	if (ret < 0)
	{
		fprintf(stderr, "write failed, error %d (%s)\n", ret,
			ftdi_get_error_string ? ftdi_get_error_string(&in->ftdic) : "ERROR");
		return -EIO;
	}

	return ret;
}
