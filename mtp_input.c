#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#define MTP_ADDR 	0x70 
#define MTP_MAX_X 	800
#define MTP_MAX_Y 	480
#define MTP_IRQ 	123

struct input_dev *ts_dev;
static struct work_struct mtp_work;

static irqreturn_t mtp_interrupt(int irq, void *dev_id)
{
	/* we are supposed to get multitouch parameters here, and the input_event */
	/* however i2c is too slow, so can't put in interrupt here */
	/* we hence used work_queue to handle this */
	schedule_work(&mtp_work);

	return IRQ_HANDLED;
}

static void mtp_work_func(struct work_struct *work)
{
	/* read i2c device, obtain the multitouch parameters and report back */
	/* read */
	
	/* report */
	for()
	{
		input_report_abs(ts_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(ts_dev, ABS_MT_TRACKING_ID, id);
		input_mt_sync(ts_dev);	
	}
	input_sync(ts_dev);

}

static int __devinit mtp_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	/* Allocate input device */
	ts_dev = input_allocate_device();	

	/* Set it up */
	/* what type of events */
	set_bit(EV_SYN, ts_dev->evbit); //sync event
	set_bit(EV_ABC, ts_dev->evbit); //absolute positioning

	/* what type of sub-events */
	set_bit(ABS_MT_TRACKING_ID, ts_dev->absbit);
	set_bit(ABS_MT_POSITION_X, ts_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, ts_dev->absbit);

	/* the scope of the events */
	input_set_abs_params(ts_dev, ABS_MT_TRACKING_ID, 0,10, 0, 0); /*10 points touch screen*/
	input_set_abs_params(ts_dev, ABS_MT_POSITION_X, 0, MTP_MAX_X, 0, 0);
	input_set_abs_params(ts_dev, ABS_MT_POSITION_Y, 0, MTP_MAX_Y, 0, 0);

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
	cancel_work_synce(&mtp_work);
	input_unregister_device(ts_dev);
	input_free_device(ts_dev);
	return 0;
}

static const struct i2c_device_id mtp_id_table[] = {
	{ "KelvinKoh_mtp", 0 },
	{}
};

/*This function is called from i2c_detec_address in i2c-core.c*/
static int mtp_detect(struct i2c_client *client,
		       struct i2c_board_info *info)
{
	printk("mtp_detect : addr = 0x%x\n", client->addr);

	/* proceed to judge with brand it's */
	
	strlcpy(info->type, "KelvinKoh_mtp", I2C_NAME_SIZE); /*if this name matched the name in .id_table below*/
	return 0;											 /*the probe function will be called"/
	/* return 0 and create i2c device */
	/* i2c_new_device(adapter, &info), the info above is used here */
	/* this device is compare to the one in id table */
}

static const unsigned short addr_list[] = { MTP_ADDR, I2C_CLIENT_END };

/* 1. 分配/设置i2c_driver */
static struct i2c_driver mtp_driver = {
	.class  = I2C_CLASS_HWMON, /* 表示去哪些适配器上找设备 */
	.driver	= {
		.name	= "100ask",
		.owner	= THIS_MODULE,
	},
	.probe		= mtp_probe,
	.remove		= __devexit_p(mtp_remove),
	.id_table	= mtp_id_table,
	.detect     = mtp_detect,  /* 用这个函数来检测设备确实存在 */
	.address_list	= addr_list,   /* 这些设备的地址 */
};

static int mtp_drv_init(void)
{
	/* 2. 注册i2c_driver */
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


