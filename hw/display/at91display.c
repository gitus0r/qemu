/**
 * Portux920T display a,b and c
 */
#include "hw/sysbus.h"

#define TYPE_AT91DISPLAY "at91display"
#define AT91DISPLAY(obj) OBJECT_CHECK(display_state, (obj), TYPE_AT91DISPLAY)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    uint32_t charreg;
} display_state;

static uint64_t display_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0;
}

static void display_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    display_state *s = (display_state *)opaque;

    //TODO: Don't write if Chipselect is LOW?
    /*printf("\nW:%c",(char)value);
    fflush(0);*/
    switch (offset) {
        case 0: //display enable register
            s->charreg = value; //der value wird ins display status register eingetragen
            unsigned int buf[1];
            buf[0]=value;
            cpu_physical_memory_write(0xfffff500,buf,1);
            break;
        default:
            fprintf(stderr, "display A Write: Bad offset %x\n", (int)offset);
    }
}


static const MemoryRegionOps display_ops = {
    .read = display_read,
    .write = display_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_display = {
    .name = "at91display",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32(charreg, display_state),
        VMSTATE_END_OF_LIST()
    }
};

static int display_init(SysBusDevice *dev)
{
    display_state *s = AT91DISPLAY(dev);
    memory_region_init_io(&s->iomem, OBJECT(s), &display_ops, s, "at91display", 0x40);
    sysbus_init_mmio(dev, &s->iomem);
    return 0;
}

static void display_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);
    sdc->init = display_init;
}

static const TypeInfo display_info = {
    .name          = TYPE_AT91DISPLAY,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(display_state),
    .class_init    = display_class_init,
};

static void display_register_types(void)
{
    //Achtung! Hier können auch mehrere verschiedene TypeInfos angegeben werden!
    type_register_static(&display_info);
}



type_init(display_register_types)
