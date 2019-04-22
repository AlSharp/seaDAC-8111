/*
 * ftdi.h
 * SeaMAX for Linux
 *
 * This code defines libftdi specific entry point function pointers for
 * use with dynamic library calls (dlopen/dlsym).
 *
 * Sealevel and SeaMAX are registered trademarks of Sealevel Systems
 * Incorporated.
 *
 * based upon ftdi.h - Apr 4 2003 - (C) 2003 by Intra2net AG
 * © 2009-2017 Sealevel Systems, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the Lesser GNU General Public License
 * as published by the Free Software Foundation; either version
 * 3 of the License, or (at your option) any later version.
 * LGPL v3
 */

#ifndef __libftdi_bind_h__
#define __libftdi_bind_h__

/** MPSSE bitbang modes */
enum ftdi_mpsse_mode
{
    BITMODE_RESET  = 0x00,
    BITMODE_BITBANG= 0x01,
    BITMODE_MPSSE  = 0x02,
    BITMODE_SYNCBB = 0x04,
    BITMODE_MCU    = 0x08,
    /* CPU-style fifo mode gets set via EEPROM */
    BITMODE_OPTO   = 0x10,
    BITMODE_CBUS   = 0x20
};

typedef void * ftdi_context;

//struct ftdi_context *ftdi_new();
typedef ftdi_context (*pf_ftdi_new)();
//void ftdi_free(struct ftdi_context *ftdi);
typedef void (*pf_ftdi_free)(ftdi_context ftdi);

//int ftdi_init(struct ftdi_context *ftdi);
typedef int (*pf_ftdi_init)(ftdi_context ftdi);
//void ftdi_deinit(struct ftdi_context *ftdi);
typedef void (*pf_ftdi_deinit)(ftdi_context ftdi);

//int ftdi_usb_open(struct ftdi_context *ftdi, int vendor, int product);
typedef int (*pf_ftdi_usb_open)(ftdi_context ftdi, int vendor, int product);
//int ftdi_usb_close(struct ftdi_context *ftdi);
typedef int (*pf_ftdi_usb_close)(ftdi_context ftdi);
//int ftdi_usb_purge_buffers(struct ftdi_context *ftdi);
typedef int (*pf_ftdi_usb_purge_buffers)(ftdi_context ftdi);

//int ftdi_read_data(struct ftdi_context *ftdi, unsigned char *buf, int size);
typedef int (*pf_ftdi_read_data)(ftdi_context ftdi, unsigned char *buf, int size);
//int ftdi_write_data(struct ftdi_context *ftdi, unsigned char *buf, int size);
typedef int (*pf_ftdi_write_data)(ftdi_context ftdi, unsigned char *buf, int size);

//int ftdi_enable_bitbang(struct ftdi_context *ftdi, unsigned char bitmask);
typedef int (*pf_ftdi_enable_bitbang)(ftdi_context ftdi, unsigned char bitmask);
//int ftdi_disable_bitbang(struct ftdi_context *ftdi);
typedef int (*pf_ftdi_disable_bitbang)(ftdi_context ftdi);
//int ftdi_set_bitmode(struct ftdi_context *ftdi, unsigned char bitmask, unsigned char mode);
typedef int (*pf_ftdi_set_bitmode)(ftdi_context ftdi, unsigned char bitmask, unsigned char mode);
//int ftdi_read_pins(struct ftdi_context *ftdi, unsigned char *pins);
typedef int (*pf_ftdi_read_pins)(ftdi_context ftdi, unsigned char *pins);

//char *ftdi_get_error_string(struct ftdi_context *ftdi);
typedef char* (*pf_ftdi_get_error_string)(ftdi_context ftdi);
 
#endif /* __libftdi_bind_h__ */
