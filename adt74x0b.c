/*
  *
  * A very simple user space program to read the temperature
  * from ADT7410 and ADT7420 I2C sensors.
  *
  * usage: adt74x0b
  *
  * This client is Raspberry Pi specific and uses Mike McCauley's
  * bcm2835 library http://www.airspayce.com/mikem/bcm2835/index.html
  * instead of the I2C drivers in the kernel.
  *
  * The kernel driver fails dismally with multiple sensors on the bus,
  * but the I2C support in libbcm2835 only supports revision 2 of the
  * Raspberry Pi hardware.
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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <bcm2835.h>

/* I2C registers in ADT74x0 */
#define T_MSB  0x00
#define T_LSB  0x01
#define STATUS 0x02
#define CONFIG 0x03
#define IDREG  0x0b
#define RESET  0x2f

#define I2C_ADDRS 128

static int init_adt74x0(const uint8_t addr);
static int read_adt74x0(const uint8_t addr, double *temp);

int main(int argc, const char *argv[])
{
  bcm2835_init();
  bcm2835_i2c_begin();

  // slowing down to 10kHz (std is 100kHz) works better
  // when the cables are long and termination dodgy...
  bcm2835_i2c_set_baudrate(10000);

  // Keep track of the status of all I2C devices:
  //    +ve good, 0 ignorable, -ve bad
  int8_t devs[I2C_ADDRS];
  for(int i = 0; i < I2C_ADDRS; i++)
    devs[i] = -1;

  // Initialize chips & start conversions
  for(int i = 0x48; i <= 0x4b; i++)
    {
      int stat = init_adt74x0(i);
      devs[i] = stat;
#ifdef DEBUG
      printf("# scan(addr = %02x) = %02x\n", i, -stat);
#endif
    }
  
  // Allow 1s for chips to read the temperature
  bcm2835_delay(1000);

  // Get results
  for(int i = 0; i < I2C_ADDRS; i++)
    {
      if (devs[i] != 0)
	continue;

      double t;
      int stat = read_adt74x0(i, &t);
      
      if (stat < 0) { printf("# 0x%02x error %d\n", i, stat); }
      else          { printf("0x%02x %.5fC\n", i, t);         }
    }

  bcm2835_i2c_end();
  bcm2835_close();
  
  return 0;
}

// Return 0 if OK, -ve to show error
static int init_adt74x0(const uint8_t addr)
{
  int stat;
  uint8_t buff[4];

  bcm2835_i2c_setSlaveAddress(addr);
  
  buff[0] = RESET;
  if ((stat = bcm2835_i2c_write((char *)buff, 1)) != 0)
    return -(0x10 + stat);

  bcm2835_delay(1); // Device needs 200us after reset, give it 1ms

  buff[0] = CONFIG;
  buff[1] = 0x80;    // 16bit cts conversions

  if ((stat = bcm2835_i2c_write((char *)buff, 2)) != 0)
    return -(0x20 + stat);

  char reg = IDREG;
  if ((stat = bcm2835_i2c_read_register_rs(&reg, (char *)buff, 1)) != 0)
    return -(0x30 + stat);

  if ((buff[0] & 0xf8) != 0xc8)
    return -0x3f;

  return 0;
}  

// Return 0 if OK, -ve to show error
// Set *temp to be the temperature in Celsius
static int read_adt74x0(const uint8_t addr, double *temp)
{
  int stat;
  uint8_t buff[4];

  bcm2835_i2c_setSlaveAddress(addr);
  
  char reg = T_MSB;
  if ((stat = bcm2835_i2c_read_register_rs(&reg, (char *)buff, 2)) != 0)
    return -(0x40 + stat);

  // ADT74x0 puts MSB first so flip order
  int16_t hi = buff[0];
  int16_t lo = buff[1];

  int16_t t128 = hi << 8 | lo;

  *temp = t128 / 128.0;

  return 0;
}
