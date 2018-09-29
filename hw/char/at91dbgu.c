/*
 * Debug Unit
 *
 * No support for Channel Test Modes.
 * Every baud rate work; no parity checks.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#define TYPE_AT91DBGU "at91dbgu"
#define AT91DBGU(obj) OBJECT_CHECK(at91dbgu_state, (obj), TYPE_AT91DBGU)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    CharBackend chr;

    // DBGU Register
    uint32_t cr;
    uint32_t mr;
    uint32_t imr;
    uint32_t sr;
    uint32_t rhr;
    uint32_t brgr;

    // DMA Register
    uint32_t periph_rpr; // Receive Pointer Register
    uint32_t periph_rcr; // Receive Counter Register
    uint32_t periph_tpr; // Transmit Pointer Register
    uint32_t periph_tcr; // Transmit Counter Register
    uint32_t periph_rnpr; // Receive Next Pointer Register
    uint32_t periph_rncr; // Receive Next Counter Register
    uint32_t periph_tnpr; // Transmit Next Pointer Register
    uint32_t periph_tncr; // Transmit Next Counter Register
    uint32_t periph_ptsr; // Transfer Status Register

} at91dbgu_state;

static const VMStateDescription vmstate_at91dbgu = {
    .name = "at91dbgu",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32(cr, at91dbgu_state),
        VMSTATE_UINT32(mr, at91dbgu_state),
        VMSTATE_UINT32(imr, at91dbgu_state),
        VMSTATE_UINT32(sr, at91dbgu_state),
        VMSTATE_UINT32(rhr, at91dbgu_state),
        VMSTATE_UINT32(brgr, at91dbgu_state),
        VMSTATE_UINT32(periph_rpr, at91dbgu_state),
        VMSTATE_UINT32(periph_rcr, at91dbgu_state),
        VMSTATE_UINT32(periph_tpr, at91dbgu_state),
        VMSTATE_UINT32(periph_tcr, at91dbgu_state),
        VMSTATE_UINT32(periph_rnpr, at91dbgu_state),
        VMSTATE_UINT32(periph_rncr, at91dbgu_state),
        VMSTATE_UINT32(periph_tnpr, at91dbgu_state),
        VMSTATE_UINT32(periph_tncr, at91dbgu_state),
        VMSTATE_UINT32(periph_ptsr, at91dbgu_state),
        VMSTATE_END_OF_LIST()
    }
};

#define DBGU_CR     0x00
#define DBGU_MR     0x04
#define DBGU_IER    0x08
#define DBGU_IDR    0x0c
#define DBGU_IMR    0x10
#define DBGU_SR     0x14
#define DBGU_RHR    0x18
#define DBGU_THR    0x1c
#define DBGU_BRGR   0x20
#define DBGU_CIDR   0x40
#define DBGU_EXID   0x44

#define RSTRX   (1 << 2)
#define RSTTX   (1 << 3)
#define RXEN    (1 << 4)
#define RXDIS   (1 << 5)
#define TXEN    (1 << 6)
#define TXDIS   (1 << 7)
#define RSTSTA  (1 << 8)

#define DBGU_MR_MASK    0xce00
#define CHMODE_MASK     0xc000
#define PAR_MASK        0x0e00

#define DBGU_SR_MASK    0xc0001afb
#define RXRDY   (1 << 0)
#define TXRDY   (1 << 1)
#define ENDRX   (1 << 3)
#define ENDTX   (1 << 4)
#define OVRE    (1 << 5)
#define FRAME   (1 << 6)
#define PARE    (1 << 7)
#define TXEMPTY (1 << 9)
#define TXBUFE  (1 << 11)
#define RXBUFF  (1 << 12)
#define COMMTX  (1 << 30)
#define COMMRX  (1 << 31)

#define DBGU_RHR_MASK   0xff
#define DBGU_THR_MASK   0xff
#define DBGU_BRGR_MASK  0xffff

#define PERIPH_RPR  0x100
#define PERIPH_RCR  0x104
#define PERIPH_TPR  0x108
#define PERIPH_TCR  0x10c
#define PERIPH_RNPR 0x110
#define PERIPH_RNCR 0x114
#define PERIPH_TNPR 0x118
#define PERIPH_TNCR 0x11c
#define PERIPH_PTCR 0x120
#define PERIPH_PTSR 0x124

#define PERIPH_RCR_MASK 0xffff
#define PERIPH_TCR_MASK 0xffff
#define PERIPH_RNCR_MASK 0xffff
#define PERIPH_TNCR_MASK 0xffff

#define RXTEN   (1 << 0)
#define RXTDIS  (1 << 1)
#define TXTEN   (1 << 8)
#define TXTDIS  (1 << 9)

static void at91dbgu_send(void *opaque, const uint8_t *buf, int size)
{
    at91dbgu_state *s = opaque;

    qemu_chr_fe_write(&s->chr, buf, size);
}

static void at91dbgu_update(at91dbgu_state *s)
{
    /* process PDC receive */
    if (s->periph_rcr == 0 && s->periph_rncr != 0) {
        s->periph_rcr = s->periph_rncr;
        s->periph_rpr = s->periph_rnpr;
        s->periph_rncr = 0;
    }
    if (s->periph_ptsr & RXTEN) {
        if (s->sr & RXRDY && s->periph_rcr > 0) {
            char c = s->rhr;
            cpu_physical_memory_write(s->periph_rpr, &c, 1);
            s->periph_rcr -= 1;
            s->periph_rpr += 1;
            s->sr &= ~RXRDY;
            if (s->periph_rcr == 0 && s->periph_rncr != 0) {
                s->periph_rcr = s->periph_rncr;
                s->periph_rpr = s->periph_rnpr;
                s->periph_rncr = 0;
            }
        }
    }
    if (s->periph_rcr == 0) {
        s->sr |= ENDRX;
        if (s->periph_rncr == 0) {
            s->sr |= RXBUFF;
        }
    }

    /* process PDC transmit */
    if (s->periph_tcr == 0 && s->periph_tncr != 0) {
        s->periph_tcr = s->periph_tncr;
        s->periph_tpr = s->periph_tnpr;
        s->periph_tncr = 0;
    }
    if (s->periph_ptsr & TXTEN && s->cr & TXEN) {
        while (s->periph_tcr) {
            uint32_t len = s->periph_tcr <= 32 ? s->periph_tcr : 32;
            uint8_t buf[32];
            cpu_physical_memory_read(s->periph_tpr, buf, len);
            at91dbgu_send(s, buf, len);
            s->periph_tpr += len;
            s->periph_tcr -= len;

            if (s->periph_tcr == 0 && s->periph_tncr != 0) {
                s->periph_tcr = s->periph_tncr;
                s->periph_tpr = s->periph_tnpr;
                s->periph_tncr = 0;
            }
        }
    }
    if (s->periph_tcr == 0) {
        s->sr |= ENDTX;
        if (s->periph_tncr == 0) {
            s->sr |= TXBUFE;
        }
    }

    /* update IRQs */
    qemu_set_irq(s->irq, s->sr & s->imr);
}

static void at91dbgu_receive(void *opaque, const uint8_t *buf, int size)
{
    at91dbgu_state *s = opaque;

    if (!(s->cr & RXEN)) {
        return;
    }

    s->rhr = *buf;
    if (s->sr & RXRDY) {
        s->sr |= OVRE;
    } else {
        s->sr |= RXRDY;
    }
    at91dbgu_update(s);
}

static int at91dbgu_can_receive(void *opaque)
{
    at91dbgu_state *s = opaque;
    if (s->sr & RXRDY) {
        return 0;
    } else {
        return 1;
    }
}

static void at91dbgu_event(void *opaque, int event)
{
}

static uint64_t at91dbgu_read(void *opaque, hwaddr offset, unsigned size)
{
    at91dbgu_state *s = opaque;

    switch (offset) {
        case DBGU_MR: // Mode Register
            return s->mr;
        case DBGU_IMR: // Interrupt Mask Register
            return s->imr;
        case DBGU_SR: // Status Register
            return s->sr;
        case DBGU_RHR: // Receive Holding Register
            s->sr &= ~RXRDY;
            at91dbgu_update(s);
            return s->rhr;
        case DBGU_BRGR: // Baud Rate Generator Register
            return s->brgr;
        case DBGU_CIDR: // Chip Id Register
                // 0bENNNAAAAAAAASSSS0000NNNNPPPVVVVV
            return 0b00001001001010010000001010000000;
        case DBGU_EXID: // Chip Id Extension Register
            return 0;
        case PERIPH_RPR: // Receive Pointer Register
            return s->periph_rpr;
        case PERIPH_RCR: // Receive Counter Register
            return s->periph_rcr;
        case PERIPH_TPR: // Transmit Pointer Register
            return s->periph_tpr;
        case PERIPH_TCR: // Transmit Counter Register
            return s->periph_tcr;
        case PERIPH_RNPR: // Receive Next Pointer Register
            return s->periph_rnpr;
        case PERIPH_RNCR: // Receive Next Counter Register
            return s->periph_rncr;
        case PERIPH_TNPR: // Transmit Next Pointer Register
            return s->periph_tnpr;
        case PERIPH_TNCR: // Transmit Next Counter Register
            return s->periph_tncr;
        case PERIPH_PTSR: // Transfer Status Register
            return s->periph_ptsr;
        default:
            fprintf(stderr, "at91dbgu_read: Bad offset %x (returning zero)\n", (int)offset);
            return 0;
    }
}

static void at91dbgu_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    at91dbgu_state *s = opaque;

    switch (offset) {
        case DBGU_CR: // Control Register
            if (value & RSTRX) {
                s->cr &= ~RXEN;
            }
            if (value & RSTTX) {
                s->cr &= ~TXEN;
            }
            if (value & RXEN) {
                s->cr |= RXEN;
            }
            if (value & RXDIS) {
                s->cr &= ~RXEN;
            }
            if (value & TXEN) {
                s->cr |= TXEN;
            }
            if (value & TXDIS) {
                s->cr &= ~TXEN;
            }
            if (value & RSTSTA) {
                s->sr &= ~(PARE | FRAME | OVRE);
            }
            at91dbgu_update(s);
            break;
        case DBGU_MR: // Mode Register
            s->mr = value & DBGU_MR_MASK;
            break;
        case DBGU_IER: // Interrupt Enable Register
            s->imr |= value & DBGU_SR_MASK;
            at91dbgu_update(s);
            break;
        case DBGU_IDR: // Interrupt Disable Register
            s->imr &= ~value;
            at91dbgu_update(s);
            break;
        case DBGU_THR: // Transmit Holding Register
            if (s->cr & TXEN) {
                unsigned char ch = value;
                at91dbgu_send(s, &ch, 1);
            }
            break;
        case DBGU_BRGR: // Baud Rate Generator Register
            s->brgr = value & DBGU_BRGR_MASK;
            break;
        case PERIPH_RPR: // Receive Pointer Register
            s->periph_rpr = value;
            break;
        case PERIPH_RCR: // Receive Counter Register
            s->periph_rcr = value & PERIPH_RCR_MASK;
            at91dbgu_update(s);
            break;
        case PERIPH_TPR: // Transmit Pointer Register
            s->periph_tpr = value;
            break;
        case PERIPH_TCR: // Transmit Counter Register
            s->periph_tcr = value & PERIPH_TCR_MASK;
            at91dbgu_update(s);
            break;
        case PERIPH_RNPR: // Receive Next Pointer Register
            s->periph_rnpr = value;
            break;
        case PERIPH_RNCR: // Receive Next Counter Register
            s->periph_rncr = value & PERIPH_RNCR_MASK;
            at91dbgu_update(s);
            break;
        case PERIPH_TNPR: // Transmit Next Pointer Register
            s->periph_tnpr = value;
            break;
        case PERIPH_TNCR: // Transmit Next Counter Register
            s->periph_tncr = value & PERIPH_TNCR_MASK;
            at91dbgu_update(s);
            break;
        case PERIPH_PTCR: // Transfer Control Register
            s->periph_ptsr |= value & (RXTEN | TXTEN);
            s->periph_ptsr &= ~((value & (RXTDIS | TXTDIS)) >> 1);
            at91dbgu_update(s);
            break;
        default:
            fprintf(stderr, "at91dbgu_write: Bad offset %x\n", (int)offset);
    }
}

static const MemoryRegionOps at91dbgu_ops = {
    .read = at91dbgu_read,
    .write = at91dbgu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void at91dbgu_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    at91dbgu_state *s = AT91DBGU(obj);
    
    memory_region_init_io(&s->iomem, OBJECT(s), &at91dbgu_ops, s, "at91dbgu", 0x200);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);

    // DBGU
    s->cr = TXEN | RXEN; /* Should be zero */
    s->mr = 0x0;
    s->imr = 0x0;
    s->sr = TXRDY | TXEMPTY; /* Should be zero */
    s->rhr = 0x0;
    s->brgr = 0x0;

    // PDC
    s->periph_rpr = 0x0;
    s->periph_rcr = 0x0;
    s->periph_tpr = 0x0;
    s->periph_tcr = 0x0;
    s->periph_rnpr = 0x0;
    s->periph_rncr = 0x0;
    s->periph_tnpr = 0x0;
    s->periph_tncr = 0x0;
    s->periph_ptsr = 0x0;
}

static void at91dbgu_realize(DeviceState *dev, Error **errp)
{
    at91dbgu_state *s = AT91DBGU(dev);
    
    qemu_chr_fe_set_handlers(&s->chr, at91dbgu_can_receive, at91dbgu_receive,
            at91dbgu_event, NULL, s, NULL, true);
}

static Property at91dbgu_properties[] = {
    DEFINE_PROP_CHR("chardev", at91dbgu_state, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void at91dbgu_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    
    dc->realize = at91dbgu_realize;
    dc->vmsd = &vmstate_at91dbgu;
    dc->props = at91dbgu_properties;
}

static const TypeInfo at91dbgu_info = {
    .name          = TYPE_AT91DBGU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(at91dbgu_state),
    .instance_init = at91dbgu_init,
    .class_init    = at91dbgu_class_init,
};

static void at91dbgu_register_types(void)
{
    type_register_static(&at91dbgu_info);
}

type_init(at91dbgu_register_types)
