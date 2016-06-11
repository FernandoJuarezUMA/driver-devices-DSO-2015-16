/*
 *	Fernando Adrian Juarez Mellado
 *	Grado de Computadores
 *	Diseño de Sistemas Operativos
 */
 
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h> // misc dev
#include <linux/fs.h>         // file operations
#include <asm/uaccess.h>      // copy to/from user space
#include <linux/wait.h>       // waiting queue
#include <linux/sched.h>      // TASK_INTERRUMPIBLE
#include <linux/delay.h>      // udelay
#include <linux/timer.h>   
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/semaphore.h>  // semaphore para seccion critica

#define TAM_MAX_BUFFER 200	  //Tamaño del buffer para las pulsaciones

#define DRIVER_AUTHOR "Fernando Adrian Juarez Mellado"
#define DRIVER_DESC   "Driver de 3 Devices"

//GPIOS numbers as in BCM RPi
//Pulsadores
#define GPIO_BUTTON1 2
#define GPIO_BUTTON2 3
//Speaker
#define GPIO_SPEAKER 4
//LEDS
#define GPIO_GREEN1  27
#define GPIO_GREEN2  22
#define GPIO_YELLOW1 17
#define GPIO_YELLOW2 11
#define GPIO_RED1    10
#define GPIO_RED2    9

//Cadenas de descripcion de los botones y el speaker
#define GPIO_BUTTON1_DESC           "BOTON 1"
#define GPIO_BUTTON2_DESC           "BOTON 2"
#define GPIO_SPEAKER_DESC           "SPEAKER"

#define GPIO_BUTTON1_DEVICE_DESC    "Placa Auxiliar.Boton 1"
#define GPIO_BUTTON2_DEVICE_DESC    "Placa Auxiliar.Boton 2"

//Utilizacion de un semaforo para regiones criticas
DEFINE_SEMAPHORE(semaforo);

//Variable para controlar la apertura del fichero
static int Device_Open = 0;

//Array para almacenar las pulsaciones
static char BUFFER[TAM_MAX_BUFFER];
static int indice_Buffer=0;
static int contador=0;

//LEDS y sus descripciones
static int LED_GPIOS[]= {GPIO_GREEN1, GPIO_GREEN2, GPIO_YELLOW1, GPIO_YELLOW2, GPIO_RED1, GPIO_RED2} ;
static char *led_desc[]= {"GPIO_GREEN1","GPIO_GREEN2","GPIO_YELLOW1","GPIO_YELLOW2","GPIO_RED1","GPIO_RED2"} ;

//variables para los timers
static int rebotetmp = 900;
static int rebotesticks;

//Cola y variable para la cola
static DECLARE_WAIT_QUEUE_HEAD(cola);
static int flag = 0;

//Se puede modificar la variable en el momento de carga
module_param(rebotetmp, int, S_IRUGO);

/*****************************************************/
/*   TASKLETS y TIMERS								 */
/*****************************************************/

static void tasklet_boton1(unsigned long n);
static void tasklet_boton2(unsigned long n);
DECLARE_TASKLET(tasklet1, tasklet_boton1,0);
DECLARE_TASKLET(tasklet2, tasklet_boton2,0);

static void funcion_timer1(unsigned long n);
DEFINE_TIMER(rebote1, funcion_timer1, 0, 0);
static void funcion_timer2(unsigned long n);
DEFINE_TIMER(rebote2, funcion_timer2,0,0);

/*****************************************************/
/*   Interrupciones habilitadas						 */
/*****************************************************/
static short int irq_BUTTON1 = 0;
static short int irq_BUTTON2 = 0;

static void funcion_timer1(unsigned long n){
	enable_irq(irq_BUTTON1);
}
static void funcion_timer2(unsigned long n){
	enable_irq(irq_BUTTON2);
}

/****************************************************************************/
/* PRIMER DEVICE "leds"                            							*/
/****************************************************************************/

//Evaluo el byte introducido
static void byte2leds(char ch)
{
    int i,b7,b6,aux;
    int val = (int)ch;
	
	// Tomo el valor del bit 6
	b6 = (val>>6 & 1);
	
	// Tomo el valor del bit 7
	b7 = (val>>7 & 1);

	
	//Una vez tenga los valores de ambos bits se procede
	
	//Esos mismos valores tendran los leds
	if((b6==0) && (b7==0))
	{
		 for(i=0; i<6; i++) gpio_set_value(LED_GPIOS[i], (val >> i) & 1);
	
	}//Solo se activan los bits que estan a 1,los demas siguen igual
	else if((b6==1) && (b7==0))
	{
		for(i=0; i<6; i++)
		{
			aux=(val >> i) & 1;
			//
			if(aux==1)
			{
				gpio_set_value(LED_GPIOS[i], 1);
			}
		}
	}//Solo se apagan los bits que estan a 1,los demas siguen igual
	else if((b6==0) && (b7==1))
	{
		for(i=0; i<6; i++)
		{
			aux=(val >> i) & 1;
			if(aux==1)
			{
				gpio_set_value(LED_GPIOS[i], 0);
			}
		}
	}//Estado indefinido,apago todos
	else
	{
		for(i=0; i<6; i++) gpio_set_value(LED_GPIOS[i], 0);
	
	}

}

//Se leen los leds
static char leds2byte(void)
{
    int val;
    char ch;
    int i;
    ch=0;

    for(i=0; i<6; i++)
    {
		//valor de cada led
        val=gpio_get_value(LED_GPIOS[i]);
        ch= ch | (val << i);
    }
    return ch;
}


/****************************************************************************/
/* LEDs device file operations                                              */
/****************************************************************************/

static ssize_t leds_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{

    char ch;

    if (copy_from_user( &ch, buf, 1 )) {
        return -EFAULT;
    }

    printk( KERN_INFO " (write) valor recibido: %d\n",(int)ch);

    byte2leds(ch);

    return 1;
}

static ssize_t leds_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    char ch;

    if(*ppos==0) *ppos+=1;
    else return 0;

    ch=leds2byte();

    printk( KERN_INFO " (read) valor entregado: %d\n",(int)ch);


    if(copy_to_user(buf,&ch,1)) return -EFAULT;
    return 1;
}

static const struct file_operations leds_fops = {
    .owner	= THIS_MODULE,
    .write	= leds_write,
    .read	= leds_read,
};

// LEDs device struct
static struct miscdevice leds_miscdev = {
    .minor	= MISC_DYNAMIC_MINOR,
    .name	= "leds",
    .fops	= &leds_fops,
};


/****************************************************************************/
/* SEGUNDO DEVICE "speaker"                            						*/
/****************************************************************************/

//Evaluo el byte introducido
static void byte2speaker(char ch)
{
    int i;
    int val=(int)ch;
	
	
	val=val & 1;
	// Si val=1 activo el speaker
	if(val==1)
	{
		gpio_set_value(GPIO_SPEAKER,1);
	}
	// Si val=0 desactivo el speaker
	else
	{
		gpio_set_value(GPIO_SPEAKER,0);
	}

}

/****************************************************************************/
/* SPEAKER device file operations                                           */
/****************************************************************************/

static ssize_t speaker_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{

    char ch;

    if (copy_from_user( &ch, buf, 1 )) {
        return -EFAULT;
    }

    printk( KERN_INFO " (write) valor recibido: %d\n",(int)ch);

    byte2speaker(ch);

    return 1;
}

static const struct file_operations speaker_fops = {
    .owner	= THIS_MODULE,
    .write	= speaker_write,
};

/****************************************************************************/
/* SPEAKER device struct                       								*/
/****************************************************************************/

static struct miscdevice speaker_miscdev = {
    .minor	= MISC_DYNAMIC_MINOR,
    .name	= "speaker",
    .fops	= &speaker_fops,
};


/****************************************************************************/
/* TERCER DEVICE "buttons"                            						*/
/****************************************************************************/

/****************************************************************************/
/* BUTTONS device file operations                                           */
/****************************************************************************/

static ssize_t buttons_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
	//Se duerme si flag=0,si el bloqueo devuelve un valor distinto de cero
	//se lanza el error ERESTARTSYS
	if((wait_event_interruptible(cola, flag != 0)) != 0) return -ERESTARTSYS;;
	
	if(down_interruptible(&semaforo)) return -ERESTARTSYS;
	//Inicio Seccion Critica
	
	if(copy_to_user(buf,&BUFFER[indice_Buffer],1)) return -EFAULT;
	
	indice_Buffer++;
	
	if(BUFFER[indice_Buffer] == '\0' ) flag=0;
	
	up(&semaforo);
	//Fin Seccion Critica
	
	*ppos+=1;
	
	return 1;
}

static int buttons_open(struct inode *inode, struct file *file)
{
	 if (down_interruptible(&semaforo)) return -ERESTARTSYS;
	 //Inicio Seccion Critica
	
	//Comprueba si ya esta abierto
	if (Device_Open){
		up(&semaforo);
		//Fin Seccion Critica
		return -EBUSY;	
    }
	printk ("Device_open(%p,%p)\n", inode, file);
	
	Device_Open++;
		
	up(&semaforo);
	//Fin Seccion Critica
	
	return 0;
}

static int buttons_release(struct inode *inode, struct file *file)
{
	if (down_interruptible(&semaforo)) return -ERESTARTSYS;
	 //Inicio Seccion Critica
	
	//Cerrando el fichero y comunicandolo
	printk ("device_release(%p,%p)\n", inode, file);
	
	Device_Open --;
	
	up(&semaforo);
	//Fin Seccion Critica
	
	return 0;
}

/*******************************************************************/
/*   Funciones de los TASKLETS	(BOTTOM-HALF)					   */
/*******************************************************************/
static void guardarPulsacion(char n){
	down(&semaforo);
	//Inicio Seccion Critica
	//Hay espacio todavia
	if(contador<(TAM_MAX_BUFFER-1)){
		BUFFER[contador]=n;
		BUFFER[contador+1]='\0';
		contador++;
	}else{
		//Buffer lleno
		printk(KERN_NOTICE "Buffer lleno\n");

	}
	
	up(&semaforo);
	//Fin Seccion Critica
	
	flag = 1;
	//Despierto los procesos dormidos
    wake_up_interruptible(&cola);
	
}

// Añadiendo '1'
static void tasklet_boton1(unsigned long n){
	guardarPulsacion('1');
}

// Añadiendo '2'
static void tasklet_boton2(unsigned long n){
	guardarPulsacion('2');
}


/****************************************************************/
/* IRQ handler - fired on interrupt (TOP-HALF)                  */
/****************************************************************/
static irqreturn_t r_irq_handler1(int irq, void *dev_id, struct pt_regs *regs) {
    // we will increment value in leds with button push
    // due to switch bouncing this hadler will be fired few times for every putton push

	disable_irq_nosync(irq_BUTTON1);
	mod_timer(&rebote1, jiffies+rebotesticks);
	//BOTTOM-HALF
	tasklet_schedule(&tasklet1);
	
    return IRQ_HANDLED;
}

static irqreturn_t r_irq_handler2(int irq, void *dev_id, struct pt_regs *regs) {
    // we will decrement value in leds with button push
    // due to switch bouncing this hadler will be fired few times for every putton push

    disable_irq_nosync(irq_BUTTON2);
	mod_timer(&rebote2, jiffies+rebotesticks);
	//BOTTOM-HALF
	tasklet_schedule(&tasklet2);
	
    return IRQ_HANDLED;
}

static const struct file_operations buttons_fops = {
    .owner	= THIS_MODULE,
    .read	= buttons_read,
	.open   = buttons_open,
	.release= buttons_release,
};

/****************************************************************************/
/* BUTTONS device struct                       								*/
/****************************************************************************/

static struct miscdevice buttons_miscdev = {
    .minor	= MISC_DYNAMIC_MINOR,
    .name	= "buttons",
    .fops	= &buttons_fops,
};


/*****************************************************************************/
/* This functions registers devices, requests GPIOs and configures interrupts*/
/*****************************************************************************/

/************************************************************
 *  register device for leds,speaker & buttons
 ***********************************************************/

static void r_dev_config(void)
{
   int ret;
   //LEDS
    ret = misc_register(&leds_miscdev);
    if (ret < 0) {
        printk(KERN_ERR "misc_register failed\n");
        return;
    }
    printk(KERN_NOTICE "  leds_miscdev.minor   =%d\n", leds_miscdev.minor);
    
	//SPEAKER
    ret = misc_register(&speaker_miscdev);
    if (ret < 0) {
        printk(KERN_ERR "misc_register failed\n");
        return;
    }
    printk(KERN_NOTICE "  speaker_miscdev.minor   =%d\n", speaker_miscdev.minor);

	//BUTTONS
    ret = misc_register(&buttons_miscdev);
	if (ret < 0) {
        printk(KERN_ERR "misc_register failed\n");
        return;
    }
    printk(KERN_NOTICE "  buttons_miscdev.minor   =%d\n", buttons_miscdev.minor);


}

/************************************************************
 *  request and init gpios for leds
 ***********************************************************/

static void r_GPIO_config(void)
{
    int i;
    for(i=0; i<6; i++)
    {
        if (gpio_request_one(LED_GPIOS[i], GPIOF_INIT_LOW, led_desc[i])) 
        {
            printk("GPIO request failure: led GPIO %d %s\n",LED_GPIOS[i], led_desc[i]);
            return ;
        }
        gpio_direction_output(LED_GPIOS[i],0);
	}
}

/************************************************************
 *  request and init gpio for speaker
 ***********************************************************/
static void r_SPEAKER_config(void)
{
	 //speaker
    if (gpio_request(GPIO_SPEAKER, GPIO_SPEAKER_DESC)) {
        printk("GPIO request failure: %s\n", GPIO_SPEAKER_DESC);
        return;
    }
    
	gpio_direction_output(GPIO_SPEAKER,1);
	
}

/************************************************************
 *  set interrup for button 1 y 2
 ***********************************************************/

static void r_int_config(void)
{
	//Configuracion Boton1
    if (gpio_request(GPIO_BUTTON1, GPIO_BUTTON1_DESC)) {
        printk("GPIO request failure: %s\n", GPIO_BUTTON1_DESC);
        return;
    }

    if ( (irq_BUTTON1 = gpio_to_irq(GPIO_BUTTON1)) < 0 ) {
        printk("GPIO to IRQ mapping failure %s\n", GPIO_BUTTON1_DESC);
        return;
    }

    printk(KERN_NOTICE "  Mapped int %d for button1 in gpio %d\n", irq_BUTTON1, GPIO_BUTTON1);

    if (request_irq(irq_BUTTON1,
                    (irq_handler_t ) r_irq_handler1,
                    IRQF_TRIGGER_FALLING,
                    GPIO_BUTTON1_DESC,
                    GPIO_BUTTON1_DEVICE_DESC)) {
        printk("Irq Request failure\n");
        return;
    }
	//Configuracion Boton2
	  if (gpio_request(GPIO_BUTTON2, GPIO_BUTTON2_DESC)) {
        printk("GPIO request failure: %s\n", GPIO_BUTTON2_DESC);
        return;
    }

    if ( (irq_BUTTON2 = gpio_to_irq(GPIO_BUTTON2)) < 0 ) {
        printk("GPIO to IRQ mapping failure %s\n", GPIO_BUTTON2_DESC);
        return;
    }

    printk(KERN_NOTICE "  Mapped int %d for button2 in gpio %d\n", irq_BUTTON2, GPIO_BUTTON2);

    if (request_irq(irq_BUTTON2,
                    (irq_handler_t ) r_irq_handler2,
                    IRQF_TRIGGER_FALLING,
                    GPIO_BUTTON2_DESC,
                    GPIO_BUTTON2_DEVICE_DESC)) {
        printk("Irq Request failure\n");
        return;
    }
    
    return;
}

/****************************************************************************/
/* Module init / cleanup block.                                             */
/****************************************************************************/
static int r_init(void) {
	printk(KERN_NOTICE "Hi, loading %s module!\n", KBUILD_MODNAME);
    printk(KERN_NOTICE "%s - devices config\n", KBUILD_MODNAME);
    r_dev_config();
    printk(KERN_NOTICE "%s - GPIO config\n", KBUILD_MODNAME);
    r_GPIO_config();
    r_SPEAKER_config();
    r_int_config();
	rebotesticks=msecs_to_jiffies(rebotetmp);
    return 0;
}

static void r_cleanup(void) {
    int i;
    printk(KERN_NOTICE "%s module cleaning up...\n", KBUILD_MODNAME);
	//Libero todos los recursos utilizados
	//leds
    for(i=0; i<6; i++)
    {
        gpio_set_value(LED_GPIOS[i], 0);
        gpio_free(LED_GPIOS[i]);
    }
    //borra el registro del device "leds"
    if (leds_miscdev.this_device) misc_deregister(&leds_miscdev);
	//borra el registro del device "speaker"
    if (speaker_miscdev.this_device) misc_deregister(&speaker_miscdev);
	//borra el registro del device "buttons"
    if (buttons_miscdev.this_device) misc_deregister(&buttons_miscdev);
   //Boton 1
    if(irq_BUTTON1) free_irq(irq_BUTTON1, GPIO_BUTTON1_DEVICE_DESC);
    gpio_free(GPIO_BUTTON1);
    //Boton 2
    if(irq_BUTTON2) free_irq(irq_BUTTON2, GPIO_BUTTON2_DEVICE_DESC);
    gpio_free(GPIO_BUTTON2);
     //Speaker
    gpio_free(GPIO_SPEAKER);
	
	//Timers
	del_timer(&rebote1);
	del_timer(&rebote2);
	//Tasklets
	tasklet_kill(&tasklet_boton1);
	tasklet_kill(&tasklet_boton2);
	
	printk(KERN_NOTICE "Done. Bye from %s module\n", KBUILD_MODNAME);
	
    return;
}

module_init(r_init);
module_exit(r_cleanup);

/****************************************************************************/
/* Module licensing/description block.                                      */
/****************************************************************************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
