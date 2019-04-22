/*
 * seamaxlin.h
 * SeaMAX for Linux
 *
 * This code defines a simple Application Programming Interface to enable
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

#ifndef PUBLIC_DOCUMENTATION
/*! \mainpage   Linux SeaMAX API Documentation
 *  \section   warning      Internal Disclaimer
 *
 *  <b>WARNING! This document is intended for internal use only.</b>
 */  
#else
/*! \mainpage Linux SeaMAX API Documentation
 *
 * \section intro_sec Introduction
 *
 * Sealevel digital and analog I/O modules supported by the SeaMAX software 
 * suite are designed to work with third party applications via the SeaMAX API.
 * To help simplify application development, the following documentation 
 * details the functions of the SeaMAX API.  To help you get started, example
 * C and C++ source code is provided.
 *
 * \section start Getting Started
 *
 * For help specific to USB enabled SeaIO and SeaDAC Modules, please see the 
 * <b>\ref seaio_usb_guide</b> for further details.
 *
 * There are three modules that are included in the SeaMAX API:
 *
 * \li	<b>\ref group_seamax_all</b><br>
 *		The foundation of SeaMAX with functions for configuring, interfacing,
 *		and modifying supported Sealevel digital I/O modules and devices.
 * \li	<b>\ref group_cethernet_all</b><br>
 *		The Ethernet module contains functions related to the discovery and 
 *		configuration of Sealevel I/O devices with an Ethernet interface.
 * \li	<b>\ref modbusbreakdown</b><br>
 *		The Modbus Specification contains detailed descriptions of all RTU 
 *        and TCP Modbus commands applicable to SeaIO and SeaDAC modules.
 *
 * The 'Modules' tab above lists all SeaMAX modules and their functions.  For 
 * additional information regarding legacy products and technical 
 * specifications, please refer to the 'Related Pages' tab above.
 *
 * \section questions Questions & Comments
 *
 * Send your questions and comments to Sealevel Systems.  For technical 
 * assistance, please include your model or part number and any device
 * settings that may apply.  Technical support is available Monday to
 * Friday from 8:00AM to 5:00PM (US Eastern Time Zone, UTC-6 hours) by
 * email (support@sealevel.com) or by phone at +1 (864) 843.4343.
 */



/// \defgroup	group_seamax_all  SeaMAX API
/// The SeaMAX API consists of the functions outline below, and provides an
/// interface for construction, transmission, and reception of Modbus RTU and 
/// TCP commands.  For more information, click a function name below for 
/// detailed information on function use, parameter types, and return codes.
///
/// \note Not every function below may apply to your specific Sealevel I/O 
/// device.  Refer to the \ref modbusbreakdown "modbus breakdown".

/// \defgroup	group_cethernet_all	 SeaMAX Ethernet Discovery/Configuration API
///
/// The SeaMAX Ethernet Discover API includes functions used for configuration 
/// and discovery of Ethernet enabled Sealevel I/O devices.  For specifics,
/// click any of the function names below to view function use, parameters, and
/// return code information.

/// \ingroup	group_seamax_all
/// \defgroup	group_seamax_oop  Object-Oriented SeaMAX Modbus Interface
/// This is a C++ wrapper for the standard SeaMAX Modbus interface library.
/// This library is designed to aid in the construction, transmission, and
/// reception of Modbus RTU and TCP commands.  This library is designed for use
/// with Sealevel SeaIO modules.
///

/// \ingroup	group_seamax_all
/// \defgroup	group_seamax_fun  Functional SeaMAX Modbus Interface
/// This is a simple C library to aid in the construction, transmission, and 
/// reception of Modbus RTU and TCP commands.  This library is designed for use
/// with Sealevel SeaIO modules.
/// 

/// \ingroup	group_cethernet_all
/// \defgroup	group_cethernet_oop	 Object Oriented CEthernet Interface
/// This is a C++ wrapper for the standard CEthernet library.  This library is
/// designed to aid in the discovery and configuration of Ethernet enabled 
/// SeaIO modules.
/// 

/// \ingroup	group_cethernet_all
/// \defgroup	group_cethernet_fun	 Functional CEthernet Interface
/// This is a simple C library to aid in the discovery and configuration
/// of Ethernet enabled SeaIO modules.
/// 
#endif

#ifndef SEAMAXLIN_H__
#define SEAMAXLIN_H__

// This is used for proper inclusion when used with C++
#ifdef __cplusplus
	extern "C" {
#endif


// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \typedef SeaMaxLin
/// Pointer to a SeaMAX object. The data structure this points to is private 
/// and should not be accessed directly, but through the function calls instead.
// ----------------------------------------------------------------------------
typedef long  		SeaMaxLin;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \typedef HANDLE 
/// A handle or pointer to another data structure.
/// This particular pointer is used to pass the serial communications struct.
// ----------------------------------------------------------------------------
typedef int   		HANDLE;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \typedef slave_address_t
/// The slave ID of a device.  This value can be from 1 to 247.
// ----------------------------------------------------------------------------
typedef unsigned char 	slave_address_t;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \typedef address_loc_t 
/// An address specific to your device.  Check read/write functions for 
/// specific use.
// ----------------------------------------------------------------------------
typedef unsigned short	address_loc_t;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \typedef address_range_t 
/// The range of address to be used.  Check read/write functions for specific
/// use.
// ----------------------------------------------------------------------------
typedef unsigned short 	address_range_t;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief The module connection type.  
/// Currently there is only support for RTU and TCP type connections.
// ----------------------------------------------------------------------------
typedef enum
{
	NO_CONNECT = 0,   ///< Connection not open.
	MODBUS_RTU = 1,   ///< An RTU type connection.  232, 485, USB.
	MODBUS_TCP = 2,   ///< An Ethernet connection.
	FTDI_DIRECT = 3   ///< A SeaDAC Lite direct USB connection type.
} seaio_mode_t;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief The current baud rate used by a RTU type module.
/// If the module is TCP type, then BR9600, the default, will be used.
/// Note these are not the same values used by the termios library.
// ----------------------------------------------------------------------------
typedef enum
{
	BRNONE   =  0,   ///< Default value, no connection, or unknown baud rate.
	BR1200   =  1,   ///< 1200 baud.
	BR2400   =  2,   ///< 2400 baud.
	BR4800   =  3,   ///< 4800 baud.
	BR9600   =  4,   ///< 9600 baud.
	BR14400  =  5,   ///< 14400 baud.
	BR19200  =  6,   ///< 19200 baud.
	BR28800  =  7,   ///< 28800 baud.
	BR38400  =  8,   ///< 38400 baud.
	BR57600  =  9,   ///< 57600 baud.
	BR115200 = 10    ///< 115200 baud.
} baud_rates_t;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief The currently used parity.
/// Parity is a quick and simple method of checking for errors.  It doesn't 
/// work all the time, but it is very simple to implement.
// ----------------------------------------------------------------------------
typedef enum
{
	P_NONE = 0,     ///< No parity (Default).
	P_ODD  = 1,     ///< Odd parity.
	P_EVEN = 2      ///< Even parity.
} parity_t;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief The type of read / write to preform.
/// Note that not all types can be written.  For example an attempt to write to
/// D_INPUTS or INPUTREG would cause an EINVAL error.  Also you can not directly
/// write to SETUPREGS, you must use an appropriate Ioctl.
// ----------------------------------------------------------------------------
typedef enum
{
	COILS		= 1,  ///< Coils are any relay type outputs.
	D_INPUTS 	= 2,  ///< Digital inputs.  Single bit inputs.
	HOLDINGREG 	= 3,  ///< Configuration registers.
	INPUTREG 	= 4,  ///< Registers only used on A/D type devices.
	SETUPREG 	= 5,  ///< Advanced device configuration registers.
	SEAMAXPIO 	= 6   ///< Programmable type I/O.
} seaio_type_t;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \ingroup	group_seamax_all
/// \brief The type of IOCTL operation desired.
/// Read the manual carefully when using these.  There are some specific
/// methods that must be followed to use correctly.
// ----------------------------------------------------------------------------
typedef enum
{
	IOCTL_READ_COMM_PARAM 		= 1,  ///< Read communication parameters.
	IOCTL_SET_ADDRESS		= 2,  ///< Set device slave ID.
	IOCTL_SET_COMM_PARAM		= 3,  ///< Set communication parameters.
	IOCTL_GET_PIO			= 4,  ///< Get direction of programmable I/O.
	IOCTL_SET_PIO			= 5,  ///< Set direction of programmable I/O.
	IOCTL_GET_ADDA_CONFIG		= 6,  ///< Get A/D configuration information.
	IOCTL_SET_ADDA_CONFIG		= 7,  ///< Set the A/D configuration.
	IOCTL_GET_EXT_CONFIG 		= 8,  ///< Extended module id (SeaDAC).
	IOCTL_GET_ADDA_EXT_CONFIG	= 9,  ///< Information about D/A jumpers.
} IOCTL_t;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief 48 Bit pio configuration.
/// This struct is contained within the \a SeaMAX_PIO_ioctl_s struct.  It is
/// used whenever retrieving or setting port direction on a PIO device with
/// 48 bits of I/O.
// ----------------------------------------------------------------------------
typedef struct PIO48_config_s
{
	unsigned char channel1;         ///< Bits 0-5 map ports 1-6. (0:O, 1:I)
} PIO48_config_s;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief 96 Bit pio configuration.
/// This struct is contained within the \a SeaMAX_PIO_ioctl_s struct.  It is
/// used whenever retrieving or setting port direction on a PIO device with
/// 96 bits of I/O. 
// ----------------------------------------------------------------------------
typedef struct PIO96_config_s
{
	unsigned char channel1;         ///< Bits 0-5 map ports 1-6. (0:O, 1:I)
	unsigned char channel2;         ///< Bits 0-5 map ports 7-12. (0:O, 1:I)
} PIO96_config_s;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief This struct is used to set the address of a particular device.
/// In this way, it becomes unnecessary to manually turn the screw terminal to
/// configure a particular SeaIO device.  It also allows for more devices than
/// the physically selectable 15 addresses.
// ----------------------------------------------------------------------------
typedef struct seaio_ioctl_address_s
{
	unsigned char new_address;      ///< Address value from 1 to 247.
} seaio_ioctl_address_s;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief Communication parameters (desired) struct.
/// This struct can be used to set a desired communication configuration.
/// Note that you must first call get params before you attempt to set params,
/// or it will fail.
// ----------------------------------------------------------------------------
typedef struct seaio_ioctl_comms_s
{
	baud_rates_t new_baud_rate;     ///< Desire baud rate (/a baud_rates_t).
	parity_t new_parity;            ///< Desire parity (/a parity_t).
} seaio_ioctl_comms_s;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief Communication parameters (current) struct.
/// This struct can be used to store the current communication parameters.
/// Note that you will have call get params before you may set a configuration,
/// or it will fail.
// ----------------------------------------------------------------------------
typedef struct seaio_ioctl_get_params_s
{
	unsigned short 	model;        ///< Device model number (410, 462, ...)
	unsigned char  	bridge_type;  ///< Bridge type (M, E, U, S, or N).
	baud_rates_t 	baud_rate;    ///< The device's communication baud rate.
	parity_t 	parity;       ///< The device's communication parity.
	unsigned char	magic_cookie; ///< \internal Multithread safety.
} seaio_ioctl_get_params_s;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief PIO data/setup struct.
/// This struct can be used to read data from a PIO device, or configure a PIO
/// device.
// ----------------------------------------------------------------------------
typedef struct SeaMAX_PIO_ioctl_s
{
	unsigned short model;         ///< Device model number (462, 463, ...)
	union
	{
		PIO48_config_s PIO48; ///< PIO directions /a PIO48_config_s
		PIO96_config_s PIO96; ///< PIO directions /a PIO96_config_s
	} config_state;
} SeaMAX_PIO_ioctl_s;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief PIO data/setup struct.
/// This struct can be used to read data from a PIO device, or configure a PIO
/// device.
// ----------------------------------------------------------------------------
typedef struct seaio_ioctl_ext_config
{
	unsigned short model;	     ///< Device model number (SeaDAC)
} seaio_ioctl_ext_config;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief The IOCTL struct used in the majority of the IOCTL calls.
/// Every call except the three involving A/D and D/A converters use this 
/// struct.  This struct is actually the union of several smaller structs, so 
/// that it can be used multiple times.
// ----------------------------------------------------------------------------
typedef struct seaio_ioctl_s
{
	union
	{
		seaio_ioctl_address_s	  address; ///< IOCTL_SET_ADDRESS
		seaio_ioctl_comms_s	  comms;   ///< IOCTL_READ_COMM_PARAM
		seaio_ioctl_get_params_s  params;  ///< IOCTL_SET_COMM_PARAM
		SeaMAX_PIO_ioctl_s	  pio;     ///< IOCTL_GET/SET_PIO
		seaio_ioctl_ext_config	  config;  ///< IOCTL_GET_EXT_CONFIG
	} u;    ///< Keep size down to largest struct inside.
} seaio_ioctl_s;

// ----------------------------------------------------------------------------
// | ADDA structs and types.                                                  |
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief ADDA conversion range configuration type.
/// This is the range of voltages that the A/D will convert into values from
/// 0x000 to 0xFFF (0-4095).  Plus to minus conversions do use a sign bit, so
/// 4095 is not necessarily the highest value.
// ----------------------------------------------------------------------------
typedef enum
{
	ZERO_TO_FIVE	= 0,   ///<   0-5V  (0x000-0xFFF)
	PLS_MIN_FIVE	= 1,   ///<  -5-5V  (0x800-0x7FF)
	ZERO_TO_TEN	= 2,   ///<   0-10V (0x000-0xFFF)
	PLS_MIN_TEN	= 3    ///< -10-10V (0x800-0x7FF)
} channel_range_type;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// ADDA mode configuration type.
/// This is how the board is physically configured to read Analog signals.  The
/// way to measure.  You can either measure 16 signals with a single common
/// ground, 8 isolated signals, or the current pulled through internal resistors
/// from 8 separate signals.
// ----------------------------------------------------------------------------
typedef enum
{
	SINGLE_ENDED	= 0,   ///< 16 common ground channels.
	DIFFERENTIAL	= 1,   ///< 8 differential channels.
	CURRENT_LOOP	= 2    ///< 8 current loop measurements.
} channel_mode_type;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief ADDA reference type configuration.
/// This controls the mux that is in charge of input to the single A/D chip on
/// an A/D capable device.  This provides some small A/D diagnostics if you
/// desire to use them.
// ----------------------------------------------------------------------------
typedef enum
{
	ANALOG_OFFSET	= 0,   ///< The A/D inputs available on the device.
	GND_OFFSET	= 1,   ///< A ground value.  Should always read 0V.
	AD_REF_OFFSET	= 2,   ///< A/D reference value.  Should always read 0V.
	DA_CHANNEL_1	= 4,   ///< D/A channel one as input.
	DA_CHANNEL_2	= 8    ///< D/A channel two as input.
} ad_reference_type;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief  The ADDA data struct used in the ADDA ioctl calls.
/// Contains information about device configuration.
// ----------------------------------------------------------------------------
typedef struct adda_config
{
     /// Overall device settings.
	struct 
	{
		unsigned char reference_offset;  ///< A/D Mux address.
		unsigned char channel_mode;      ///< Measurement mode.

	} device;

	/// Each channel uses 2 bits for configuration.
	/// Each of the bytes in this struct uses the lowest
     /// two bits for each channel configuration.
	struct
	{
		unsigned char ch_1;
		unsigned char ch_2;
		unsigned char ch_3;
		unsigned char ch_4;
		unsigned char ch_5;
		unsigned char ch_6;
		unsigned char ch_7;
		unsigned char ch_8;
		unsigned char ch_9;
		unsigned char ch_10;
		unsigned char ch_11;
		unsigned char ch_12;
		unsigned char ch_13;
		unsigned char ch_14;
		unsigned char ch_15;
		unsigned char ch_16;

	} channels;
} adda_config;

// ----------------------------------------------------------------------------
/// \ingroup	group_seamax_all
/// \brief Data struct returned by get ext adda ioctl.
/// Contains information about the physical jumper configuration of the device.
// ----------------------------------------------------------------------------
typedef struct adda_ext_config
{
	unsigned char		ad_multiplier_enabled;  ///< A/D amplifier.

	channel_range_type	da_channel_1_range;     ///< D/A1 range.
	channel_range_type	da_channel_2_range;     ///< D/A2 range 
} adda_ext_config;

// ----------------------------------------------------------------------------
// Private
// SeaMaxModule struct.
// This structure is used internally to keep track of a module that open() has
// been called on.
// ----------------------------------------------------------------------------
typedef struct seaMaxModule
{
	int throttle;                   //Throttling delay for RTU mode.
	seaio_mode_t commMode;          //Communication medium (RTU or TCP).
	HANDLE hDevice;                 //Device comm interface.
	int mutex;                      //Multithread (force sequential).
	struct termios *initalConfig;   //Original serial configuration.
	void *libftdi;			//Library handle
	void *ftdic;			//For SeaDAC Lite modules
	int deviceType;
	
} seaMaxModule;

// ----------------------------------------------------------------------------
// |                             private prototypes                           |
// ----------------------------------------------------------------------------
int openD2X(SeaMaxLin *SeaMaxPointer, char *devName);
void closeD2X(SeaMaxLin *SeaMaxPointer);

// ----------------------------------------------------------------------------
// |                             API prototypes                               |
// ----------------------------------------------------------------------------

SeaMaxLin *SeaMaxLinCreate(void);
int SeaMaxLinDestroy(SeaMaxLin *SeaMaxPointer);
int SeaMaxLinOpen(SeaMaxLin *SeaMaxPointer, char *filename);
int SeaMaxLinClose(SeaMaxLin *SeaMaxPointer);

int SeaMaxLinRead(SeaMaxLin *SeaMaxPointer, slave_address_t slaveId, 
		  seaio_type_t type, address_loc_t starting_address,
		  address_range_t range, void *data);

int SeaDacLinRead(SeaMaxLin *SeaMaxPointer, unsigned char *data, 
			int numBytes);

int SeaMaxLinWrite(SeaMaxLin *SeaMaxPointer, slave_address_t slaveId, 
		  seaio_type_t type, address_loc_t starting_address,
		  address_range_t range, unsigned char *data);

int SeaDacLinWrite(SeaMaxLin *SeaMaxPointer, unsigned char *data, 
			int numBytes);

int SeaMaxLinIoctl(SeaMaxLin *SeaMaxPointer, slave_address_t slaveId,
		  IOCTL_t which, void *data);

int SeaMaxLinSetIMDelay(SeaMaxLin *SeaMaxPointer, int delay);

int SeaDacGetPIO(SeaMaxLin *SeaMaxPointer, unsigned char* data);

int SeaDacSetPIO(SeaMaxLin *SeaMaxPointer, unsigned char* data);

int SeaDacSetPIODirection(SeaMaxLin *SeaMaxPointer, unsigned char* data);

int SeaDacGetPIODirection(SeaMaxLin *SeaMaxPointer, unsigned char* data);

HANDLE SeaMaxLinGetCommHandle(SeaMaxLin *SeaMaxPointer);



// ----------------------------------------------------------------------------
// | End of C library function calls.                                         |
// ----------------------------------------------------------------------------
#ifdef __cplusplus
	}

class CSeaMaxLin {
public:

	CSeaMaxLin(void);
	~CSeaMaxLin(void);
	int Open(char *filename);
	int Close(void);

	int Read(slave_address_t slaveId, seaio_type_t type, 
			  address_loc_t starting_address, address_range_t range,
			  void *data);
	
	int Read(unsigned char *data, int length);
	
	int Write(slave_address_t slaveId, seaio_type_t type, 
			   address_loc_t starting_address,
			   address_range_t range, unsigned char *data);

	int Write(unsigned char *data, int length);
	
	int Ioctl(slave_address_t slaveId, IOCTL_t which, void *data);

	int set_intermessage_delay(int delay);
	
	int GetPIO(unsigned char* data);
	
	int SetPIO(unsigned char* data);
	
	int SetPIODirection(unsigned char* data);
	
	int GetPIODirection(unsigned char* data);
	
	HANDLE getCommHandle(void);

private:
	SeaMaxLin *SeaMaxPointer;
};

#endif //__cplusplus
#endif //SEAMAXLIN_H__
