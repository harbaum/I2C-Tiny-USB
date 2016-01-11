/*
    tpa81.c - Part of lm_sensors, Linux kernel modules for hardware
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
static unsigned short normal_i2c[] = { 0x68, I2C_CLIENT_END };

/* Insmod parameters */
I2C_CLIENT_INSMOD_1(tpa81);

/* The TPA81 registers */
#define TPA81_REG_VERSION	        0x00
#define TPA81_REG_AMBIENT		0x01
#define TPA81_REG_PIXEL1		0x02
#define TPA81_REG_PIXEL2		0x03
#define TPA81_REG_PIXEL3		0x04
#define TPA81_REG_PIXEL4		0x05
#define TPA81_REG_PIXEL5		0x06
#define TPA81_REG_PIXEL6		0x07
#define TPA81_REG_PIXEL7		0x08
#define TPA81_REG_PIXEL8		0x09

/* Each client has this additional data */
struct tpa81_data {
	struct i2c_client client;
	struct semaphore update_lock;
	char valid;			/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

        u8 ver, ambient;
        u8 pixel[8];
};

static int tpa81_attach_adapter(struct i2c_adapter *adapter);
static int tpa81_detect(struct i2c_adapter *adapter, int address,
			 int kind);
static int tpa81_detach_client(struct i2c_client *client);
static struct tpa81_data *tpa81_update_client(struct device *dev);

/* This is the driver that will be inserted */
static struct i2c_driver tpa81_driver = {
        .driver = {
	          .name	= "tpa81",
	},
	.attach_adapter	= tpa81_attach_adapter,
	.detach_client	= tpa81_detach_client,
};

static int tpa81_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static ssize_t show_version(struct device *dev,  struct device_attribute *attr, char *buf)
{
	struct tpa81_data *data = tpa81_update_client(dev);
	return sprintf(buf, "%d\n", data->ver);
}

static ssize_t show_ambient(struct device *dev,  struct device_attribute *attr, char *buf)
{
	struct tpa81_data *data = tpa81_update_client(dev);
	return sprintf(buf, "%d\n", data->ambient);
}

#define show_pixel(value)  \
static ssize_t show_pixel##value(struct device *dev, struct device_attribute *attr, char *buf)               \
{                                                                       \
        struct tpa81_data *data = tpa81_update_client(dev);           \
        return sprintf(buf, "%d\n", data->pixel[value-1]);   \
}

show_pixel(1);
show_pixel(2);
show_pixel(3);
show_pixel(4);
show_pixel(5);
show_pixel(6);
show_pixel(7);
show_pixel(8);

static DEVICE_ATTR(version, S_IRUGO, show_version, NULL);
static DEVICE_ATTR(ambient, S_IRUGO, show_ambient, NULL);
static DEVICE_ATTR(pixel1, S_IRUGO, show_pixel1, NULL);
static DEVICE_ATTR(pixel2, S_IRUGO, show_pixel2, NULL);
static DEVICE_ATTR(pixel3, S_IRUGO, show_pixel3, NULL);
static DEVICE_ATTR(pixel4, S_IRUGO, show_pixel4, NULL);
static DEVICE_ATTR(pixel5, S_IRUGO, show_pixel5, NULL);
static DEVICE_ATTR(pixel6, S_IRUGO, show_pixel6, NULL);
static DEVICE_ATTR(pixel7, S_IRUGO, show_pixel7, NULL);
static DEVICE_ATTR(pixel8, S_IRUGO, show_pixel8, NULL);

static int tpa81_attach_adapter(struct i2c_adapter *adapter)
{
	if (!(adapter->class & I2C_CLASS_HWMON))
		return 0;
	return i2c_probe(adapter, &addr_data, tpa81_detect);
}

/* This function is called by i2c_detect */
int tpa81_detect(struct i2c_adapter *adapter, int address,
                  int kind)
{
        int ver, tmp, i;
	struct i2c_client *new_client;
	struct tpa81_data *data;
	int err = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA 
				     | I2C_FUNC_SMBUS_WORD_DATA 
				     | I2C_FUNC_SMBUS_WRITE_BYTE))
		goto exit;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access tpa81_{read,write}_value. */
	if (!(data = kmalloc(sizeof(struct tpa81_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct tpa81_data));
	
	new_client = &data->client;
	i2c_set_clientdata(new_client, data);
	new_client->addr = address;
	new_client->adapter = adapter;
	new_client->driver = &tpa81_driver;
	new_client->flags = 0;

	/* read software version */
	data->ver = tpa81_read_value(new_client, TPA81_REG_VERSION);

	/* Now, we do the remaining detection. */
	if (kind < 0) {
		ver = tpa81_read_value(new_client, TPA81_REG_VERSION);
		if ((ver < 6) || (ver > 8))
			goto exit_free;

		for(i=0;i<9;i++) {
		        tmp = tpa81_read_value(new_client, TPA81_REG_AMBIENT+i);
		        if ((tmp < 4) || (tmp > 100))
			        goto exit_free;
		}
	}

	/* Determine the chip type - only one kind supported! */
	if (kind <= 0)
		kind = tpa81;

	/* Fill in remaining client fields and put it into the global list */
	strlcpy(new_client->name, "tpa81", I2C_NAME_SIZE);
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto exit_free;

	/* Register sysfs hooks */
	device_create_file(&new_client->dev, &dev_attr_version);
	device_create_file(&new_client->dev, &dev_attr_ambient);
	device_create_file(&new_client->dev, &dev_attr_pixel1);
	device_create_file(&new_client->dev, &dev_attr_pixel2);
	device_create_file(&new_client->dev, &dev_attr_pixel3);
	device_create_file(&new_client->dev, &dev_attr_pixel4);
	device_create_file(&new_client->dev, &dev_attr_pixel5);
	device_create_file(&new_client->dev, &dev_attr_pixel6);
	device_create_file(&new_client->dev, &dev_attr_pixel7);
	device_create_file(&new_client->dev, &dev_attr_pixel8);
	
	return 0;

        /* OK, this is not exactly good programming practice, usually. But it is
	   very code-efficient in this case. */
        exit_free:
	        kfree(data);
        exit:
	        return err;
}

static int tpa81_detach_client(struct i2c_client *client)
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

static struct tpa81_data *tpa81_update_client(struct device *dev)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct tpa81_data *data = i2c_get_clientdata(client);
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > HZ + HZ / 2) ||
	    (jiffies < data->last_updated) || !data->valid) {

		dev_dbg(&client->dev, "Starting tpa81 update\n");

		data->ver = tpa81_read_value(client, TPA81_REG_VERSION);
		data->ambient = tpa81_read_value(client, TPA81_REG_AMBIENT);
		for(i=0;i<8;i++)
		        data->pixel[i] = tpa81_read_value(client, TPA81_REG_PIXEL1+i);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);

	return data;
}

static int __init tpa81_init(void)
{
        printk("tpa81 driver loaded\n");
	return i2c_add_driver(&tpa81_driver);
}

static void __exit tpa81_exit(void)
{
	i2c_del_driver(&tpa81_driver);
}


MODULE_AUTHOR("Till Harbaum <till@harbaum.org>");
MODULE_DESCRIPTION("TPA81 driver");
MODULE_LICENSE("GPL");

module_init(tpa81_init);
module_exit(tpa81_exit);
