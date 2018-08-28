/*
 * Atmel AT91RM9200 System Timer (ST)
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "sysemu/sysemu.h"

#define TYPE_AT91ST "at91st"
#define AT91ST(obj) OBJECT_CHECK(at91st_state, (obj), TYPE_AT91ST)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    QEMUTimer *st_timer;
    QEMUTimer *wd_timer;
    QEMUTimer *rt_timer;
    qemu_irq irq;
    uint32_t pimr;
    uint32_t wdmr;
    uint32_t rtmr;
    uint32_t sr;
    uint32_t imr;
    uint32_t rtar;
    uint32_t crtr;
} at91st_state;

static const VMStateDescription vmstate_at91st = {
    .name = "at91st",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(pimr, at91st_state),
        VMSTATE_UINT32(wdmr, at91st_state),
        VMSTATE_UINT32(rtmr, at91st_state),
        VMSTATE_UINT32(sr, at91st_state),
        VMSTATE_UINT32(imr, at91st_state),
        VMSTATE_UINT32(rtar, at91st_state),
        VMSTATE_UINT32(crtr, at91st_state),
        VMSTATE_END_OF_LIST()
    }
};


/* System Timer Register Mapping */
#define ST_CR   0x00
#define ST_PIMR 0x04
#define ST_WDMR 0x08
#define ST_RTMR 0x0C
#define ST_SR   0x10
#define ST_IER  0x14
#define ST_IDR  0x18
#define ST_IMR  0x1C
#define ST_RTAR 0x20
#define ST_CRTR 0x24

/* Status bits and masks */
#define WDRST (1 << 0)

#define PIMR_MASK 0xffff
#define PIV_MASK 0xffff

#define WDMR_MASK 0x0001ffff
#define WDV_MASK 0xffff
#define RSTEN (1 << 16)

#define RTMR_MASK 0xffff
#define RTPRES_MASK 0xffff

#define IMR_MASK 0x000f
#define PITS (1 << 0)
#define WDOVF (1 << 1)
#define RTTINC (1 << 2)
#define ALMS (1 << 3)

#define RTAR_MASK 0x000fffff
#define ALMV_MASK 0x000fffff

#define CRTV_MASK 0x000fffff

static void at91st_update(at91st_state *s)
{
    /*
     * Signal IRQ if an interrupt is enabled and the corresponding
     * status bit is set
     */
    qemu_set_irq(s->irq, !!(s->imr & s->sr));
}

static void st_set_alarm(at91st_state *s)
{
    int64_t t = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    int64_t piv = s->pimr & PIV_MASK;

    /* zero in PIV is largest value */
    if (piv == 0) {
        piv = 0x10000;
    }

    /* piv == 0x10000 => 2 second interval */
    t += NANOSECONDS_PER_SECOND * piv * 2 / 0x10000;

    timer_mod(s->st_timer, t);
}

static void st_interrupt(void *opaque)
{
    at91st_state *s = opaque;
    s->sr |= PITS;
    st_set_alarm(s);
    at91st_update(s);
}

static void wd_set_alarm(at91st_state *s)
{
    int64_t t = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    int64_t wdv = s->wdmr & WDV_MASK;

    /* zero in WDV is largest value */
    if (wdv == 0)
        wdv = 0x10000;

    /* wdv == 0x10000 => 256 second interval */
    t += NANOSECONDS_PER_SECOND * wdv * 256 / 0x10000;

    timer_mod(s->wd_timer, t);
}

static void wd_interrupt(void *opaque)
{
    at91st_state *s = opaque;
    s->sr |= WDOVF;
    if (s->wdmr & RSTEN) {
        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
    }
    wd_set_alarm(s);
    at91st_update(s);
}

static void rt_set_alarm(at91st_state *s)
{
    int64_t t = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    int64_t rtpres = s->rtmr & RTPRES_MASK;

    /* zero in RTPRES is largest value */
    if (rtpres == 0)
        rtpres = 0x10000;

    /* rtpres == 0x10000 => 2 second interval */
    t += NANOSECONDS_PER_SECOND * rtpres * 2 / 0x10000;

    timer_mod(s->rt_timer, t);
}

static void rt_interrupt(void *opaque)
{
    at91st_state *s = opaque;

    /* Increment Realtime Counter */
    s->crtr = (s->crtr + 1) & CRTV_MASK;
    s->sr |= RTTINC;

    /* Check alarm */
    if (s->crtr == s->rtar) {
        s->sr |= ALMS;
    }

    rt_set_alarm(s);
    at91st_update(s);
}

static uint64_t at91st_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    at91st_state *s = (at91st_state *)opaque;
    uint32_t temp;

    switch (offset) {
    case ST_PIMR:   // Period Interval Mode Register
        return s->pimr;
    case ST_WDMR:   // Watchdog Mode Register
        return s->wdmr;
    case ST_RTMR:   // Real-Time Mode Register
        return s->rtmr;
    case ST_SR:     // Status Register
        temp = s->sr;
        s->sr = 0;
        at91st_update(s);
        return temp;
    case ST_IMR:    // Interrupt Mask Register
        return s->imr;
    case ST_RTAR:   // Real-Time Alarm Register
        return s->rtar;
    case ST_CRTR:   // Current Real-Time Register
        return s->crtr;
    default:
        fprintf(stderr, "at91st_read: Bad offset %x (returning zero)\n", (int)offset);
        return 0;
    }
}

static void at91st_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    at91st_state *s = opaque;

    switch (offset) {
    case ST_CR:     // Control Register
        if (value & WDRST) {
            wd_set_alarm(s);
        }
        break;
    case ST_PIMR:   // Period Interval Mode Register
        s->pimr = value & PIMR_MASK;
        st_set_alarm(s);
        break;
    case ST_WDMR:   // Watchdog Mode Register
        s->wdmr = value & WDMR_MASK;
        break;
    case ST_RTMR:   // Real-Time Mode Register
        s->rtmr = value & RTMR_MASK;
        rt_set_alarm(s);
        break;
    case ST_IER:    // Interrupt Enable Register
        s->imr |= value & IMR_MASK;
        at91st_update(s);
        break;
    case ST_IDR:    // Interrupt Disable Register
        s->imr &= ~(value & IMR_MASK);
        at91st_update(s);
        break;
    case ST_RTAR:   // Real-Time Alarm Register
        s->rtar = value & RTAR_MASK;
        break;
    default:
        fprintf(stderr, "at91st_write: Bad offset %x\n", (int)offset);
    }
}

static const MemoryRegionOps at91st_ops = {
    .read = at91st_read,
    .write = at91st_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void at91st_reset(DeviceState *dev) {
    at91st_state *s = AT91ST(dev);

    s->pimr = 0;
    s->wdmr = 0; /* Datasheet say default is 0x20000, but that bit is not defined */
    s->rtmr = 0x8000;
    s->sr = 0;
    s->imr = 0;
    s->rtar = 0;
    s->crtr = 0;

    at91st_update(s);
}

static int at91st_init(SysBusDevice *dev)
{
    at91st_state *s = AT91ST(dev);
    struct tm tm;

    memory_region_init_io(&s->iomem, OBJECT(s), &at91st_ops, s, "at91st", 0x1FF);
    sysbus_init_mmio(dev, &s->iomem);

    sysbus_init_irq(dev, &s->irq);
    qemu_get_timedate(&tm, 0);

    s->st_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, st_interrupt, s);
    s->wd_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, wd_interrupt, s);
    s->rt_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, rt_interrupt, s);

    at91st_reset(DEVICE(dev));
    return 0;
}

static void at91st_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = at91st_init;
    dc->user_creatable = false; /* FIXME explain why */
    dc->reset = at91st_reset;
    dc->vmsd = &vmstate_at91st;
}

static const TypeInfo at91st_info = {
    .name          = TYPE_AT91ST,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(at91st_state),
    .class_init    = at91st_class_init,
};

static void at91st_register_types(void)
{
    type_register_static(&at91st_info);
}

type_init(at91st_register_types)
