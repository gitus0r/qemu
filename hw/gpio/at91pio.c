/**
 * Portux920T PIO a,b and c
 */

#include "qemu/osdep.h"
#include "migration/vmstate.h"
#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#define TYPE_AT91PIO "at91pio"
#define AT91PIO(obj) OBJECT_CHECK(at91pio_state, (obj), TYPE_AT91PIO)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    qemu_irq irq;
    //PIO A
    uint32_t pioa_psr;
    uint32_t pioa_osr;
    uint32_t pioa_odsr;
    //PIO B
    uint32_t piob_psr;
    uint32_t piob_osr;
    uint32_t piob_odsr;
    //PIO C
    uint32_t pioc_psr;
    uint32_t pioc_osr;
    uint32_t pioc_odsr;

    //Display
    uint32_t display_pos;
    uint8_t display[128];

} at91pio_state;

//KBS-Special-Telnet-Client
Chardev *char_kbs;
extern int pio_telnet;

static uint64_t at91pio_read(void *opaque, hwaddr offset, unsigned size)
{
    at91pio_state *s = (at91pio_state *)opaque;
    if(offset<0x200){
        switch (offset) {
            //PIO A
            case 0x8:
                return s->pioa_psr;
            case 0x18:
                return s->pioa_osr;
            case 0x38:
                return s->pioa_odsr;
            default:
                fprintf(stderr, "PIO A Read: Bad Offset %x (returning zero)\n", (int)offset);
                return 0;
        }
    }else if(offset<0x400){
        switch (offset) {
            //PIO B
            case 0x208:
                return s->piob_psr;
            case 0x218:
                return s->piob_osr;
            case 0x238:
                return s->piob_odsr;
            default:
                fprintf(stderr, "PIO B Read: Bad Offset %x (returning zero)\n", (int)(offset-0x200));
                return 0;
        }
    }else{
        switch (offset) {
            //PIO C
            case 0x408:
                return s->pioc_psr;
            case 0x418:
                return s->pioc_osr;
            case 0x438:
                return s->pioc_odsr;
            default:
                fprintf(stderr, "PIO C Read: Bad Offset %x (returning zero)\n", (int)(offset-0x400));
                return 0;
        }
    }
        return 0;
}

static void at91pio_write(void *opaque, hwaddr offset, uint64_t value, unsigned size)
{
    //printf("\nPIO WRITE: %X | %X",offset, value);
    //fflush(0);
    at91pio_state *s = (at91pio_state *)opaque;

    uint32_t pioc_ctrl, piob_ctrl;

    int i = 0;
    int j = 0;//Counter

    //PIO A
    if(offset<0x200){
        switch (offset) {
            case 0: //pio enable register
                s->pioa_psr = s->pioa_psr|value; //der value wird ins pio status register eingetragen
                break;
            case 4: //pio disable register
                s->pioa_psr = s->pioa_psr&(~value); //der value wird bitwise negiert und mit psr verunded
                break;
            case 0x10: //output enable register
                s->pioa_osr = s->pioa_osr|value;
                break;
            case 0x14: //output disable register
                s->pioa_osr = s->pioa_psr&(~value);
                break;
            case 0x30: //set output data register
                s->pioa_odsr = s->pioa_odsr|value;
                break;
            case 0x34: //clear output data register
                s->pioa_odsr = s->pioa_odsr&(~value);
                break;
            case 0x70: //For Flock-OS, does nothing
                break;
            case 0x100: //This register is originally never used so we use it for executing commands from the at91display.c
                //PIOC PSR and OSR Enabled, ReadWrite, RegionSelect and ChipSelect
                if((((s->pioc_psr) & (s->pioc_osr))&0x1C00)==0x1C00){
                    //Is ODSR ChipSelect set to low (0)
                    if((s->pioc_odsr&(1<<12))==0){
                        //Command..
                        if((s->pioc_odsr&(3<<10))==0){
                        //printf("\nODSR 10 Bit: %X",(s->pioc_odsr&(3<<10)));
                            while((value>>i)>0 && i<8){
                                i++;
                            }
                            //printf("\n I: %d",(i));
                            //fflush(0);
                            switch(i){
                                //Clear Display
                                case 0x1:
                                    for(j=0; j<64; j++){
                                        s->display[j]='\0';
                                    }
                                    s->display_pos=0;
                                    break;
                                //Return Home (do not delete)
                                case 0x2:
                                    //TODO Beim Shiften wird der alte Zustand wieder hergestellt
                                    s->display_pos=0;
                                    break;
                                //Entry Mode Set
                                case 0x3:
                                //    printf("Entry Set: %X",value);
                                    //TODO: Variable kann hoch oder runtergezählt werden, je nach Laufrichtung
                                    //TODO: Shiften?
                                    break;
                                //Display On Off (Cursor Blinks and stuff)
                                case 0x4:
                                    //printf("Display ON - Value: %X",value);
                                    break;

                                //Cursor or Display Shift
                                case 0x5:
                                    break;
                                //Function Set
                                case 0x6:
                                    //8 or 4 Bit -- Not fully implented
                                    break;

                                //Set CGRAM Address
                                case 0x7:
                                    break;

                                //Chose Row
                                case 0x8:
                                    //printf("\nROW Value: 0x%X\n",(value&0x7F));
                                    s->display_pos=(value&0x7F);
                                    break;
                                default:
                                    break;
                            }

                        // or Character
                        }else if((s->pioc_odsr&(1<<10))==0x400){
                            if(s->display_pos>=128){
                                s->display_pos=0;
                            }
                            s->display[s->display_pos]=(char)value;
                            s->display_pos++;
                        }//Reading is not implemented

                    }
                }

                break;
            default:
                fprintf(stderr, "PIO A Write: Bad offset %x\n", (int)offset);
        }
    }else if(offset<0x400){
        switch (offset) {
                case 0x200: //pio enable register

                    s->piob_psr = s->piob_psr|value; //der value wird ins pio status register eingetragen
                    break;
                case 0x204: //pio disable register
                    s->piob_psr = s->piob_psr&(~value); //der value wird bitwise negiert und mit psr verunded
                    break;
                case 0x210: //output enable register
                    s->piob_osr = s->piob_osr|value;
                    break;
                case 0x214: //output disable register
                    s->piob_osr = s->piob_psr&(~value);
                    break;
                case 0x230: //set output data register
                    s->piob_odsr = s->piob_odsr|value;
                    break;
                case 0x234: //clear output data register
                    s->piob_odsr = s->piob_odsr&(~value);
                    break;
                case 0x270: //TODO Micha? Sind das Lampen? Pio ASR
                    break;
                default:
                    fprintf(stderr, "PIO B Write: Bad offset %x\n", (int)(offset-0x200));
                }
    }else{
        switch (offset) {
            case 0x400: //pio enable register
                s->pioc_psr = s->pioc_psr|value; //der value wird ins pio status register eingetragen
                break;
            case 0x404: //pio disable register
                s->pioc_psr = s->pioc_psr&(~value); //der value wird bitwise negiert und mit psr verunded
                break;
            case 0x410: //output enable register
                s->pioc_osr = s->pioc_osr|value;
                break;
            case 0x414: //output disable register
                s->pioc_osr = s->pioc_psr&(~value);
                break;
            case 0x430: //set output data register
                s->pioc_odsr = s->pioc_odsr|value;
                break;
            case 0x434: //clear output data register
                s->pioc_odsr = s->pioc_odsr&(~value);
                break;
            case 0x470: //TODO Micha? Sind das Lampen? Pio ASR
                break;
            default:
                fprintf(stderr, "PIO C Write: Bad offset %x\n", (int)(offset-0x400));
            }
    }
            pioc_ctrl=(s->pioc_odsr & s->pioc_osr & s->pioc_psr);
            piob_ctrl=(s->piob_odsr & s->piob_osr & s->piob_psr);

            //TODO: Nur wenn wirklich etwas neugezeichnet werden soll
            if(pio_telnet){
                // Clear Telnet Display
                qemu_chr_fe_printf(char_kbs->be,"\033[2J\033[1;1H");

                /**
                 * LED CONTROL
                 */
                char yellow_led[]="\033[1;43m\033[1;30m \033[0m";
                if((piob_ctrl&0x8000000)!=0x8000000){
                    strcpy(yellow_led, "\033[1;40m\033[1;33m_\033[0m");
                }

                char green_led[]="\033[1;42m\033[1;30m \033[0m";
                if((pioc_ctrl&2)!=2){
                    strcpy(green_led, "\033[1;40m\033[1;32m_\033[0m");
                }

                char red_led[]="\033[1;41m\033[1;30m \033[0m";
                if((pioc_ctrl&1)!=1){
                    strcpy(red_led, "\033[1;40m\033[1;31m_\033[0m");
                }
                qemu_chr_fe_printf(char_kbs->be,"\r\nLED: %s%s%s",yellow_led, green_led, red_led);

                /**
                 * DISPLAY
                 */

                qemu_chr_fe_printf(char_kbs->be,"\r\n*** DISPLAY ****\r\n");
                int row_starts[]={0,64,16,80};
                for(i=0;i<4;i++){
                    /*for(j=0;j<16;j++){
                        qemu_chr_fe_printf(char_kbs->be,"%c",s->display[j+row_starts[i]]);
                    }*/
                    qemu_chr_fe_write(char_kbs->be,&(s->display[row_starts[i]]),16);
                    qemu_chr_fe_printf(char_kbs->be,"\r\n");
                }
                qemu_chr_fe_printf(char_kbs->be,"*END OF DISPLAY*\r\n");
            }
}


static const MemoryRegionOps at91pio_ops = {
    .read = at91pio_read,
    .write = at91pio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_at91pio = {
    .name = "at91pio",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32(pioa_psr, at91pio_state),
        VMSTATE_UINT32(pioa_osr, at91pio_state),
        VMSTATE_UINT32(pioa_odsr, at91pio_state),
        VMSTATE_UINT32(piob_psr, at91pio_state),
        VMSTATE_UINT32(piob_osr, at91pio_state),
        VMSTATE_UINT32(piob_odsr, at91pio_state),
        VMSTATE_UINT32(pioc_psr, at91pio_state),
        VMSTATE_UINT32(pioc_osr, at91pio_state),
        VMSTATE_UINT32(pioc_odsr, at91pio_state),
        VMSTATE_UINT32(display_pos, at91pio_state),
        VMSTATE_UINT8_ARRAY(display, at91pio_state, 128),
        VMSTATE_END_OF_LIST()
    }
};

static void at91pio_init(Object *obj)
{
    at91pio_state *s = AT91PIO(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    
    memory_region_init_io(&s->iomem, obj, &at91pio_ops, s, "at91pio", 0x5FF);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    
    for (int i=0; i<128; i++){
        s->display[i]='\0';
    }
    s->display_pos=0;
    //Telnet-Client for LCD and LED Output. Port 44444
    if(pio_telnet!=0){
        char_kbs = qemu_chr_new("kbs_telnet", "telnet:localhost:44444,server", NULL);
    }
}

static void at91pio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    
    dc->vmsd = &vmstate_at91pio;
}

static const TypeInfo at91pio_info = {
    .name          = TYPE_AT91PIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(at91pio_state),
    .instance_init = at91pio_init,
    .class_init    = at91pio_class_init,
};

static void at91pio_register_types(void)
{
    //Achtung! Hier können auch mehrere verschiedene TypeInfos angegeben werden!
    type_register_static(&at91pio_info);
}



type_init(at91pio_register_types)
