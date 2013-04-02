 /*
  *
  * A very simple user space program to read the temperature
  * from ADT7410 and ADT7420 I2C sensors.
  *
  * usage: adt74x0 /dev/i2c-0
  *
  * Feature free but works on the Raspberry Pi where the I2C
  * bus doesn't play well with the chips. I think it's this issue:
  *   http://www.raspberrypi.org/phpBB3/viewtopic.php?f=44&t=15840
  *
  * The main consequence is that you can't easily be sure that
  * the chips really are ADT74x0s, which isn't the end-of-the-world.
  * However, if you're running this on a saner bus, define 
  * GOOD_I2C_BUS for more checks (notably that the ID code is 0b11001xxx).
  *
  * ADT data can be found at:
  *   http://www.analog.com/en/mems-sensors/digital-temperature-sensors/adt7410/products/product.html
  *   http://www.analog.com/en/mems-sensors/digital-temperature-sensors/adt7420/products/product.html
  *
  * LICENSE
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU General Public License as
  * published by the Free Software Foundation; either version 2 of
  * the License, or (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful, but
  * WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  * General Public License for more details at
  * http://www.gnu.org/copyleft/gpl.html
  *
  * COPYRIGHT
  *
  * Copyright (C) 2013 Martin Oldfield <adt74x0-prog-2013@mjo.tc>
  *
  */

#define _BSD_SOURCE // So that we can usleep()

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

/* I2C registers in ADT74x0 */
#define T_MSB  0x00
#define T_LSB  0x01
#define STATUS 0x02
#define CONFIG 0x03
#define IDREG  0x0b
#define RESET  0x2f

#define I2C_ADDRS 128

static int init_adt74x0(const int file, const int addr);
static int read_adt74x0(const int file, const int addr, double *temp);



int main(int argc, const char *argv[])
{
  const char default_file[] = "/dev/i2c-0";
  const char *filename = (argc > 1) ? argv[1] : default_file;

  printf("# Scanning %s for ADT74x0...\n", filename);

  int file = open(filename,O_RDWR);
  if (file < 0) {
    printf("Unable to open %s\n", filename);
    exit(1);
  }

  // Keep track of the status of all I2C devices:
  //    +ve good, 0 ignorable, -ve bad
  int8_t devs[I2C_ADDRS];
  for(int i = 0; i < I2C_ADDRS; i++)
    devs[i] = (i >= 0x48 && i <= 0x4b) ? 1 : 0;

  // Initialize chips & start conversions
  for(int i = 0; i < I2C_ADDRS; i++)
    {
      if (devs[i] <= 0)
	continue;

      int stat = init_adt74x0(file, i);
      if (stat < 0)
	devs[i] = stat;
    }

  // Allow 1s for chips to read the temperature
  usleep(1000000);

  // Get results
  for(int i = 0; i < I2C_ADDRS; i++)
    {
      if (devs[i] <= 0)
	continue;

      double t;
      int stat = read_adt74x0(file, i, &t);

      if (stat < 0) { printf("# 0x%02x error %d\n", i, stat); }
      else          { printf("0x%02x %.5fC\n", i, t);         }
    }

  close(file);
  
  return 0;
}

// Return 0 if OK, -ve to show error
static int init_adt74x0(const int file, const int addr)
{
  if (ioctl(file,I2C_SLAVE,addr) < 0)
    return -1;

  if (i2c_smbus_write_byte(file, RESET) < 0)
    return -2;

  usleep(250); // Device needs 200us after reset
  
#ifdef GOOD_I2C_BUS
  // This call fails on e.g. the Raspberry Pi
  // presumably because of some oddity with their i2c hardware
  // see the preamble at the top of the file for more.
  int stat = i2c_smbus_read_byte_data(file, IDREG);  
  if (stat < 0)
    return -3;

  printf("# 0x%02x has ID 0x%02x\n", addr, stat);
  if ((stat & 0xf8) != 0xc8)
    return -4;
#endif

  if (i2c_smbus_write_byte_data(file, CONFIG, 0x80) < 0) // 16bit cts conversions
    return -5;

  return 0;
}  

// Return 0 if OK, -ve to show error
// Set *temp to be the temperature in Celsius
static int read_adt74x0(const int file, const int addr, double *temp)
{
  if (ioctl(file,I2C_SLAVE,addr) < 0)
    return -1;

  int32_t t_raw  = i2c_smbus_read_word_data(file, T_MSB);
  if (t_raw < 0)
    return -6;
 
  // ADT74x0 puts MSB first so flip order
  int16_t lo = (t_raw & 0xff00) >> 8;
  int16_t hi = (t_raw & 0x00ff);

  int16_t t128 = hi << 8 | lo;

  *temp = t128 / 128.0;

  return 0;
}
