#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/kobject.h>
#define GPIO_LED 20

#define DEV_MEM_SIZE 512

uint8_t* device_buffer;
dev_t device_number;
struct cdev ccdev;
static struct class* ccdev_class;
struct file_operations fops_ccdev;

static unsigned int irqNumber;

int number_press=0;
bool led_on = 0;
static char name_dev[] = "LKM_GPIO_DEV";

bool gpio_input_init(int gpio_num);
bool gpio_output_init(int gpio_num);


bool isRising = 0;
module_param(isRising, bool, S_IRUGO);
MODULE_PARM_DESC(isRising, "Rising edge = 1 (default), Falling edge = 0");

int gpio_button=16;
module_param(gpio_button, int, S_IRUGO);
MODULE_PARM_DESC(gpio_button, "Button GPIO number");

int gpio_led=20;
module_param(gpio_led, int, S_IRUGO);
MODULE_PARM_DESC(gpio_led, "Led GPIO number");

/*
 * Callback return number of button click
 */
static ssize_t number_press_show(struct kobject* kobj, struct kobj_attribute* attr, char* buff){
	return(sprintf(buff, "%d\n", number_press));
}

/*
 * Callback update number of button click 
 */
static ssize_t number_press_store(struct kobject* kobj, struct kobj_attribute* attr, const char* buff, size_t count){
	sscanf(buff, "%du", &number_press);
	return count;
}

static struct kobj_attribute number_press_attr=__ATTR(number_press, 0664, number_press_show, number_press_store);

static struct attribute* gpio_dev_attr[] = {
	&number_press_attr.attr,
	NULL
};

static struct attribute_group attr_group = {
	.name = name_dev,
	.attrs = gpio_dev_attr,
};

static struct kobject* kobj_gpio_dev;

/*
 *Callback on open request 
 */
static int ccdev_open (struct inode* inod, struct file* fil){
	printk(KERN_INFO "Device opened...:)\n");
	device_buffer = kmalloc(DEV_MEM_SIZE, GFP_KERNEL);
	if(!device_buffer){
		printk(KERN_INFO "Can not allocate kernel memory...\n");
		return -1;
	}
	return 0;
}

/*
 *Callback on close request 
 */
static int ccdev_release(struct inode* inod, struct file* fil){
	kfree(device_buffer);
	printk(KERN_INFO "Device FILE closed...\n");
	return 0;
}

/*
 *Callback on read request 
 */
static ssize_t ccdev_read(struct file *fil, char __user *buf, size_t len, loff_t* off){
	char* str = "Hello world back string";
	copy_to_user(buf, str, 24);
	printk(KERN_INFO "Buffer copied to user...%s\n", str); 
	return 24;
}


/*
 *Callback on write request 
 */
static ssize_t ccdev_write(struct file* fil, const char* buff, size_t size, loff_t* off){
	memset(device_buffer,'\0', DEV_MEM_SIZE);
	copy_from_user(device_buffer, buff, size);
	printk(KERN_INFO "Buffer copied from user...res....%s\n", device_buffer);
	return size;
}

/*
 * Struct contain callbacks pointers 
 */
struct file_operations fops_ccdev = {
	.read = ccdev_read,
	.write = ccdev_write,
	.open = ccdev_open,
	.owner = THIS_MODULE,
	.release = ccdev_release
};

/*
 * Callback when dev create 
 */
static int ccdev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

/*
 * IRQ handler
 */
static irq_handler_t ebbgpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){
   led_on = !led_on;                          // Invert the LED state on each button press
   gpio_set_value(gpio_led, led_on);          // Set the physical LED accordingly
   printk(KERN_INFO "GPIO_TEST: Interrupt! (button state is %d)\n", gpio_get_value(gpio_button));
   number_press++;                         // Global counter, will be outputted when the module is unloaded
   return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly
}

/*
 * Function: init GPIO as Input
 */
bool gpio_input_init(int gpio_num){
	int result = 0;


	kobj_gpio_dev = kobject_create_and_add("gpio_dev", kernel_kobj->parent);
	if(!kobj_gpio_dev){
		printk(KERN_INFO "Error create kobject interface\n");
		return -ENOMEM;
	}

	result = sysfs_create_group(kobj_gpio_dev, &attr_group);
	if(result) {
		printk(KERN_ALERT "EBB Button: failed to create sysfs group\n");
		kobject_put(kobj_gpio_dev);
		return result;
	}

	if(gpio_is_valid(gpio_num)){
		printk(KERN_INFO "gpio input is valid");
	}
	else{
		printk(KERN_INFO "gpio input is unvalid");
	}
	
	gpio_request(gpio_num, "sysfs");
	gpio_direction_input(gpio_num);
	
	result = gpio_set_debounce(gpio_num, 200);  // unsupported in some kernels
	printk(KERN_INFO "gpio set debounce %d", result);
	
	gpio_export(gpio_num, false);

	printk(KERN_INFO "GPIO_TEST: The button state is currently: %d\n", gpio_get_value(gpio_button));

	irqNumber = gpio_to_irq(gpio_button);
	   // This next call requests an interrupt line
	result = request_irq(irqNumber,             // The interrupt number requested
        	(irq_handler_t) ebbgpio_irq_handler, // The pointer to the handler function below
        	IRQF_TRIGGER_RISING,   // Interrupt on rising edge (button press, not release)
        	"ebb_gpio_handler",    // Used in /proc/interrupts to identify the owner
        	NULL);                 // The *dev_id for shared interrupt lines, NULL is okay

	printk(KERN_INFO "GPIO_TEST: The interrupt request result is: %d\n", result);
	return result;
}

/*
 * Function: init GPIO as Output
 */
bool gpio_output_init(int gpio_num){
	/*Init gpio 20 led */

	//check is gpio valid
	if(gpio_is_valid(gpio_num)){
		printk(KERN_INFO "GPIO 20 valid\n");
	}
	else {
		printk(KERN_INFO "GPIO 20 is NOT valid\n");
	}

	//allocation gpio
	if(gpio_request(gpio_num, "gpio20") == 0){
		printk(KERN_INFO "gpio 20 allocated\n");
	}
	else {
		printk(KERN_INFO "gpio 20 NOT allocated\n");
	}

	if(gpio_export(gpio_num, true) == 0){
		printk(KERN_INFO "gpio 20 exported\n");
	}
	else{
		printk(KERN_INFO "gpio 20 NOT exported\n");
	}

	if(gpio_direction_output(gpio_num, 1) == 0){
		printk(KERN_INFO "GPIO 20 turn on success\n");
	}
	else{
		printk(KERN_INFO "GPIO 20 turn on unsuccess\n");
	}

	return 1;
}


/*
 * Callback: Driver entry point, handle init process
 */
static int __init ccdev_init(void){

	printk(KERN_INFO "ccdev initialization...\n");

	if((alloc_chrdev_region(&device_number,0,1,"chrdev")) < 0){
		printk(KERN_INFO "Can not allocate major number\n");
		return -1;
	}

	printk(KERN_INFO "Major %d, Minor %d...\n", MAJOR(device_number), MINOR(device_number));

	cdev_init(&ccdev, &fops_ccdev);

	ccdev.owner = THIS_MODULE;
	if(cdev_add(&ccdev, device_number, 1) < 0){
		printk(KERN_INFO "Can not add device to the system...\n");
		goto ur_class;
	}

	if((ccdev_class = class_create(THIS_MODULE, "ccdev_class")) == NULL){
		printk(KERN_INFO "Can not create ccdev_class...\n");
		goto ur_class;
	}
	ccdev_class->dev_uevent = ccdev_uevent;

	if(device_create(ccdev_class, NULL, device_number, NULL, "ccdev") == NULL){
		printk(KERN_INFO "Can not create the device device...\n");
		goto ur_device;
	}

	printk(KERN_INFO "Device driver insert...done properly...\n");

	gpio_output_init(gpio_led);
	gpio_input_init(gpio_button);


	return 0;

ur_device:
	class_destroy(ccdev_class);

ur_class:
	unregister_chrdev_region(device_number, 1);
	return -1;
}
module_init(ccdev_init);


/*
 * Callback: exit point, handle freeing
 */
static void __exit ccdev_exit(void){
	device_destroy(ccdev_class, device_number);
	class_destroy(ccdev_class);
	cdev_del(&ccdev);
	unregister_chrdev_region(device_number,1);

   	printk(KERN_INFO "GPIO_TEST: The button state is currently: %d\n", gpio_get_value(gpio_button));
   	printk(KERN_INFO "GPIO_TEST: The button was pressed %d times\n", number_press);
   	gpio_set_value(gpio_led, 0);              // Turn the LED off, makes it clear the device was unloaded
   
	gpio_unexport(gpio_led);                  // Unexport the LED GPIO
   	free_irq(irqNumber, NULL);               // Free the IRQ number, no *dev_id required in this case
   	gpio_unexport(gpio_button);               // Unexport the Button GPIO
   	
	gpio_free(gpio_led);                      // Free the LED GPIO
   	gpio_free(gpio_button);                   // Free the Button GPIO
	
	printk(KERN_INFO "gpio dev removed.\n");
}
module_exit(ccdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yurii");
MODULE_DESCRIPTION("GPIO control device");

