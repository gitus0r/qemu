/*
 * Atmel AT91SAM7XC Advanced Interrupt Controller emulation.
 *
 * Copyright (c) 2012 René Sechting
 * Copyright (c) 2009 Daniel van Gerpen
 * Copyright (c) 2006 Pekka Nikander
 *
 * Ported to AT91SAM7XC by Daniel van Gerpen based on Pekka Nikander's code.
 * Written by Pekka Nikander based on Paul Brook's earlied code
 *
 * This code is licenced under the GPL.
 */

#include "qemu/osdep.h"
#include <stdio.h>
#include <stddef.h>

#include "hw/sysbus.h"


#define TYPE_AT91AIC "at91aic"
#define AT91AIC(obj) OBJECT_CHECK(at91aic_state, (obj), TYPE_AT91AIC)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    qemu_irq fiq;
    uint32_t smr[32];
    uint32_t svr[32];
    uint32_t isr[8];
    int32_t isr_prio;
    uint32_t levels;
    uint32_t edges;
    uint32_t st_edge;
    uint32_t st_positive;
    uint32_t ipr;
    uint32_t imr;
    uint32_t cisr;
    uint32_t spu;
    uint32_t dcr;
} at91aic_state;

static const VMStateDescription vmstate_at91aic = {
    .name = "at91aic",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(smr, at91aic_state, 32),
        VMSTATE_UINT32_ARRAY(svr, at91aic_state, 32),
        VMSTATE_UINT32_ARRAY(isr, at91aic_state, 8),
        VMSTATE_INT32(isr_prio, at91aic_state),
        VMSTATE_UINT32(levels, at91aic_state),
        VMSTATE_UINT32(edges, at91aic_state),
        VMSTATE_UINT32(st_edge, at91aic_state),
        VMSTATE_UINT32(st_positive, at91aic_state),
        VMSTATE_UINT32(ipr, at91aic_state),
        VMSTATE_UINT32(imr, at91aic_state),
        VMSTATE_UINT32(cisr, at91aic_state),
        VMSTATE_UINT32(spu, at91aic_state),
        VMSTATE_UINT32(dcr, at91aic_state),
        VMSTATE_END_OF_LIST()
    }
};

#define AIC_SMR0    0x000
#define AIC_SMR31   0x07c
#define AIC_SVR0    0x080
#define AIC_SVR31   0x0fc
#define AIC_IVR     0x100
#define AIC_FVR     0x104
#define AIC_ISR     0x108
#define AIC_IPR     0x10c
#define AIC_IMR     0x110
#define AIC_CISR    0x114
#define AIC_IECR    0x120
#define AIC_IDCR    0x124
#define AIC_ICCR    0x128
#define AIC_ISCR    0x12c
#define AIC_EOICR   0x130
#define AIC_SPU     0x134
#define AIC_DCR     0x138

#define AIC_SMR_MASK    0b01100111
#define ST_EDGE         (1 << 5)
#define ST_POSITIVE     (1 << 6)
#define PRIOR_MASK      0b00000111

#define NFIQ            (1 << 0)
#define NIRQ            (1 << 1)

#define AIC_DCR_MASK    0x03
#define PROT            (1 << 0)
#define GMSK            (1 << 1)

static inline int priority_of(at91aic_state *s, int irq)
{
    if (irq == 0) {
        return -1;
    }
    return s->smr[irq] & PRIOR_MASK;
}

static int highest_active_irq(at91aic_state *s)
{
    int i, irq = 0;
    int pending = s->ipr & s->imr;
    int prio = -1;

    for (i = 1; i < 32; i++) {
        if (!(pending & (1 << i))) {
            continue;
        }
        if (prio >= priority_of(s, i)) {
            continue;
        }
        irq = i;
        prio = priority_of(s, i);
    }
    return irq;
}


/* Update interrupts.  */
static void at91aic_update(at91aic_state *s)
{
    /* Update Pending IRQs */
    uint32_t t = ~(s->levels ^ s->st_positive);
    s->ipr = (t & ~s->st_edge) | (s->edges & s->st_edge);

    /* Update CISR */
    int irq = highest_active_irq(s);
    int prio = priority_of(s, irq);

    if (prio > s->isr_prio) {
        s->cisr |= NIRQ;
    } else {
        s->cisr &= ~NIRQ;
    }

    if (s->ipr & s->imr & NFIQ) {
        s->cisr |= NFIQ;
    } else {
        s->cisr &= ~NFIQ;
    }
    if (s->dcr & GMSK) {
        s->cisr = 0;
    }

    /* Update IRQ-Lines to CPU */
    qemu_set_irq(s->fiq, s->cisr & NFIQ);
    qemu_set_irq(s->irq, s->cisr & NIRQ);
}

static void at91aic_signal_eoi(at91aic_state *s)
{
    if (s->isr_prio == -1) {
        fprintf(stderr, "at91aic_signal_eoi: write to AIC_EOICR without corresponding read from AIC_IVR\n");
        return;
    }
    s->isr[s->isr_prio] = 0;
    while (s->isr[s->isr_prio] == 0) {
        s->isr_prio--;
        if (s->isr_prio == -1) {
            break;
        }
    }
    at91aic_update(s);
}

static uint64_t at91aic_get_ivr(at91aic_state *s, int write)
{
    int irq = highest_active_irq(s);
    int prio = priority_of(s, irq);

    if (prio > s->isr_prio) {
        /* enabled active IRQ with higher priority */
        if (write || !(s->dcr & PROT)) {
            s->isr_prio = prio;
            s->isr[s->isr_prio] = irq;
            s->edges &= ~(1 << irq);
            at91aic_update(s);
        }
        return s->svr[irq];
    } else {
        /* spurious IRQ */
        if (s->isr_prio == 7) {
            fprintf(stderr, "at91aic_get_ivr: AIC_IVR read while already at max priority\n");
        }
        if (write || !(s->dcr & PROT)) {
            s->isr_prio = 7;
            s->isr[s->isr_prio] = 0;
        }
        return s->spu;
    }
}

static uint64_t at91aic_read(void *opaque, hwaddr offset, unsigned size)
{
    at91aic_state *s = opaque;

    /* Access to SMR or SVR array */
    if (/*offset >= AIC_SMR0 &&*/ offset <= AIC_SMR31 && (offset & 3) == 0) {
        return s->smr[(offset - AIC_SMR0) >> 2];
    }
    if (offset >= AIC_SVR0 && offset <= AIC_SVR31 && (offset & 3) == 0) {
        return s->svr[(offset - AIC_SVR0) >> 2];
    }

    /* Other registers */
    switch (offset) {
    case AIC_IVR: // Interrupt Vector Register
        return at91aic_get_ivr(s, false);
    case AIC_FVR: // Fast Interrupt Vector Register
        return s->svr[0];
    case AIC_ISR: // Interrupt Status Register
        return s->isr_prio >= 0 ? s->isr[s->isr_prio] : 0;
    case AIC_IPR: // Interrupt Pending Register
        return s->ipr;
    case AIC_IMR: // Interrupt Mask Register
        return s->imr;
    case AIC_CISR: // Core Interrupt Status Register
        return s->cisr;
    case AIC_SPU: // Spurious Interrupt Vector Register
        return s->spu;
    case AIC_DCR: // Debug Control Register
        return s->dcr;
    default:
        fprintf(stderr, "at91aic_read: Bad offset %x (returning zero)\n", (int)offset);
        return 0;
    }
}

static void at91aic_write(void *opaque, hwaddr offset, uint64_t val, unsigned size)
{
    at91aic_state *s = opaque;

    /* Access to SMR or SVR array */
    if (/*offset >= AIC_SMR0 &&*/ offset <= AIC_SMR31 && (offset & 3) == 0) {
        int irq = (offset - AIC_SMR0) >> 2;

        s->smr[irq] = val & AIC_SMR_MASK;

        if (val & ST_EDGE) {
            s->st_edge |= 1 << irq;
        } else {
            s->st_edge &= ~(1 << irq);
        }
        if (val & ST_POSITIVE || irq == 1) {
            s->st_positive |= 1 << irq;
            s->smr[irq] |= ST_POSITIVE;
        } else {
            s->st_positive &= ~(1 << irq);
        }

        at91aic_update(s);
        return;
    } else if (offset >= AIC_SVR0 && offset <= AIC_SVR31 && (offset & 3) == 0) {
        s->svr[(offset - AIC_SVR0) >> 2] = val;
        return;
    }

    /* Other registers */
    switch (offset) {
    case AIC_IVR: // Interrupt Vector Register
        at91aic_get_ivr(s, true);
        break;
    case AIC_IECR: // Interrupt Enable Command Register
        s->imr |= val;
        at91aic_update(s);
        break;
    case AIC_IDCR: // Interrupt Disable Command Register
        s->imr &= ~val;
        at91aic_update(s);
        break;
    case AIC_ICCR: // Interrupt Clear Command Register
        s->edges &= ~val;
        at91aic_update(s);
        break;
    case AIC_ISCR: // Interrupt Set Command Register
        s->edges |= val;
        at91aic_update(s);
        break;
    case AIC_EOICR: // End Of Interrupt Command Register
        at91aic_signal_eoi(s);
        break;
    case AIC_SPU: // Spurious Interrupt Vector Register
        s->spu = val;
        break;
    case AIC_DCR: // Debug Control Register
        s->dcr = val & AIC_DCR_MASK;
        at91aic_update(s);
        break;
    default:
        fprintf(stderr, "at91aic_write: Bad offset %x\n", (int)offset);
    }
}

static void at91aic_reset(DeviceState *dev) {
    at91aic_state *s = AT91AIC(dev);
    int i;
    for (i = 0; i < 32; i++) {
      s->smr[i] = 0;
      s->svr[i] = 0;
    }
    s->smr[1] |= ST_POSITIVE;
    for (i = 0; i < 8; i++) {
        s->isr[i] = 0;
    }
    s->isr_prio = -1;
    /* levels need no reset */
    s->edges = 0;
    s->st_edge = 0;
    s->st_positive = 2;
    s->imr = 0;
    s->spu = 0;
    s->dcr = 0;

    /* Sets ipr, cisr */
    at91aic_update(s);
}

static const MemoryRegionOps at91aic_ops = {
    .read = at91aic_read,
    .write = at91aic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void at91aic_set_irq(void *opaque, int irq, int level) {
    at91aic_state *s = opaque;
    int old_levels = s->levels;

    /* Store current level of irq */
    if (level) {
        s->levels |= 1 << irq;
    } else {
        s->levels &= ~(1 << irq);
    }

    /* Detect edge */
    s->edges |= (old_levels ^ s->levels) & (s->levels ^ ~s->st_positive);

    at91aic_update(s);
}

static int at91aic_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    at91aic_state *s = AT91AIC(dev);
    memory_region_init_io(&s->iomem, OBJECT(s), &at91aic_ops, s, "at91aic", 0x1FF);
    sysbus_init_mmio(sbd, &s->iomem);
    qdev_init_gpio_in(dev, at91aic_set_irq, 32);
    sysbus_init_irq(sbd, &s->irq);
    sysbus_init_irq(sbd, &s->fiq);

    at91aic_reset(dev);
    return 0;
}

static void at91aic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);
    k->init = at91aic_init;
    dc->user_creatable = false;
    dc->reset = at91aic_reset;
    dc->vmsd = &vmstate_at91aic;
}

static const TypeInfo at91aic_info = {
    .name = TYPE_AT91AIC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(at91aic_state),
    .class_init = at91aic_class_init,
};

static void at91aic_register_types(void)
{
    type_register_static(&at91aic_info);
}

type_init(at91aic_register_types)
