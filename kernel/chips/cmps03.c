/*
    cmps03.c - Part of lm_sensors, Linux kernel modules for hardware
             monitoring
    Till Harbaum <till@harbaum.org>  2006-11-20    
    based on ds1621.c by Christian W. Zuckschwerdt <zany@triq.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x60, I2C_CLIENT_END };

/* Insmod parameters */
I2C_CLIENT_INSMOD_1(cmps03);

/* The CMPS03 registers */
#define CMPS03_REG_VERSION	        0x00 /* byte, RO */
#define CMPS03_REG_DIR_BYTE		0x01 /* byte, RO */
#define CMPS03_REG_DIR_DECDEG		0x02 /* word, RO */
#define CMPS03_REG_ZERO0		0x0c /* byte, RO */
#define CMPS03_REG_ZERO1		0x0d /* byte, RO */
#define CMPS03_REG_CALIBRATE		0x0f /* byte, RW */

/* Each client has this additional data */
struct cmps03_data {
	struct i2c_client client;
	struct semaphore update_lock;
	char valid;			/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

        u16 dir;	                /* Register values, word */
        u8 ver,cal;
};

static int cmps03_attach_adapter(struct i2c_adapter *adapter);
static int cmps03_detect(struct i2c_adapter *adapter, int address,
			 int kind);
static int cmps03_detach_client(struct i2c_client *client);
static struct cmps03_data *cmps03_update_client(struct device *dev);

/* This is the driver that will be inserted */
static struct i2c_driver cmps03_driver = {
        .driver = {
	          .name	= "cmps03",
	},
	.attach_adapter	= cmps03_attach_adapter,
	.detach_client	= cmps03_detach_client,
};

static int cmps03_read_value(struct i2c_client *client, u8 reg)
{
	if (reg != CMPS03_REG_DIR_DECDEG)
		return i2c_smbus_read_byte_data(client, reg);
	else
	        return swab16(i2c_smbus_read_word_data(client, reg));
}

static int cmps03_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static ssize_t show_version(struct device *dev,  struct device_attribute *attr, char *buf)
{
	struct cmps03_data *data = cmps03_update_client(dev);
	return sprintf(buf, "%d\n", data->ver);
}

static ssize_t show_heading(struct device *dev,  struct device_attribute *attr, char *buf)
{
	struct cmps03_data *data = cmps03_update_client(dev);
	return sprintf(buf, "%d\n", data->dir);
}

static ssize_t set_cal(struct device *dev,  struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cmps03_data *data = cmps03_update_client(dev);
	data->cal = simple_strtoul(buf, NULL, 10);
	cmps03_write_value(client, CMPS03_REG_CALIBRATE, data->cal);
	return count;
}

static DEVICE_ATTR(calibration, S_IWUSR, NULL, set_cal);
static DEVICE_ATTR(heading, S_IRUGO, show_heading, NULL);
static DEVICE_ATTR(version, S_IRUGO, show_version, NULL);

static int cmps03_attach_adapter(struct i2c_adapter *adapter)
{
	if (!(adapter->class & I2C_CLASS_HWMON))
		return 0;
	return i2c_probe(adapter, &addr_data, cmps03_detect);
}

/* This function is called by i2c_detect */
int cmps03_detect(struct i2c_adapter *adapter, int address,
                  int kind)
{
	int heading, zero;
	struct i2c_client *new_client;
	struct cmps03_data *data;
	int err = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA 
				     | I2C_FUNC_SMBUS_WORD_DATA 
				     | I2C_FUNC_SMBUS_WRITE_BYTE))
		goto exit;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access cmps03_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct cmps03_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct cmps03_data));
	
	new_client = &data->client;
	i2c_set_clientdata(new_client, data);
	new_client->addr = address;
	new_client->adapter = adapter;
	new_client->driver = &cmps03_driver;
	new_client->flags = 0;

	/* read software version */
	data->ver = cmps03_read_value(new_client, CMPS03_REG_VERSION);

	/* Now, we do the remaining detection. */
	if (kind < 0) {

	        /* only values 0 ... 3599 are valid */
		heading = cmps03_read_value(new_client, CMPS03_REG_DIR_DECDEG);
		if ((heading < 0) || (heading > 3599))
			goto exit_free;

	        /* these two registers must return 0 */
		zero = cmps03_read_value(new_client, CMPS03_REG_ZERO0);
		if (zero != 0)
			goto exit_free;

		zero = cmps03_read_value(new_client, CMPS03_REG_ZERO1);
		if (zero != 0)
			goto exit_free;
	}

	/* Determine the chip type - only one kind supported! */
	if (kind <= 0)
		kind = cmps03;

	/* Fill in remaining client fields and put it into the global list */
	strlcpy(new_client->name, "cmps03", I2C_NAME_SIZE);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto exit_free;

	/* Register sysfs hooks */
	device_create_file(&new_client->dev, &dev_attr_version);
	device_create_file(&new_client->dev, &dev_attr_heading);
	device_create_file(&new_client->dev, &dev_attr_calibration);
	
	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */
      exit_free:
	kfree(data);
      exit:
	return err;
}

static int cmps03_detach_client(struct i2c_client *client)
{
	int err;

	if ((err = i2c_detach_client(client))) {
		dev_err(&client->dev, "Client deregistration failed, "
			"client not detached.\n");
		return err;
	}

	kfree(i2c_get_clientdata(client));

	return 0;
}

static struct cmps03_data *cmps03_update_client(struct device *dev)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct cmps03_data *data = i2c_get_clientdata(client);

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

		dev_dbg(&client->dev, "Starting cmps03 update\n");

		data->ver = cmps03_read_value(client, CMPS03_REG_VERSION);
		data->dir = cmps03_read_value(client, CMPS03_REG_DIR_DECDEG);
		data->cal = cmps03_read_value(client, CMPS03_REG_CALIBRATE);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);

	return data;
}

static int __init cmps03_init(void)
{
	return i2c_add_driver(&cmps03_driver);
}

static void __exit cmps03_exit(void)
{
	i2c_del_driver(&cmps03_driver);
}


MODULE_AUTHOR("Till Harbaum <till@harbaum.org>");
MODULE_DESCRIPTION("CMPS03 driver");
MODULE_LICENSE("GPL");

module_init(cmps03_init);
module_exit(cmps03_exit);
