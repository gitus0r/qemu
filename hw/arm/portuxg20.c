/*
 *
 *    ARM taskit PortuxG20
 *
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "migration/vmstate.h"
#include "hw/sysbus.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "net/net.h"
#include "hw/net/at91g20emac.h"

/* Board init.  */
static struct arm_boot_info portuxg20_binfo;

//static MemoryRegion *ram_alias;

static void portuxg20_init(MachineState *machine)
{
    Object *cpuobj;
    ram_addr_t ram_size = machine->ram_size;
    ARMCPU *cpu;
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *sram0 = g_new(MemoryRegion, 1); //internal 16kB Ram
    MemoryRegion *sram1 = g_new(MemoryRegion, 1); //internal 16kB Ram
    MemoryRegion *sdram = g_new(MemoryRegion, 1); //sdram auf 0x20000000
    qemu_irq aic[32];
    qemu_irq aic_sys[32];
    DeviceState *dev;

    cpuobj = object_new(machine->cpu_type);

    cpu = ARM_CPU(cpuobj);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    /* Initialize RAM */
    memory_region_init_ram(sram0, NULL, "Internal SRAM0", 0x4000, NULL); // 16kB sram0
    memory_region_init_ram(sram1, NULL, "Internal SRAM1", 0x4000, NULL); // 16kB sram1
    memory_region_init_ram(sdram, NULL, "SDRAM", 0x8000000, NULL);         // 128MB sdram
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
    portuxg20_binfo.board_id = 0x88F; //auf seite arm.linux.org.uk/developer/machines/ zu finden
    portuxg20_binfo.loader_start = 0x20000000; //Start executing at 0x20000000 instead of 0x0
    arm_load_kernel(cpu, machine, &portuxg20_binfo);
}

static void portuxg20_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    
    mc->desc = "ARM Taskit PortuxG20 (ARM926EJ-S)";
    mc->init = portuxg20_init;
    //mc->block_default_type = IF_SCSI; XXX ?
    //mc->ignore_memory_transaction_failures = true;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("arm920");
}

static const TypeInfo portuxg20_type = {
    .name = MACHINE_TYPE_NAME("portuxg20"),
    .parent = TYPE_MACHINE,
    .class_init = portuxg20_class_init,
};

static void portuxg20_machine_init(void)
{
    type_register_static(&portuxg20_type);
}

type_init(portuxg20_machine_init);
