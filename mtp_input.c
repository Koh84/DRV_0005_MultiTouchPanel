/* This file is just a driver, not the device */
/* we need to disable the original driver ft5x06_ts.c by changing Makefile in /drivers/input/touchscreen*/
/* we also need to disable the original device "ft5x0x_ts" in Mach-tiny4412.c */
/* comment out line 4141 in Mach-tiny4412.c where i2c_register_board_info(1, i2c_devs1, ....)*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <asm/mach/irq.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

#define MTP_ADDR 	(0x70 >> 1)
#define MTP_MAX_X 	800
#define MTP_MAX_Y 	480

#define MTP_IRQ 	gpio_to_irq(EXYNOS4_GPX1(6))

#define MTP_NAME "ft5x0x_ts"
#define MTP_MAX_ID 15

struct input_dev *ts_dev;
static struct work_struct mtp_work;
static struct i2c_client *mtp_client;

struct mtp_event {
	int x;
	int y;
	int id;
};

static struct mtp_event mtp_events[16];
static int mtp_points;

static irqreturn_t mtp_interrupt(int irq, void *dev_id)
{
	/* we are supposed to get multitouch parameters here, and the input_event */
	/* however i2c is too slow, so can't put in interrupt here */
	/* we hence used work_queue to handle this */
	schedule_work(&mtp_work);

	return IRQ_HANDLED;
}

/* start i2c transfer */
static int mtp_ft5x0x_i2c_rxdata(struct i2c_client *client, char *rxdata, int length) {
	int ret;
	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("%s: i2c read error: %d\n", __func__, ret);

	return ret;
}

static int mtp_ft5x0x_read_data(void) {
	
	u8 buf[32] = { 0 };
	int ret;

	/* multitouch touch screen */
	/* read all the data, contain 32 buffer in total, see pdf of ft5x06 chip */
	ret = mtp_ft5x0x_i2c_rxdata(mtp_client, buf, 31);

	if (ret < 0) {
		printk("%s: read touch data failed, %d\n", __func__, ret);
		return ret;
	}

	//memset(event, 0, sizeof(struct ft5x0x_event));
	mtp_points = buf[2] & 0x0f;       /*how many touch points*/

	/* get the coordinate of x y and id for the touches */
	switch (mtp_points) {
		case 5:
			mtp_events[4].x = (s16)(buf[0x1b] & 0x0F)<<8 | (s16)buf[0x1c];
			mtp_events[4].y = (s16)(buf[0x1d] & 0x0F)<<8 | (s16)buf[0x1e];
			mtp_events[4].id = buf[0x1d] >> 4;
		case 4:
			mtp_events[3].x = (s16)(buf[0x15] & 0x0F)<<8 | (s16)buf[0x16];
			mtp_events[3].y = (s16)(buf[0x17] & 0x0F)<<8 | (s16)buf[0x18];
			mtp_events[3].id = buf[0x17] >> 4;
		case 3:
			mtp_events[2].x = (s16)(buf[0x0f] & 0x0F)<<8 | (s16)buf[0x10];
			mtp_events[2].y = (s16)(buf[0x11] & 0x0F)<<8 | (s16)buf[0x12];
			mtp_events[2].id = buf[0x11] >> 4;
		case 2:
			mtp_events[1].x = (s16)(buf[0x09] & 0x0F)<<8 | (s16)buf[0x0a];
			mtp_events[1].y = (s16)(buf[0x0b] & 0x0F)<<8 | (s16)buf[0x0c];
			mtp_events[1].id = buf[0x0b] >> 4;
		case 1:
			mtp_events[0].x = (s16)(buf[0x03] & 0x0F)<<8 | (s16)buf[0x04];
			mtp_events[0].y = (s16)(buf[0x05] & 0x0F)<<8 | (s16)buf[0x06];
			mtp_events[0].id = buf[0x05] >> 4;
			break;
		case 0:
			return 0;
		default:
			//printk("%s: invalid touch data, %d\n", __func__, event->touch_point);
			return -1;
	}

	return 0;
}

static void mtp_work_func(struct work_struct *work)
{
	int i;
	int ret;

	/* read i2c device, obtain the multitouch parameters and report back */
	/* read */
	ret = mtp_ft5x0x_read_data();
	if(ret < 0)
		return;

	/* report */
	/* release touch, there is no touch points */
	if (!mtp_points) {
		input_mt_sync(ts_dev);
		input_sync(ts_dev);
		return;
	}

	/* report */
	for(i = 0; i< mtp_points; i++)
	{
		input_report_abs(ts_dev, ABS_MT_POSITION_X, mtp_events[i].x);
		input_report_abs(ts_dev, ABS_MT_POSITION_Y, mtp_events[i].y);
		input_report_abs(ts_dev, ABS_MT_TRACKING_ID, mtp_events[i].id);
		input_mt_sync(ts_dev);	
	}
	input_sync(ts_dev);

}

static int __devinit mtp_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	mtp_client = client;

	/* Allocate input device */
	ts_dev = input_allocate_device();	

	/* Set it up */
	/* what type of events */
	set_bit(EV_SYN, ts_dev->evbit); //sync event
	set_bit(EV_ABS, ts_dev->evbit); //absolute positioning

	/* what type of sub-events */
	set_bit(ABS_MT_TRACKING_ID, ts_dev->absbit);
	set_bit(ABS_MT_POSITION_X, ts_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, ts_dev->absbit);

	/* the scope of the events */
	input_set_abs_params(ts_dev, ABS_MT_TRACKING_ID, 0,10, 0, 0); /*10 points touch screen*/
	input_set_abs_params(ts_dev, ABS_MT_POSITION_X, 0, MTP_MAX_X, 0, 0);
	input_set_abs_params(ts_dev, ABS_MT_POSITION_Y, 0, MTP_MAX_Y, 0, 0);

	ts_dev->name = MTP_NAME; /* android use this to find the right device file */

	/* register this input device */
	input_register_device(ts_dev);

	/* hardware related */
	INIT_WORK(&mtp_work, mtp_work_func);
	
	request_irq(MTP_IRQ, mtp_interrupt, IRQ_TYPE_EDGE_FALLING, "KelvinKoh_mtp", ts_dev);

	return 0;
}

static int __devexit mtp_remove(struct i2c_client *client)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	free_irq(MTP_IRQ, ts_dev);
	cancel_work_sync(&mtp_work);
	input_unregister_device(ts_dev);
	input_free_device(ts_dev);
	return 0;
}

static const struct i2c_device_id mtp_id_table[] = {
	{ "KelvinKoh_mtp", 0 },
	{}
};

static int mtp_ft5x06_valid(struct i2c_client *client)
{
	u8 buf[32] = { 0 };
	int ret;

	/* if we are here, it means there is a device with this address */
	/* but we need to make sure we are targeting the right device */
	/* we need to detect the right chip and we can use this function */
	printk("mtp_ft5x06_valid : addr = 0x%x\n", client->addr);

	/* proceed to judge the device validity */
	/* multitouch touch screen */
	/* read all the data, contain 32 buffer in total, see pdf of ft5x06 chip */
	buf[0] = 0xa3; /*chip vendor id */
	ret = mtp_ft5x0x_i2c_rxdata(client, buf, 1);
	if(ret < 0){
		printk("There is no real device, i2c read err\n");
		return ret;
	}

	printk("chip vendor id = 0x%x\n", buf[0]);

	if(buf[0] != 0x55)
	{
		printk("There is no real device, val err\n");
		return -1;
	}

	return 0;
}

/*This function is called from i2c_detect_address in i2c-core.c*/
static int mtp_detect(struct i2c_client *client,
		       struct i2c_board_info *info)
{
	printk("mtp_detect : addr = 0x%x\n", client->addr);

	if(mtp_ft5x06_valid(client) < 0)
		return -1;

	/* we basically just cheat here, making the same match between device and driver name */
	strlcpy(info->type, "KelvinKoh_mtp", I2C_NAME_SIZE); 	/*if this name matched the name in .id_table below*/
	return 0;						/*the probe function will be called"/
	/* return 0 and create i2c device */
	/* i2c_new_device(adapter, &info), the info above is used here */
	/* info->type = "KelvinKoh_mtp" */
	/* this device is compare to the one in id table */
}

static const unsigned short addr_list[] = { MTP_ADDR, I2C_CLIENT_END };

/* allocate/setup i2c driver */
static struct i2c_driver mtp_driver = {
	.class  = I2C_CLASS_HWMON, /* where to look for adaptors */
	.driver	= {
		.name	= "KelvinKoh",
		.owner	= THIS_MODULE,
	},
	.probe		= mtp_probe,
	.remove		= __devexit_p(mtp_remove),
	.id_table	= mtp_id_table,
	.detect     = mtp_detect,  	/* use this to validate availability of device*/
	.address_list	= addr_list,   	/* address of this device */
};

static int mtp_drv_init(void)
{
	/* register i2c driver */
	i2c_add_driver(&mtp_driver);
	
	return 0;
}

static void mtp_drv_exit(void)
{
	i2c_del_driver(&mtp_driver);
}


module_init(mtp_drv_init);
module_exit(mtp_drv_exit);
MODULE_LICENSE("GPL");


