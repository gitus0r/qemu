/*
 * Portux920t USART
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "chardev/char.h"
#include "chardev/char-fe.h"

/* Control Register */
#define CR_RXEN (1<<4)
#define CR_TXEN (1<<6)
#define CR_RXDIS (1<<5)
#define CR_TXDIS (1<<7)
#define CR_RSTRX (1<<2)
#define CR_RSTTX (1<<3)
/* Interrupt Mask Register */
#define IMR_RXRDY (1<<0)
#define IMR_TXRDY (1<<1)
/* Channel Status Register */
#define CSR_RXRDY (1<<0)
#define CSR_TXRDY (1<<1)
#define CSR_OVRE (1<<5)
#define CSR_TXEMPTY (1<<9)


#define TYPE_AT91USART "at91usart"
#define AT91USART(obj) OBJECT_CHECK(at91usart_state, (obj), TYPE_AT91USART)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    uint32_t cr;
    uint32_t mr;
    uint32_t imr;
    uint32_t csr;
    uint32_t rhr;
    CharBackend chr;
    qemu_irq irq;
    const unsigned char *id;
    //DMA Register
    uint32_t periph_rpr; //Receive Pointer Register
    uint32_t periph_rcr; //Receive Counter Register
    uint32_t periph_tpr; //Transmit Pointer Register
    uint32_t periph_tcr; //Transmit Counter Register
    uint32_t periph_rnpr; //Receive Next Pointer Register
    uint32_t periph_rncr; //Receive Next Counter Register
    uint32_t periph_tnpr; //Transmit Next Pointer Register
    uint32_t periph_tncr; //Transmit Next Counter Register
    uint32_t periph_ptcr; //Transfer Control Register
    uint32_t periph_ptsr; //Transfer Status Register
} at91usart_state;

/*
 * source: 0=receive 1=transmit
 */
static void at91usart_update(at91usart_state *s, int source)
{
    if((1<<source)&s->imr){
        qemu_set_irq(s->irq, 1);
    }
}

static uint64_t at91usart_read(void *opaque, hwaddr offset,
                           unsigned size)
{

    at91usart_state *s = (at91usart_state *)opaque;
    switch (offset) {
    case 0x04: /* US_MR */
        return s->mr;
    case 0x10: /* US_IMR */
        return s->imr;
    case 0x14: /* US_CSR */
        return s->csr;
    case 0x18: /* US_RHR */
        return s->rhr;
    case 0x20: /* US_BRGR (not implemented -> return 0) */
        return 0;
    case 0x24: /* US_RTOR (not implemented -> return 0) */
        return 0;
    case 0x28: /* US_TTGR (not implemented -> return 0) */
        return 0;
    case 0x40: /* US_FIDI (not implemented -> return 0) */
        return 0;
    case 0x44: /* US_NER (not implemented -> return 0) */
        return 0;
    case 0x4C: /* US_IF (not implemented -> return 0) */
        return 0;
    case 0x100: //Receive Pointer Register
        return s->periph_rpr;
    case 0x104: //Receive Counter Register
        return s->periph_rcr;
    case 0x108: //Transmit Pointer Register
        return s->periph_tpr;
    case 0x10C: //Receive Pointer Register
        return s->periph_tcr;
    case 0x110: //Receive Next Pointer Register
        return s->periph_rnpr;
    case 0x114: //Receive Next Counter Register
        return s->periph_rncr;
    case 0x118: //Transmit Next Pointer Register
        return s->periph_tnpr;
    case 0x11C: //Transmit Next Counter Register
        return s->periph_tncr;
    case 0x124: //Transfer Status Register
        return s->periph_ptsr;
    default:
        fprintf(stderr, "at91usart_read: Bad offset %x (returning zero)\n", (int)offset);
        return 0;
    }
}

static void at91usart_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    at91usart_state *s = (at91usart_state *)opaque;
    unsigned char ch;
    switch (offset) {
    case 0x00: /* US_CR */
        if((value&CR_RXEN)&&!(s->cr&(1<<5))){
            s->cr |= CR_RXEN; //enable receive
        }
        if((value&CR_TXEN)&&!(s->cr&(1<<7))){
            s->cr |= CR_TXEN; //enable transmit
        }

        if(value&CR_RXDIS){    //disable receiver
            if(s->cr&CR_RXEN) s->cr &= ~CR_RXEN;
            s->cr |= CR_RXDIS;
        }
        if(value&CR_TXDIS){ //disable dtransmitter
            if(s->cr&CR_TXEN) s->cr &= ~CR_TXEN;
            s->cr |= CR_TXDIS;
            s->csr &= ~(CSR_TXRDY|CSR_TXEMPTY);
        }
        if(value&CR_RSTRX){
            s->cr &= ~0x34; //reset CR_RXEN
        }
        if(value&CR_RSTTX){
            s->cr &= ~0xC8; //reset CR_TXEN
        }
        break;
    case 0x04: /* US_MR */
        s->mr = value;
        break;
    case 0x08: /* US_IER */
        s->imr |= value;
        break;
    case 0x0C: /* US_IDR */
        s->imr &= ~value;
        break;
    case 0x1C: /* US_THR */
        ch = value;
        if(s->cr & CR_TXEN){
            s->csr&=~CSR_TXRDY; //upon writing a char into the thr register set transmitter ready bit in the csr to 0
            qemu_chr_fe_write(&s->chr, &ch, 1);
            if(!(s->cr&CR_TXDIS))s->csr|=CSR_TXRDY;// set transmitter ready bit in the csr wieder to 1
            at91usart_update(s,1);
        }
        break;
    case 0x20: /* US_BRGR (not implemented) */
        //Baud Rate Generator not needed
        break;
    case 0x24: /* US_RTOR (not implemented) */
        //do nothing
        break;
    case 0x28: /* US_TTGR (not implemented) */
        //do nothing
        break;
    case 0x40: /* US_FIDI (not implemented) */
        //do nothing
        break;
    case 0x4C: /* US_IF (not implemented) */
        //do nothing
        break;
    case 0x100: //Receive Pointer Register
        s->periph_rpr=value;
        break;
    case 0x104: //Receive Counter Register
        s->periph_rcr=value;
        break;
    case 0x108: //Transmit Pointer Register
        s->periph_tpr=value;
        break;
    case 0x10C: //Transmit Counter Register
        s->periph_tcr=value;
        break;
    case 0x110: //Receive Next Pointer Register
        s->periph_rnpr=value;
        break;
    case 0x114: //Receive Next Counter Register
        s->periph_rncr=value;
        break;
    case 0x118: //Transmit Next Pointer Register
        s->periph_tnpr=value;
        break;
    case 0x11C: //Transmit Next Counter Register
        s->periph_tncr=value;
        break;
    case 0x120: //Transfer Control Register
        if(value&(1<<1)){
            s->periph_ptsr&=~0x1;
        }else{
            if(value&(1<<0)){
                s->periph_ptsr|=0x1;
            }
        }

        if(value&(1<<9)){
            s->periph_ptsr&=~(1<<8);
        }else{
            if(value&(1<<8)){
                s->periph_ptsr|=(1<<8);
                void * buf = malloc(sizeof(uint8_t)*1024);
                cpu_physical_memory_read(s->periph_tpr,buf,s->periph_tcr);
                //qemu_chr_fe_open(&s->chr);
                qemu_chr_fe_write(&s->chr,buf,s->periph_tcr);
                //qemu_chr_fe_close(&s->chr);
            }
        }

        s->periph_ptcr=value;
        break;
    default:
        fprintf(stderr, "at91usart_write: Bad offset %x\n", (int)offset);
    }
}

static int at91usart_can_receive(void *opaque)
{
    at91usart_state *s = (at91usart_state *)opaque;
    if (s->cr&CR_RXEN)
        return 1;
    else
        return 0;
}


static void at91usart_receive(void *opaque, const uint8_t *buf, int size)
{
    at91usart_state *s = (at91usart_state *)opaque;

    if(s->periph_ptcr>0){
        //DMA-Ready
            cpu_physical_memory_write (s->periph_rpr, buf, 1);
            s->periph_rcr-=1;
            s->periph_rpr+=1;
        }else{
            if(*buf == 195){ //Necessary for special characters

            }else{
                if(s->cr&CR_RXEN){
                        s->rhr=*buf;
                        if(s->csr&CSR_RXRDY) s->csr |= CSR_OVRE;
                        s->csr |= CSR_RXRDY;
                    at91usart_update(s,0);
                    }
            }
        }
}

static void at91usart_event(void *opaque, int event)
{

}

static const MemoryRegionOps at91usart_ops = {
    .read = at91usart_read,
    .write = at91usart_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_at91usart = {
    .name = "at91usart",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32(cr, at91usart_state),
        VMSTATE_UINT32(mr, at91usart_state),
        VMSTATE_UINT32(imr, at91usart_state),
        VMSTATE_UINT32(csr, at91usart_state),
        VMSTATE_UINT32(rhr, at91usart_state),
        VMSTATE_END_OF_LIST()
    }
};

static int at91usart_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    at91usart_state *s = AT91USART(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &at91usart_ops, s, "at91usart", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    s->cr = 0x0;
    s->csr = (1<<4); //Always set the End of Transfer signal to active
    qemu_chr_fe_set_handlers(&s->chr, at91usart_can_receive, at91usart_receive,
                              at91usart_event, NULL, s, NULL, true);
    vmstate_register(dev, -1, &vmstate_at91usart, s);
    return 0;
}

static Property at91usart_properties[] = {
    DEFINE_PROP_CHR("chardev", at91usart_state, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void at91usart_arm_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    sdc->init = at91usart_init;
    dc->props = at91usart_properties;
}

static const TypeInfo at91usart_info = {
    .name          = TYPE_AT91USART,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(at91usart_state),
    .class_init    = at91usart_arm_class_init,
};



static void at91usart_register_types(void)
{
    type_register_static(&at91usart_info);
}

type_init(at91usart_register_types)
