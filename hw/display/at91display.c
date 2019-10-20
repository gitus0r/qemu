/**
 * Portux920T display a,b and c
 */

#include "qemu/osdep.h"
#include "migration/vmstate.h"
#include "hw/sysbus.h"

#define TYPE_AT91DISPLAY "at91display"
#define AT91DISPLAY(obj) OBJECT_CHECK(at91display_state, (obj), TYPE_AT91DISPLAY)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    uint32_t charreg;
} at91display_state;

static uint64_t at91display_read(void *opaque, hwaddr offset, unsigned size)
{
    return 0;
}

static void at91display_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    at91display_state *s = (at91display_state *)opaque;

    //TODO: Don't write if Chipselect is LOW?
    /*printf("\nW:%c",(char)value);
    fflush(0);*/
    switch (offset) {
        case 0: //at91display enable register
            s->charreg = value; //der value wird ins at91display status register eingetragen
            unsigned int buf[1];
            buf[0]=value;
            cpu_physical_memory_write(0xfffff500,buf,1);
            break;
        default:
            fprintf(stderr, "at91display A Write: Bad offset %x\n", (int)offset);
    }
}


static const MemoryRegionOps at91display_ops = {
    .read = at91display_read,
    .write = at91display_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_at91display = {
    .name = "at91display",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32(charreg, at91display_state),
        VMSTATE_END_OF_LIST()
    }
};

static void at91display_init(Object *obj)
{
    at91display_state *s = AT91DISPLAY(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    
    memory_region_init_io(&s->iomem, obj, &at91display_ops, s, "at91display", 0x40);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void at91display_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    
    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
    dc->vmsd = &vmstate_at91display;
}

static const TypeInfo at91display_info = {
    .name          = TYPE_AT91DISPLAY,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(at91display_state),
    .instance_init = at91display_init,
    .class_init    = at91display_class_init,
};

static void at91display_register_types(void)
{
    //Achtung! Hier können auch mehrere verschiedene TypeInfos angegeben werden!
    type_register_static(&at91display_info);
}



type_init(at91display_register_types)
