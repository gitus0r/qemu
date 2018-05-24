/*
 *
 *    ARM taskit PortuxG20
 *
 */


#include "hw/sysbus.h"
#include "hw/arm/arm.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "net/net.h"
#include "hw/devices.h"

/* Board init.  */
static struct arm_boot_info portuxg20_binfo;

//static MemoryRegion *ram_alias;

static void portuxg20_init(MachineState *machine)
{
    ram_addr_t ram_size = machine->ram_size;
    const char *cpu_model = machine->cpu_model;
    const char *kernel_filename = machine->kernel_filename;
    const char *kernel_cmdline = machine->kernel_cmdline;
    const char *initrd_filename = machine->initrd_filename;
    ARMCPU *cpu;
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *sram0 = g_new(MemoryRegion, 1); //internal 16kB Ram
    MemoryRegion *sram1 = g_new(MemoryRegion, 1); //internal 16kB Ram
    MemoryRegion *sdram = g_new(MemoryRegion, 1); //sdram auf 0x20000000
    qemu_irq aic[32];
    qemu_irq aic_sys[32];
    DeviceState *dev;

    cpu_model = "arm926";
    cpu = cpu_arm_init(cpu_model);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    /* Initialize RAM */
    memory_region_init_ram(sram0, NULL, "Internal SRAM0", 0x4000); // 16kB sram0
    memory_region_init_ram(sram1, NULL, "Internal SRAM1", 0x4000); // 16kB sram1
    memory_region_init_ram(sdram, NULL, "SDRAM", 0x8000000);         // 128MB sdram
    vmstate_register_ram_global(sram0);
    vmstate_register_ram_global(sram1);
    vmstate_register_ram_global(sdram);
    memory_region_add_subregion(sysmem,0x200000, sram0);
    memory_region_add_subregion(sysmem,0x300000, sram1);
    memory_region_add_subregion(sysmem,0x20000000, sdram);

    /*
     * +++++++++++
     * IRQ and AIC
     * +++++++++++
     * CPU-interrupt and advanced interrupt controller setup
     */

    /* Create AIC and connect to CPU */
    dev = sysbus_create_varargs("at91aic", 0xFFFFF000,
	    qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ),
	    qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_FIQ),
	    NULL);

    /* Set up 32 hardware interrupt connections for AIC */
    int n;
    for (n = 0; n < 32; n++) {
        aic[n] = qdev_get_gpio_in(dev, n);
    }

    /* Create OR-Logic for interrupts of SYS devices */
    dev = sysbus_create_simple("at91_intor", -1, aic[1]);
    for (n = 0; n < 32; n++) {
        aic_sys[n] = qdev_get_gpio_in(dev, n);
    }

    /*
     * ++++++++++++++++++
     * Ethernet EMAC Card
     * ++++++++++++++++++
     */
    if (nd_table[0].used){
        at91g20emac_init1(&nd_table[0], 0xFFFC4000, aic[21]);
    }

    /* dbgu, usarts, pio's und systemtimer */
    sysbus_create_simple("at91dbgu", 0xFFFFF200, aic_sys[0]);
    sysbus_create_simple("at91pio", 0xFFFFF400, NULL);
    sysbus_create_simple("at91pio", 0xFFFFF600, NULL);
    sysbus_create_simple("at91pio", 0xFFFFF800, NULL);
    sysbus_create_simple("at91g20st", 0xFFFFFD20, aic_sys[1]); //RealTime, Periodic, Watchdog
    sysbus_create_simple("at91usart", 0xFFFB0000, NULL);
    sysbus_create_simple("at91usart", 0xFFFB4000, NULL);
    sysbus_create_simple("at91usart", 0xFFFB8000, NULL);
    sysbus_create_simple("at91usart", 0xFFFD0000, NULL);
    sysbus_create_simple("at91usart", 0xFFFD4000, NULL);
    sysbus_create_simple("at91usart", 0xFFFD8000, NULL);


    portuxg20_binfo.ram_size = ram_size;
    portuxg20_binfo.kernel_filename = kernel_filename;
    portuxg20_binfo.kernel_cmdline = kernel_cmdline;
    portuxg20_binfo.initrd_filename = initrd_filename;
    portuxg20_binfo.board_id = 0x88F; //auf seite arm.linux.org.uk/developer/machines/ zu finden
    portuxg20_binfo.loader_start = 0x20000000; //Start executing at 0x20000000 instead of 0x0
    arm_load_kernel(cpu, &portuxg20_binfo);
}


static QEMUMachine portuxg20_machine = {
    .name = "portuxg20",
    .desc = "ARM Taskit PortuxG20 (ARM926EJ-S)",
    .init = portuxg20_init,
};



static void portuxg20_machine_init(void)
{
    qemu_register_machine(&portuxg20_machine);
}

machine_init(portuxg20_machine_init);
