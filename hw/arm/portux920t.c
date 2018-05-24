/*
 * ARM Taskit Portux920T Emulation
 *
 * Written by Christian René Sechting, Daniel Röhrig
 *
 * Loosely based on the Platform/Application Baseboard emulation
 * written by Paul Brook
 *
 * This code is licensed under the GPL.
 *
 * Warning: This is not a full emulation of the Portux920t!
 *
 */

#include "hw/sysbus.h"
#include "hw/devices.h"
#include "hw/arm/arm.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "net/net.h"

static ARMCPU *cpu;

/*
 * ++++++++++++++++++++++++++++++++++++
 * Memory Controller for the Portux920t
 * ++++++++++++++++++++++++++++++++++++
 *
 * Everything that triggers an operation on the memory
 * regions of the 920T is declared here
 */
static MemoryRegion *ram_alias;

#define TYPE_PORTUX920MC "portux920mc"
#define PORTUX920MC(obj) OBJECT_CHECK(portux920mc_state, (obj), TYPE_PORTUX920MC)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;

    uint32_t mpr;
} portux920mc_state;

static const VMStateDescription vmstate_portux920mc = {
    .name = "at91aic",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(mpr, portux920mc_state),
        VMSTATE_END_OF_LIST()
    }
};
/*
 * Overlay Flash-Memory at address 0x0
 *
 * ctrl == true  ==> toggle
 * ctrl == false ==> disable
 */
static void do_remap(bool ctrl)
{
    if(ctrl){
        if(ram_alias->enabled) {
            memory_region_set_enabled(ram_alias, false);
        } else {
            memory_region_set_enabled(ram_alias, true);
        }
    } else {
        memory_region_set_enabled(ram_alias, false);
    }
}

/*
 * Read-Access (not implented -> return 0 if not a bad offset)
 */
static uint64_t portux920mc_read(void *opaque, hwaddr offset, unsigned size)
{
    portux920mc_state *s = PORTUX920MC(opaque);
    uint32_t asr;

    switch (offset) {
    case 0x4: // Abort Status Register
        asr = cpu->env.asr;
        cpu->env.asr &= ~0x0f000000;
        return asr;
    case 0x8: // Abort Address Status Register
        return cpu->env.aasr;
    case 0xC: // Master Priority Register
        return s->mpr;
    default:
        fprintf(stderr, "portux920mc_read: Bad offset %x (returning zero)\n", (int)offset);
        return 0;
    }
}

/*
 * Write-Access
 */
static void portux920mc_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    portux920mc_state *s = PORTUX920MC(opaque);

    switch (offset) {
    case 0x0: // Remap Control Register
        if(value & 0x1){
            do_remap(true);
        }
        break;
    case 0xC: // Master Priority Register
        s->mpr = value & 0x00007777;
        break;
    default:
        fprintf(stderr, "portux920mc_write: Bad offset %x\n", (int)offset);
    }
}

static void portux920mc_reset(DeviceState *dev) {
    portux920mc_state *s = PORTUX920MC(dev);

    s->mpr = 0x3210;
    cpu->env.asr = 0;
    cpu->env.aasr = 0;
    do_remap(false);
}

/*
 * Functions that will be used when somebody tries
 * to do an operation on the memory regions
 */
static const MemoryRegionOps portux920mc_ops = {
    .read = portux920mc_read,
    .write = portux920mc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/*
 * Initialization of the memory regions and the IRQ
 */
static int portux920mc_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    portux920mc_state *s = PORTUX920MC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &portux920mc_ops, s, "portux920mc", 0x50);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);

    portux920mc_reset(dev);

    return 0;
}

/*
 * ++++++++++++++++++++++++
 * End of Memory Controller
 * ++++++++++++++++++++++++
 */




/*
 * ++++++++++++++++++++
 * Board initialization
 * ++++++++++++++++++++
 * Everything that is needed to initialize the board goes here
 *
 */
static struct arm_boot_info portux920t_binfo;

static void portux920t_init(MachineState *machine)
{
    ram_addr_t ram_size = machine->ram_size;
    const char *cpu_model = machine->cpu_model;
    const char *kernel_filename = machine->kernel_filename;
    const char *kernel_cmdline = machine->kernel_cmdline;
    const char *initrd_filename = machine->initrd_filename;
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1); //1MB Internal Ram
    MemoryRegion *ram2 = g_new(MemoryRegion, 1); //64MB External Ram
    ram_alias = g_new(MemoryRegion, 1); //1MB Ram Alias
    qemu_irq aic[32];
    qemu_irq aic_sys[32];
    DeviceState *dev;

    /* Warning! This is in fact just a copy of the arm926 with a V4T chip set
       instead of a V5! */
    cpu_model = "arm920";
    cpu = cpu_arm_init(cpu_model);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    /* Initialize 1MB RAM and 64MB RAM */
    memory_region_init_ram(ram, NULL, "internal.ram", 0x100000);
    memory_region_init_ram(ram2, NULL, "64MB.ram", 0x4000000); //Memory-Size is fixed at 64M
    //memory_region_init_ram(nand_flash, NULL, "LCD.ram", 0x40); //Cute 64Byte, no not K just B
    vmstate_register_ram_global(ram);
    vmstate_register_ram_global(ram2);
    /* 1MB ram at address zero.  */
    memory_region_add_subregion(sysmem, 0x200000, ram);
    memory_region_set_enabled(ram, true);
    /* ram_alias: Map the 0x200000 Internal RAM and overlay it at 0x0 (disabled at first)*/
    memory_region_init_alias(ram_alias, NULL, "ram.remapped", ram, 0x0, 0x100000);
    memory_region_add_subregion(sysmem,0x0, ram_alias);
    memory_region_set_enabled(ram_alias, true);
    do_remap(false); //Just enable the internal RAM, do not remap
    memory_region_add_subregion(sysmem, 0x20000000, ram2);

    /* Initialize Memory Controller */
    dev = qdev_create(NULL, "portux920mc");
    qdev_init_nofail(dev);
    sysbus_mmio_map((SysBusDevice *)dev, 0, 0xFFFFFF00);



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
     * This is legacy code (as far as we know)
     */
    // Initialize at91emac on interrupt-line 24
    if (nd_table[0].used){
        at91emac_init1(&nd_table[0], 0xFFFBC000, aic[24]);
    }

    /*
     * +++++++++
     * Periphery
     * +++++++++
     */
    sysbus_create_simple("at91dbgu", 0xFFFFF200, aic_sys[0]);
    sysbus_create_simple("at91pio", 0xFFFFF400, NULL);
    sysbus_create_simple("at91st", 0xFFFFFD00, aic_sys[1]);
    sysbus_create_simple("at91usart", 0xFFFC0000, NULL);
    sysbus_create_simple("at91usart", 0xFFFC4000, NULL);
    sysbus_create_simple("at91usart", 0xFFFC8000, NULL);
    sysbus_create_simple("at91usart", 0xFFFCC000, NULL);

    sysbus_create_simple("at91display", 0x40000000, NULL);


    /*
     * +++++++++++++
     * INFO FOR QEMU
     * +++++++++++++
     */
    portux920t_binfo.ram_size = ram_size;
    portux920t_binfo.kernel_filename = kernel_filename;
    portux920t_binfo.kernel_cmdline = kernel_cmdline;
    portux920t_binfo.initrd_filename = initrd_filename;
    portux920t_binfo.board_id = 0x310; //found at arm.linux.org.uk/developer/machines/
    portux920t_binfo.loader_start = 0x20000000; //Start executing at 0x20000000 instead of 0x0

    arm_load_kernel(cpu, &portux920t_binfo);
}

static QEMUMachine portux920t_machine = {
    .name = "portux920t",
    .desc = "ARM Taskit Portux920t (ARM920)",
    .init = portux920t_init,
};


/* Register the machine */
static void portux920t_machine_init(void)
{
    qemu_register_machine(&portux920t_machine);
}

machine_init(portux920t_machine_init);


/* Initialize Memory Controller Class */
static void portux920mc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = portux920mc_init;
    dc->reset = portux920mc_reset;
    dc->vmsd = &vmstate_portux920mc;
}

static const TypeInfo portux920mc_info = {
    .name          = TYPE_PORTUX920MC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(portux920mc_state),
    .class_init    = portux920mc_class_init,
};

/* Register machine */
static void portux920t_register_types(void)
{
    type_register_static(&portux920mc_info);
}

type_init(portux920t_register_types)
