/*
 * seamaxlin.c
 * SeaMAX for Linux
 *
 * This code implements a simple Application Programming Interface to enable
 * access to Sealevel Systems IO products.
 *
 * Sealevel and SeaMAX are registered trademarks of Sealevel Systems
 * Incorporated.
 *
 * © 2008-2017 Sealevel Systems, Inc.
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "seamaxlin.h"

// privately used tcp transaction number... unused in RTU type communications.
static int tcp_transaction = 0;

//  --------------------------------------------------------------------------
// ( Private function to calculate and tack on the crc.                       )
//  --------------------------------------------------------------------------
void calc_crc(int n, unsigned char *data)
{
	int i, j;
	unsigned char carry_flag;
	unsigned short crc = 0xFFFF;

	for (i = 0; i < n; i++)
	{
		crc = crc ^ data[i];

		for (j = 0; j < 8; j++)
		{
			carry_flag = crc & 0x0001;
			crc = crc >> 1;

			if (carry_flag == 0x1) crc = crc ^ 0xA001;
		}
	}


	data[n++] = crc & 0xFF;
	data[n] = (crc >> 8) & 0xFF;

}

//  --------------------------------------------------------------------------
// ( Private function to open a SeaIO device using serial                     )
//  --------------------------------------------------------------------------
int openRTU(SeaMaxLin *SeaMaxPointer, char *devName)
{
	struct termios newtio;
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	//Quick test to make sure nothing is already opened
	if (in->hDevice > 0) return -EBUSY;

	//Open device for reading and writing, and not control tty.  (CTRL-C)
	in->hDevice = open(devName, O_RDWR | O_NOCTTY);
	if (in->hDevice < 0) return -EBADF;

	//Save the current serial line configuration.
	in->initalConfig = (struct termios*) malloc(sizeof(struct termios));
	if (in->initalConfig == NULL) return -ENOMEM;
	if (tcgetattr(in->hDevice, in->initalConfig) < 0) return -EPERM;
	bzero(&newtio, sizeof(newtio));

	//9600 Baud, 8 data bits, local use, and readable
	newtio.c_cflag = B9600 | CS8 | CLOCAL | CREAD;

	//IGNPAR  : ignore bytes with parity errors
	newtio.c_iflag = IGNPAR;

	//Raw output.
	newtio.c_oflag = 0;

	//ICANON  : enable canonical input
	newtio.c_lflag = 0;

	//1/10 second timeout VERY IMPORTANT
	newtio.c_cc[VTIME] = 1;
	newtio.c_cc[VMIN] = 0;

	//clean line and activate settings 
	tcflush(in->hDevice, TCIFLUSH);
	if (tcsetattr(in->hDevice, TCSANOW, &newtio) < 0) return -EXDEV;

	//if we got this far without any errors, it's ok to update local data
	in->commMode = MODBUS_RTU;

	return 0;
}

//  --------------------------------------------------------------------------
// ( Private function to open a SeaIO device using sockets                    )
//  --------------------------------------------------------------------------
int openTCP(SeaMaxLin *SeaMaxPointer, char *devName)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;
	char *passed;
	char port[6] = "502";
	struct hostent *host;
	struct sockaddr_in sockinfo;

	//Quick test to make sure nothing is already opened
	if (in->hDevice > 0) return -EBUSY;

	//Open socket.
	in->hDevice = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (in->hDevice < 0) return -EBUSY;

	//Get the port number if one was supplied, otherwise default to 502
	if ((passed = strpbrk(devName, ":")) != NULL)
	{
		passed[0] = 0;                //clear the ':'
		memcpy(port, &passed[1], 5);  //update the port variable
	}

	//Get host address from the name or address provided
	if ((host = gethostbyname(devName)) == NULL) return -EBADF;

	//Prepare and make connection.
	memset(&sockinfo, 0, sizeof(sockinfo));
	sockinfo.sin_family = AF_INET;
	sockinfo.sin_addr.s_addr = *(long*)host->h_addr_list[0];
	sockinfo.sin_port = htons(atoi(port));

	if (connect(in->hDevice, (struct sockaddr*) &sockinfo,
		sizeof(sockinfo)) < 0) return -1;

	//If we get here, it's ok to update local data.
	in->commMode = MODBUS_TCP;

	return 0;
}

//  --------------------------------------------------------------------------
// ( Private function to format a valid modbus request and send it.           )
//  --------------------------------------------------------------------------
int makeRequest(seaMaxModule* in, slave_address_t slaveId, unsigned char funct,
	address_loc_t start, address_range_t quan, unsigned char *data)
{
	int i = 0, dataSize = 0, length = 0;
	unsigned char buff[256];

	//Make sure the channel isn't in use, when it isn't, lock it
	while (in->mutex) usleep(1000 * in->throttle);
	in->mutex = 1;

	//Prepare the packet header.
	if (in->commMode == MODBUS_TCP)
	{
		int temp = tcp_transaction++;
		buff[0] = temp >> 8;      //upper byte of tcp transaction id
		buff[1] = temp & 0x00FF;  //lower byte of tcp transaction id
		buff[2] = 0;
		buff[3] = 0;
		i = 6;        //hold a place for the message length
		length += 6;  //adding header in
	}

	buff[i] = slaveId;
	buff[i + 1] = funct;
	length += 2;

	//We already have everything that is necessary for 0x41/45/66
	//Only functions <0x40
	if (funct > 0 && funct < 0x40)
	{
		buff[i + 2] = start >> 8;     //Hi byte
		buff[i + 3] = start & 0x00FF; //Lo byte
		length += 2;
		//0x06 doesn't take a quantity.
		if (funct != 0x06)
		{
			buff[i + 4] = quan >> 8;      //Hi byte
			buff[i + 5] = quan & 0x00FF;  //Lo byte
			length += 2;
		}
	}

	//0x0F needs a byte count...
	if (funct == 0x0F)
	{
		buff[i + 6] = quan / 8;
		if ((quan % 8) != 0) buff[i + 6]++;
		length++;
	}

	//0x10 needs a byte count...
	if (funct == 0x10)
	{
		buff[i + 6] = quan * 2;
		length++;
	}

	//Now the writes need the data tact'd on the ends.
	if (funct == 0x06 || funct == 0x44) dataSize = 2;
	if ((funct == 0x0F) || (funct == 0x10)) dataSize = buff[i + 6];
	if (funct == 0x42) dataSize = 12;
	if (funct == 0x46 || funct == 0x47) dataSize = 3;
	if (funct == 0x64) dataSize = 5;

	for (i = 0; i < dataSize; i++)
	{
		buff[length] = data[i];
		length++;
		if (length == 255)
		{
			in->mutex = 0;
			//fprintf(stderr, "-EINVAL\n");
			return -EINVAL;
		}
	}

	//send the command to the module.
	if (in->commMode == MODBUS_RTU)
	{
		//add on the crc
		calc_crc(length, buff);
		length += 2;

		//write the command to the opened serial port
		if (write(in->hDevice, buff, length) != length)
		{
			in->mutex = 0;  //unlock
			//fprintf(stderr, "-EBADF\n");
			return -EBADF;  //quit
		}
	}
	else
	{
		//insert my length
		buff[4] = (length - 6) >> 8;      //header Hi byte
		buff[5] = (length - 6) & 0x00FF;  //header Lo byte

		//write the packet to the opened socket
		if (send(in->hDevice, buff, length, 0) != length)
		{
			in->mutex = 0;  //unlock
			//fprintf(stderr, "-EBADF\n");
			return -EBADF;  //quit
		}
	}

	in->mutex = 0;
	return length;
}

//  --------------------------------------------------------------------------
// ( Private function to recieve a response and place it in a buffer          )
//  --------------------------------------------------------------------------
int getResponse(seaMaxModule* in, unsigned char funct, unsigned char *data,
	int expected)
{
	int i = 0, j = 0, length = 0;
	unsigned char buffer[256];

	// Parameter check
	if (expected > sizeof(buffer))
	{
		//fprintf(stderr, "-ENOMEM\n");
		return -ENOMEM;
	}

	//Read back a response into a local buffer.
	if (in->commMode == MODBUS_RTU)
	{
		while (length < expected + 4)
		{
			int incoming = read(in->hDevice,
				&buffer[length],
				expected + 4 - length);

			if (incoming <= 0)
			{
				in->mutex = 0;
				//fprintf(stderr, "-EFAULT1\n");
				return -EFAULT;
			}

			usleep(1000 * in->throttle);
			length += incoming;
			if (length >= 220)
			{
				in->mutex = 0;  //unlock
			//fprintf(stderr, "-ENOMEM\n");
				return -ENOMEM; //quit
			}
		}

		if (length < 1)
		{
			in->mutex = 0;
			//fprintf(stderr, "-ENODEV\n");
			return -ENODEV;
		}

		i = 1;  //jump over slaveid.
		length -= 1;

		//remove the crc here (only on some functs)
		length -= 2;
	}
	else
	{
		//read response, unlock channel, and check for error
		length = recv(in->hDevice, buffer, 255, 0);
		if (length < 1)
		{
			//fprintf(stderr, "-ENODEV\n");
			return -ENODEV;
		}

		i = 7;  //jump over the header.  And slaveid.
		length -= 7;
	}

	//See if an exception occured.
	if (buffer[i] != funct)
	{
		data[0] = buffer[i + 1];  //return the exception code to user
		in->mutex = 0;          //unlock channel
		//fprintf(stderr, "-EFAULT2 %d %d\n", funct, buffer[i]);
		return -EFAULT;
	}

	//Writes don't get data back, they provide it...
	if (funct == 0x06 || funct == 0x10 || funct == 0x0F || funct == 0x42 || funct == 0x64)
	{
		in->mutex = 0;  //unlock the channel
		return 0;       //return the oky doky signal
	}

	//This will be a little different for some funct codes, so pay attention
	if (funct < 0x05)  //all come back with a byte count
	{
		i += 2;
		length -= 2;
	}
	if (funct > 0x40) //just have a funct code
	{
		i += 1;
		length -= 1;
	}
	if (funct == 0x41) //has model and config info
	{
		i += 3;
		length -= 3;
	}

	//Copy data to user provided buffer.  (Note only valid for READING)
	for (j = 0; j < length; j++)
	{
		data[j] = buffer[i];
		i++;
	}

	in->mutex = 0;  //unlock the channel
	return length;  //Return the number of bytes in the buffer
}

//  --------------------------------------------------------------------------
// ( Private extended adda config ioctl.                                      )
//  --------------------------------------------------------------------------
int GetExtendedADDAConfig(SeaMaxLin *dev, slave_address_t slaveId,
	adda_ext_config *ext)
{
	unsigned char data[2];
	adda_config original, config;
	seaio_ioctl_s ioctl;
	unsigned int value, range;
	int channel = 0;

	//Get the current ADDA config.
	SeaMaxLinIoctl(dev, slaveId, IOCTL_GET_ADDA_CONFIG, &original);

	//Copy config to the one to be modified.
	memcpy(&config, &original, sizeof(adda_config));

	//Determine the number of D/A channels; based on model number
	SeaMaxLinIoctl(dev, slaveId, IOCTL_READ_COMM_PARAM, &ioctl);

	// Newer devices (520 and SeaDAC) use a different modbus command for model
	if (ioctl.u.params.model == 256)
	{
		SeaMaxLinIoctl(dev, slaveId, IOCTL_GET_EXT_CONFIG, &ioctl);
	}

	switch (ioctl.u.params.model)
	{
	case 470:
	case 8227:	channel = 2; break;
	default:	channel = 0; break;
	}

	//Iterate over each channel
	for (; channel > 0; channel--)
	{
		//Write out a D/A value of 1V (1/10 of 0xFFF)
		data[0] = 0x01;
		data[1] = 0x99;
		SeaMaxLinWrite(dev, slaveId, HOLDINGREG, channel, 1, data);

		//Set A/D channels to read 0-10V range
		config.device.channel_mode = SINGLE_ENDED;
		config.channels.ch_1 = ZERO_TO_TEN;
		config.channels.ch_2 = ZERO_TO_TEN;

		//Patch the current D/A channel back into A/D
		switch (channel)
		{
		case 1:	config.device.reference_offset = DA_CHANNEL_1;
			break;
		case 2: config.device.reference_offset = DA_CHANNEL_2;
			break;
		}
		SeaMaxLinIoctl(dev, slaveId, IOCTL_SET_ADDA_CONFIG, &config);

		//Read back the value from the A/D
		SeaMaxLinRead(dev, slaveId, INPUTREG, channel, 1, data);
		value = (data[0] << 8) + data[1];

		//If we read back the 1V we put in, the D/A is 0-10V
		if (value > 0x171 && value < 0x1C1)
		{
			ext->ad_multiplier_enabled = 0;
			range = ZERO_TO_TEN;
		}
		//If we read back 0.5V, the D/A must be at 0-5V
		else if (value > 0x0B8 && value < 0x0E0)
		{
			ext->ad_multiplier_enabled = 0;
			range = ZERO_TO_FIVE;
		}
		//If we read back 10V, the D/A must be set for 0-10V with 10X
		else if (value > 0xE66 && value <= 0xFFF)
		{
			ext->ad_multiplier_enabled = 1;
			range = ZERO_TO_TEN;
		}
		//If we read back 5V, then the D/A must be at 0-5V with 10X
		else if (value > 0x737 && value < 0x8C7)
		{
			ext->ad_multiplier_enabled = 1;
			range = ZERO_TO_FIVE;
		}
		//If the response is nowhere in these ranges, it's broken
		else
		{
			SeaMaxLinIoctl(dev, slaveId, IOCTL_SET_ADDA_CONFIG, &original);
			return -1;
		}

		//Return the found range to the user.
		switch (channel)
		{
		case 1: ext->da_channel_1_range = range; break;
		case 2: ext->da_channel_2_range = range; break;
		}
	}

	//Restore device settings.  We can't put the D/A back, so zero it out.
	data[0] = 0x00;
	data[1] = 0x00;
	SeaMaxLinWrite(dev, slaveId, HOLDINGREG, 1, 1, data);
	SeaMaxLinWrite(dev, slaveId, HOLDINGREG, 2, 1, data);
	return SeaMaxLinIoctl(dev, slaveId, IOCTL_SET_ADDA_CONFIG, &original);
}

// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Allocate a seaMaxModule object.
/// This function allocates space for a seaMaxModule object, which holds
/// information necessary for the module to be recognized internally, and 
/// returns a pointer to that object.  This object is then passed to the other 
/// functions to distinguish which seaMaxModule to preform the specified 
/// operation on.  This must be the first call in your app.
///
/// \return *SeaMaxLin   A pointer to a SeaIO module object.
/// \retval NULL         Failed to allocate space.
// ----------------------------------------------------------------------------
SeaMaxLin *SeaMaxLinCreate()
{
	//Allocate the memory we need.
	seaMaxModule *SeaMaxPointer;
	SeaMaxPointer = (seaMaxModule*)malloc(sizeof(seaMaxModule));
	if (SeaMaxPointer == NULL) return NULL;

	//Initialize the data members.
	SeaMaxPointer->throttle = 1;
	SeaMaxPointer->mutex = 0;
	SeaMaxPointer->deviceType = 0;
	SeaMaxPointer->commMode = NO_CONNECT;
	SeaMaxPointer->hDevice = -1;
	SeaMaxPointer->initalConfig = NULL;
	SeaMaxPointer->libftdi = NULL;
	SeaMaxPointer->ftdic = NULL;

	//Cast the pointer as the type expected and return it.
	return (SeaMaxLin*)SeaMaxPointer;
}

// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief De-allocate a seaMaxModule object.
/// This function releases the resources previously allocated for a SeaIO 
/// module.  This must be the last call in your program.
///
/// \param[out] *SeaMaxPointer must be a previously allocated seaMaxModule.
///
/// \return int  Error code.
/// \retval 0    Successfully de-allocated memory.
///
// ----------------------------------------------------------------------------
int SeaMaxLinDestroy(SeaMaxLin *SeaMaxPointer)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	//If there isn't an object to free... don't try.
	if (SeaMaxPointer == NULL) return 0;

	//First check to make sure the malloc'd tty struct is free
	if (in->initalConfig != NULL) free(in->initalConfig);

	//Free up the memory previously used.
	free(SeaMaxPointer);

	//Didn't fail... I guess.
	return 0;
}

// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Open a SeaIO module.
/// This function will attempt to open a specified SeaIO module.  The method of
/// specification is a string formatted like "sealevel_rtu://dev/[your_serial]"
/// or "sealevel_tcp://x.x.x.x:y" where x's represent ip address and y the port.
/// If you leave out the port, 502 will be used as the default.  You may also 
/// enter the device's DCHP name like: "sealevel_tcp://Samwise". For SeaDAC Lite
/// modules use sealevel_d2x://xxxx where xxxx=8112 or 8115 etc..
///
/// \param[out] *SeaMaxPointer Pointer to a seaMaxModule object.
/// \param[in] *filename           Filename to open.
///
/// \return int            Error code.
/// \retval 0              Successfully opened device.
/// \retval -ENAMETOOLONG  Too many characters in filename string.
/// \retval -EINVAL        Unallocated pointer.
/// \retval -EBADF         Invalid filename.
/// \retval -EBUSY         Communications medium busy.
/// \retval -EXDEV         Unable to initialize communications.
/// \retval -ENOMEM        Low memory.
/// \retval -EPERM         Unable to retrieve current communication settings.
// ----------------------------------------------------------------------------
int SeaMaxLinOpen(SeaMaxLin *SeaMaxPointer, char *filename)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;
	char rtu[15] = "sealevel_rtu://", tcp[15] = "sealevel_tcp://",
		d2x[15] = "sealevel_d2x://";

	//Possible goof ups.
	if (strlen(filename) > 256) return -ENAMETOOLONG;
	if (SeaMaxPointer == NULL) return -EINVAL;
	if (in->commMode != NO_CONNECT) SeaMaxLinClose(SeaMaxPointer);

	//Determine which method of connection to attempt (RTU)
	if (strncmp(filename, rtu, 15) == 0)
		return openRTU(SeaMaxPointer, &filename[14]);
	//TCP
	else if (strncmp(filename, tcp, 15) == 0)
		return openTCP(SeaMaxPointer, &filename[15]);
	//Direct ftdi
	else if (strncmp(filename, d2x, 15) == 0)
		return openD2X(SeaMaxPointer, &filename[15]);
	//Unsupported
	else
		return -EINVAL;
}

// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Close a SeaIO module.
/// This function will attempt to close a previously opened seaMaxModule
/// and clear any local data.  It will also close the connection interface
/// (socket or tty) and clean up any local data.  You must call this when you
/// are finished with the module.
///
/// \param[in] *SeaMaxPointer the pointer to the previously opened seaMaxModule.
///
/// \return int   Error code.
/// \retval 0     Successfully closed module.
// ----------------------------------------------------------------------------
int SeaMaxLinClose(SeaMaxLin *SeaMaxPointer)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;
	if (SeaMaxPointer == NULL) return 0;

	//Don't try to close anything, if there isn't anything open...
	if (in->hDevice > 0)
	{
		//Return serial line state to previous.
		if (in->commMode == MODBUS_RTU)
			tcsetattr(in->hDevice, TCSANOW, in->initalConfig);

		//Close SeaDAC Lite modules if connected
		if (in->commMode == FTDI_DIRECT)
		{
			closeD2X(SeaMaxPointer);
		}

		//Close the connection, TCP or RTU
		close(in->hDevice);

		//Time to clean up.
		in->throttle = 1;
		in->mutex = 0;
		in->commMode = NO_CONNECT;
		in->hDevice = -1;
		free(in->initalConfig);
		in->initalConfig = NULL;
	}

	return 0;
}

// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Read from a module.
/// This is a multifunction read that can read from a module previously opened.
/// Any SeaIO devices attached to that module can then be read from in one of 
/// the following ways.
///
/// \param[in] *SeaMaxPointer    Pointer to an open seaMaxModule.
/// \param[in] slaveId           Address of the device you wish to read.
/// \param[in] type              The read type to preform.
/// \param[in] starting_address  Where to start the read; MODBUS is base 1.
/// \param[in] range             How many consecutive addresses to read.
/// \param[out] *data            Pointer to data storage buffer.
///
/// \return int      Error code.
/// \retval >0       Number of bytes of data in the buffer.
/// \retval -EBADF   No module open or write error.
/// \retval -EINVAL  Null buffer or too much data in buffer.
/// \retval -ENOMEM  Low memory.
/// \retval -ENODEV  Didn't receive response.
/// \retval -EFAULT  MODBUS exception.  First byte of buffer contains exception.
// ----------------------------------------------------------------------------
int SeaMaxLinRead(SeaMaxLin *SeaMaxPointer, slave_address_t slaveId,
	seaio_type_t type, address_loc_t starting_address,
	address_range_t range, void *data)
{
	int error = 0;
	int expected = 0;
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	//Modbus function code mask.
	unsigned char funct[6] = { 0x01, 0x02, 0x03, 0x04, 0x45, 0x41 };

	//Possible goof ups.
	if (SeaMaxPointer == NULL)  return -EBADF;
	if (data == NULL) 	    return -EINVAL;
	if (in->commMode == NO_CONNECT)   return -EBADF;

	//Modbus wants the starting address based at 0, not 1
	starting_address--;

	//Format a valid request and send it to the module.
	error = makeRequest(in, slaveId, funct[type - 1], starting_address,
		range, NULL);
	if (error < 0) return error;

	switch (funct[type - 1])
	{
	case 0x01:
	case 0x02:
		expected = 1 + (range / 8);
		if (range % 8 != 0) expected++;
		break;
	case 0x03:
	case 0x04:
		expected = 1 + (range * 2);
		break;
	case 0x45:
		expected = 5;
		break;
	case 0x41:
		expected = 15;
		break;
	default:
		expected = 1;
		break;
	}

	//Wait for a response.
	return getResponse(in, funct[type - 1], (unsigned char*)data, expected);
}

// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Write to a module.
/// This is a multifunction write that can write from a module previously opened.
/// Any SeaIO devices attached to that module can then be written to in one of 
/// the following ways.
///
/// \param[in] *SeaMaxPointer    Pointer to an open seaMaxModule.
/// \param[in] slaveId           Address of the device you wish to write.
/// \param[in] type              The write type to preform.
/// \param[in] starting_address  Where to start the write; MODBUS is base 1.
/// \param[in] range             How many consecutive addresses to write.
/// \param[in] *data             Pointer to data buffer.
///
/// \return int      Error code.
/// \retval >0       Number of bytes of data in the buffer.
/// \retval -EBADF   No module open or write error.
/// \retval -EINVAL  Null buffer or too much data in buffer.
/// \retval -ENOMEM  Low memory.
/// \retval -ENODEV  Didn't receive response.
/// \retval -EFAULT  MODBUS exception.  First byte of buffer contains exception.
// ----------------------------------------------------------------------------
int SeaMaxLinWrite(SeaMaxLin *SeaMaxPointer, slave_address_t slaveId,
	seaio_type_t type, address_loc_t starting_address,
	address_range_t range, unsigned char *data)
{
	int error = 0, length = 0, expected = 0;
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	//Modbus function code mask.
	unsigned char funct[6] = { 0x0F, 0x00, 0x06, 0x00, 0x00, 0x42 };
	unsigned char fcode = funct[type - 1];

	//Possible goof ups.
	if (funct[type - 1] == 0x00)  return -EINVAL;
	if (SeaMaxPointer == NULL)  return -EBADF;
	if (data == NULL) 	    return -EINVAL;
	if (in->commMode == NO_CONNECT)   return -EBADF;

	//Modbus wants the starting address based at 0, not 1
	starting_address--;

	//if we have multiple register writes:
	if ((fcode == 0x06) && (range > 1))
	{
		fcode = 0x10;
	}

	//Format a valid request and send it to the module.
	error = makeRequest(in, slaveId, fcode, starting_address,
		range, data);
	if (error < 0) return error;

	//Most of the responses don't even contain the data you wrote, so
	//figure out how much we wrote based on what the user told us.
	switch (funct[type - 1])
	{
	case 0x06:
		length = 2;
		expected = 4;
		break;
	case 0x10:
		length = 2;
		expected = 4;
		break;
	case 0x0F:
		length = range / 8;
		if ((range % 8) != 0) length++;
		expected = 4;
		break;
	case 0x42:
		length = 12;
		expected = 12;
		break;
	default:
		length = 1;
		expected = 1;
		break;
	}

	error = getResponse(in, fcode, data, expected);
	if (error < 0) return error;

	return length;
}

// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Multi-function tool to configure and access SeaIO specific features.
/// Ioctl can be used to get and set device parameters and states.
///
/// \param[in] *SeaMaxPointer    Pointer to an open seaMaxModule.
/// \param[in] slaveId           Address of the device you wish to access.
/// \param[in] which             Which type of Ioctl you wish to call.
/// \param     *data             Buffer to store or retrieve data from.
///
/// \return int     Error code.
/// \retval >0       Number of bytes of data in the buffer.
/// \retval -EBADF   No module open or write error.
/// \retval -EINVAL  Null buffer or too much data in buffer.
/// \retval -ENOMEM  Low memory.
/// \retval -ENODEV  Didn't receive response.
/// \retval -EFAULT  MODBUS exception.  First byte of buffer contains exception.
// ----------------------------------------------------------------------------
int SeaMaxLinIoctl(SeaMaxLin *SeaMaxPointer, slave_address_t slaveId,
	IOCTL_t which, void *data)
{
	int error, expected = 0;
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;
	seaio_ioctl_s *ptrIoctl = (seaio_ioctl_s*)data;  //access data as ioctl
	adda_config *ptrAdda = (adda_config*)data;       //access data as adda
	adda_ext_config *ptrExt = (adda_ext_config*)data;//access data as ext

	//Modbus function code mask.
	unsigned char funct[8] = { 0x45, 0x46, 0x47, 0x43,
				  0x44, 0x65, 0x64, 0x66 };
	unsigned char buffer[32];

	//Possible goof ups
	if (SeaMaxPointer == NULL)  return -EBADF;
	if (in->commMode == NO_CONNECT)   return -EBADF;
	if (data == NULL) return -EINVAL;
	if (which == IOCTL_GET_ADDA_EXT_CONFIG)
		return GetExtendedADDAConfig(SeaMaxPointer, slaveId, ptrExt);

	//Format data to send in the request
	switch (funct[which - 1])
	{
	case 0x46:  //SET_ADDRESS: address-padd-magic cookie
		buffer[0] = ptrIoctl->u.address.new_address;
		buffer[1] = 0x00;
		buffer[2] = ptrIoctl->u.params.magic_cookie;
		break;
	case 0x47:  //SET_COMM: baud rate-parity-magic cookie
		buffer[0] = ptrIoctl->u.comms.new_baud_rate;
		buffer[1] = ptrIoctl->u.comms.new_parity;
		buffer[2] = ptrIoctl->u.params.magic_cookie;
		break;
	case 0x44:  //SET_PIO: config word 1-config word 2
		buffer[0] = ptrIoctl->u.pio.config_state.PIO96.channel2;
		buffer[1] = ptrIoctl->u.pio.config_state.PIO96.channel1;
		break;
	case 0x64:  //SET_ADDA: dev cfg-(4 bytes for 16 channels)
		buffer[0] = (ptrAdda->device.reference_offset << 4);
		buffer[0] |= (ptrAdda->device.channel_mode & 0x0F);
		buffer[1] = ((ptrAdda->channels.ch_1 & 0x03) << 6);
		buffer[1] |= ((ptrAdda->channels.ch_2 & 0x03) << 4);
		buffer[1] |= ((ptrAdda->channels.ch_3 & 0x03) << 2);
		buffer[1] |= ((ptrAdda->channels.ch_4 & 0x03) << 0);
		buffer[2] = ((ptrAdda->channels.ch_5 & 0x03) << 6);
		buffer[2] |= ((ptrAdda->channels.ch_6 & 0x03) << 4);
		buffer[2] |= ((ptrAdda->channels.ch_7 & 0x03) << 2);
		buffer[2] |= ((ptrAdda->channels.ch_8 & 0x03) << 0);
		buffer[3] = ((ptrAdda->channels.ch_9 & 0x03) << 6);
		buffer[3] |= ((ptrAdda->channels.ch_10 & 0x03) << 4);
		buffer[3] |= ((ptrAdda->channels.ch_11 & 0x03) << 2);
		buffer[3] |= ((ptrAdda->channels.ch_12 & 0x03) << 0);
		buffer[4] = ((ptrAdda->channels.ch_13 & 0x03) << 6);
		buffer[4] |= ((ptrAdda->channels.ch_14 & 0x03) << 4);
		buffer[4] |= ((ptrAdda->channels.ch_15 & 0x03) << 2);
		buffer[4] |= ((ptrAdda->channels.ch_16 & 0x03) << 0);
		break;
	default:
		break;
	}

	//makeRequest
	error = makeRequest(in, slaveId, funct[which - 1], 0, 0, buffer);
	if (error < 0) return error;

	//getResponse
	switch (funct[which - 1])
	{
	case 0x45:
	case 0x65:
		expected = 5;
		break;
	case 0x46:
	case 0x47:
	case 0x43:
		expected = 3;
		break;
	case 0x66:
		expected = 16;
		break;
	default:
	case 0x44:
	case 0x64:
		expected = 1;
		break;
	}

	error = getResponse(in, funct[which - 1], buffer, expected);
	if (error < 0) return error;

	//Update
	switch (funct[which - 1])
	{
	case 0x45:  //GET_PARAMS: model-bridge-baud-parity-cookie
		ptrIoctl->u.params.model = 256 + buffer[0];
		ptrIoctl->u.params.bridge_type = buffer[1];
		ptrIoctl->u.params.baud_rate = buffer[2];
		ptrIoctl->u.params.parity = buffer[3];
		ptrIoctl->u.params.magic_cookie = buffer[4];
		break;
	case 0x47:  //SET_COMM: baud-parity-cookie
		ptrIoctl->u.params.baud_rate = buffer[0];
		ptrIoctl->u.params.parity = buffer[1];
		break;
	case 0x43:  //GET_PIO: model-config1-config2
		ptrIoctl->u.pio.model = 256 + buffer[0];
		ptrIoctl->u.pio.config_state.PIO96.channel2 = buffer[1];
		ptrIoctl->u.pio.config_state.PIO96.channel1 = buffer[2];
		break;
	case 0x65:  //GET_ADDA: dev cfg-(4 bytes for 16 channel cfgs)
		ptrAdda->device.reference_offset = buffer[0] >> 4;
		ptrAdda->device.channel_mode = buffer[0] & 0x0F;
		ptrAdda->channels.ch_1 = (buffer[1] >> 6) & 0x03;
		ptrAdda->channels.ch_2 = (buffer[1] >> 4) & 0x03;
		ptrAdda->channels.ch_3 = (buffer[1] >> 2) & 0x03;
		ptrAdda->channels.ch_4 = (buffer[1] >> 0) & 0x03;
		ptrAdda->channels.ch_5 = (buffer[2] >> 6) & 0x03;
		ptrAdda->channels.ch_6 = (buffer[2] >> 4) & 0x03;
		ptrAdda->channels.ch_7 = (buffer[2] >> 2) & 0x03;
		ptrAdda->channels.ch_8 = (buffer[2] >> 0) & 0x03;
		ptrAdda->channels.ch_9 = (buffer[3] >> 6) & 0x03;
		ptrAdda->channels.ch_10 = (buffer[3] >> 4) & 0x03;
		ptrAdda->channels.ch_11 = (buffer[3] >> 2) & 0x03;
		ptrAdda->channels.ch_12 = (buffer[3] >> 0) & 0x03;
		ptrAdda->channels.ch_13 = (buffer[4] >> 6) & 0x03;
		ptrAdda->channels.ch_14 = (buffer[4] >> 4) & 0x03;
		ptrAdda->channels.ch_15 = (buffer[4] >> 2) & 0x03;
		ptrAdda->channels.ch_16 = (buffer[4] >> 0) & 0x03;
		break;
	case 0x66:  //GET_EXT_CONFIG
		ptrIoctl->u.config.model = (buffer[0] << 8) | buffer[1];
		break;
	default:
		break;
	}

	return 0;
}

// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Set the inter-message delay.
/// This is an effort to optimize the number of messages that can be received.
/// A default of 1ms is used.  The delay is expected in ms.
/// 
/// \param[in] *SeaMaxPointer pointer to a previously opened seaMaxModule
/// \param[in] delay integer representation of the number of ms to delay
///
/// \return int      Error code.
/// \retval 0        Successfully set delay.
/// \retval -EBADF   No open module.
/// \retval -ENODEV  No open connection.
/// \retval -EPERM   Delay must be > 1.
// ----------------------------------------------------------------------------
int SeaMaxLinSetIMDelay(SeaMaxLin *SeaMaxPointer, int delay)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	if (SeaMaxPointer == NULL) return -EBADF;
	if (in->hDevice < 0) return -ENODEV;
	if (delay < 1) return -EPERM;

	in->throttle = delay;
	return 0;
}

// ----------------------------------------------------------------------------
/// \ingroup group_seamax_fun
/// \brief Get the handle to the actual communication medium.
/// For RTU types it will be a normal file pointer, but TCP will be a socket
/// handle.  For this reason, it was decided to only return RTU file pointers.
/// An attempt to get the socket handle will return an ENODEV error.
///
/// \param[in] *SeaMaxPointer  Pointer to a previously opened seaMaxModule
///
/// \return int      Error code.
/// \retval >0       Pointer to communications termios struct.
/// \retval -EBADF   No open module.
/// \retval -ENODEV  This is not an RTU type connection.
// ----------------------------------------------------------------------------
HANDLE SeaMaxLinGetCommHandle(SeaMaxLin *SeaMaxPointer)
{
	seaMaxModule *in = (seaMaxModule*)SeaMaxPointer;

	if (SeaMaxPointer == NULL) return -EBADF;
	if (in->commMode != MODBUS_RTU) return -ENODEV;

	return in->hDevice;
}
