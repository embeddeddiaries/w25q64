#include<linux/init.h>
#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/slab.h>
#include<linux/kernel.h>
#include<linux/version.h>
#include<linux/errno.h>
#include<linux/device.h>
#include<linux/mutex.h>
#include<linux/delay.h>
#include<linux/uaccess.h>
#include<linux/ioctl.h>
#include<linux/spi/spi.h>
#include<linux/of.h>

#define DEVICE_NAME     "w25q64"

#define W25_MAGIC       'W'
#define ERASE_NO	0x01
#define W25Q_ERASE	_IO(W25_MAGIC,ERASE_NO)

static unsigned int w25q_major;
static unsigned int w25q_minor;

struct w25q_data {
    struct cdev dev;
    uint32_t cursor;
    struct spi_device *spi;
};

struct class *w25q_class = NULL;

int w25q_open(struct inode *inode,struct file *filp)
{
    struct w25q_data *w25q = NULL;

    w25q = container_of(inode->i_cdev,struct w25q_data,dev);

    if(w25q == NULL){
        pr_err("container_of did not found valid data\n");
        return -ENODEV;
    }

    filp->private_data = w25q;
    //w25q->cursor = 0;
    return 0;

}

int w25q_release(struct inode *inode,struct file *filp)
{
    return 0;
}

void w25q_write_enable(struct spi_device *spi,bool en)
{
    int8_t ret,wen;
    
    if(en)
    {
        wen = 0x06;
    }
    else
    {
	wen = 0x04;
    }
   
    ret = spi_write(spi,&wen,1);
    if(ret != 0)
    {
	pr_err("w25q_write_enable 1 failed\n");
    }

}

bool w25q_check_busy(struct spi_device *spi)
{
    int8_t ret,status_reg1 = 0x05,busy = 1;
    
    while(busy == 1)
    {
	ret = spi_write_then_read(spi,&status_reg1,1,&busy,1);
	if(ret != 0)
	{
	    pr_err("w25q_check_busy 1 failed\n");
	}
	//pr_info("busy\n");
	busy = (busy & (1 << 0x00));
	mdelay(50);
    }
    return 0;
}


void w25q_chip_erase(struct spi_device *spi)
{
    int8_t ret,chip_erase;
    
    chip_erase = 0x60;
    
    w25q_write_enable(spi,true);
    ret = spi_write(spi,&chip_erase,1);
    if(ret != 0)
    {
	pr_err("w25q_chip_erase 1 failed\n");
    }
    
    w25q_check_busy(spi);
    w25q_write_enable(spi,false);
}

ssize_t w25q_write(struct file *filp,const char __user *buff,size_t count,loff_t *offset)
{
    uint8_t *in_addr_data,*kernelBuff;
    struct w25q_data *w25q = filp->private_data;
    int8_t ret;
    uint32_t index;

    pr_info("%s\n",__func__);
    
    w25q_chip_erase(w25q->spi);
    w25q_write_enable(w25q->spi,true);

    kernelBuff = kmalloc(count,GFP_KERNEL);
    if(!kernelBuff)
    {
	pr_err("No memory\n");
	return -1;
    }
    if(copy_from_user(kernelBuff,buff,count))
    {
	pr_err("copy_from_user fail\n");
    }
   
    in_addr_data = kmalloc(count+4,GFP_KERNEL);
    if(!in_addr_data)
    {
	pr_err("No memory\n");
	return -1;
    }

    in_addr_data[0] = 0x02;
    in_addr_data[1] = 0x00;
    in_addr_data[2] = 0x00;
    in_addr_data[3] = 0x00;

    for(index = 4; index < count+4; index++)
    {
	in_addr_data[index] = kernelBuff[index - 4];
    }
  
    ret = spi_write(w25q->spi,in_addr_data,count+4);
    if(ret != 0)
    {
	pr_err("spi_write 2 failed\n");
    }
    
    w25q->cursor = count;

    w25q_check_busy(w25q->spi);
    w25q_write_enable(w25q->spi,false);
    
    kfree(kernelBuff);
    kfree(in_addr_data);
    
    return count;
}

ssize_t w25q_read(struct file *filp,char __user *buf,size_t count,loff_t *f_pos)
{
    struct w25q_data *w25q = filp->private_data;
    uint8_t *kernelBuff,in_addr[4];
    
    pr_info("%s\n",__func__);

    w25q_check_busy(w25q->spi);
    count = w25q->cursor;
    
    kernelBuff = kmalloc(count,GFP_KERNEL);
    if(!kernelBuff)
    {
	pr_err("No memory\n");
	return -1;
    }

    in_addr[0] = 0x03;
    in_addr[1] = 0x00;
    in_addr[2] = 0x00;
    in_addr[3] = 0x00;
    
    spi_write_then_read(w25q->spi,in_addr,4,kernelBuff,count);

    if(copy_to_user(buf,kernelBuff,count))
    {
	pr_err("copy_from_user fail\n");
    }
    kfree(kernelBuff);
    return count;
}

static long w25q_ioctl(struct file *filp,unsigned int cmd,unsigned long arg)
{   
    struct w25q_data *w25q = filp->private_data;
    switch(cmd)
    {
	case W25Q_ERASE:
	    w25q_chip_erase(w25q->spi);
	    w25q_check_busy(w25q->spi);
	    break;
    }
    return 0;
}

static struct file_operations w25q_fops = {
    .owner = THIS_MODULE,
    .open = w25q_open,
    .release = w25q_release,
    .read = w25q_read,
    .write = w25q_write,
    .unlocked_ioctl = w25q_ioctl,
};


static int w25q_probe(struct spi_device *spi)
{
    int8_t ret;
    int err;
    dev_t dev_no;
    struct device *w25q_device=NULL;

    struct w25q_data *w25q;

    uint8_t in_addr[4],mf_id[2] = {0,};
    pr_info("%s\n",__func__);

    spi->mode = SPI_MODE_0;
    spi->max_speed_hz = 2000000;
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    ret = spi_setup(spi);
    
    if(ret < 0){
	pr_err("spi_setup error\n");
    }

    in_addr[0] = 0x90;
    in_addr[1] = 0x00;
    in_addr[2] = 0x00;
    in_addr[3] = 0x00;
    
    spi_write_then_read(spi,in_addr,4,mf_id,2);
    pr_info("spi_read = %x %x\n",mf_id[0],mf_id[1]);
    
    if(mf_id[0] != 0xEF || mf_id[1] != 0x16)
    {
	pr_info("w25q64 not found!!\n");
    }
    
    pr_info("w25q64 found!!\n");

    /* Allocate the major and minor number */
    err = alloc_chrdev_region(&dev_no,0,1,DEVICE_NAME);
    if(err < 0){ 
        pr_err("alloc_chrdev_region fail\n");
        return err;
    }   
    w25q_major = MAJOR(dev_no);
    w25q_minor = MINOR(dev_no);
    pr_info("%s major num = %d -- minor num = %d\n",DEVICE_NAME,w25q_major,w25q_minor);

    /* Create the sysfs class */
    w25q_class = class_create(THIS_MODULE,DEVICE_NAME);
    if(IS_ERR(w25q_class)){
        err = PTR_ERR(w25q_class);
        goto fail;
    }   
    
    /* Allocating memory for device specific data */
    w25q = kmalloc(sizeof(struct w25q_data),GFP_KERNEL);
    if(w25q == NULL){
        pr_err("kmalloc fail\n");
        err = -ENOMEM;
        return err;
    }
    w25q->spi = spi;

    /* Initilize and register character device with kernel */
    cdev_init(&w25q->dev,&w25q_fops);
    w25q->dev.owner = THIS_MODULE;
    err = cdev_add(&w25q->dev,dev_no,1);

    if(err){
        pr_err("Adding cdev fail\n");
        goto fail;
    }

    /* create the device so that user can access the slave device from user space */
    w25q_device = device_create(w25q_class,
                NULL,
                dev_no,
                NULL,
                DEVICE_NAME);

    if(IS_ERR(w25q_device)) {
        err = PTR_ERR(w25q_device);
        pr_err("Device creation fail\n");
        cdev_del(&w25q->dev);
        goto fail;
    }
    
    spi_set_drvdata(spi,w25q);
    return 0;

fail:
   /* if anything fails undo everything */
    if(w25q_class != NULL){
        class_destroy(w25q_class);
    }
    unregister_chrdev_region(MKDEV(w25q_major,w25q_minor),1);
    if(w25q != NULL){
        kfree(w25q);
    }
    return err;


}

static int w25q_remove(struct spi_device *spi)
{
    struct w25q_data *w25q = spi_get_drvdata(spi);
    
    pr_info("%s\n",__func__);
    
    device_destroy(w25q_class,MKDEV(w25q_major,w25q_minor));
    kfree(w25q);
    class_destroy(w25q_class);
    unregister_chrdev_region(MKDEV(w25q_major,w25q_minor),1);
    return 0;
}

static const struct spi_device_id w25q_id[] = {
    { "w25q64" },
    { },
};

MODULE_DEVICE_TABLE(spi,w25q_id);

static const struct of_device_id w25q_of[] = {
    { .compatible = "w25q64" },
    { },
};

MODULE_DEVICE_TABLE(of,w25q_of);

static struct spi_driver w25q_driver = {
    .driver = {
	.owner = THIS_MODULE,
	.name = "w25q64",
	.of_match_table = of_match_ptr(w25q_of),
    },
    .probe = w25q_probe,
    .remove = w25q_remove,
    .id_table = w25q_id,
};

module_spi_driver(w25q_driver);

MODULE_AUTHOR("Jaggu");
MODULE_LICENSE("GPL");

