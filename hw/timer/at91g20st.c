/*
 * Timer for the portuxg20
 * All timers!
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "qemu/cutils.h"

#define TYPE_AT91G20ST "at91g20st"
#define AT91G20ST(obj) OBJECT_CHECK(at91g20st_state, (obj), TYPE_AT91G20ST)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    QEMUTimer *pi_timer;
    QEMUTimer *wd_timer;
    QEMUTimer *rt_timer;
    QEMUTimer *rt_inc_timer;
    uint64_t startup_real_time;
    uint64_t time_last_change;
    qemu_irq irq;
    uint32_t tick_offset;

    //RealTimeTimer
    uint32_t rtt_mr;
    uint32_t rtt_ar;
    uint32_t rtt_vr;
    uint32_t rtt_sr;

    //PeriodicIntervallTimer
    uint32_t pit_mr;
    uint32_t pit_sr;
    uint32_t pit_pivr;
    uint32_t pit_piir;

    //WatchdogTimer
    uint32_t wdt_cr;
    uint32_t wdt_mr;
    uint32_t wdt_sr;

} at91g20st_state;

static const VMStateDescription vmstate_at91g20st = {
    .name = "at91g20st",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(tick_offset, at91g20st_state),

        VMSTATE_UINT32(rtt_mr, at91g20st_state),
        VMSTATE_UINT32(rtt_ar, at91g20st_state),
        VMSTATE_UINT32(rtt_vr, at91g20st_state),
        VMSTATE_UINT32(rtt_sr, at91g20st_state),

        VMSTATE_UINT32(pit_mr, at91g20st_state),
        VMSTATE_UINT32(pit_sr, at91g20st_state),
        VMSTATE_UINT32(pit_pivr, at91g20st_state),
        VMSTATE_UINT32(pit_piir, at91g20st_state),

        VMSTATE_UINT32(wdt_cr, at91g20st_state),
        VMSTATE_UINT32(wdt_mr, at91g20st_state),
        VMSTATE_UINT32(wdt_sr, at91g20st_state),

        VMSTATE_END_OF_LIST()
    }
};

static void periodic_set_alarm(at91g20st_state *s);

/*
 * TODO hz is int. but hz can be 0.5. That's not good
 */
static void realtime_handler(void * opaque)
{
    printf("\n****RealTimeHAndler****");
    fflush(0);
    at91g20st_state *s = (at91g20st_state *)opaque;
    s->rtt_sr |=1;
    qemu_set_irq(s->irq, 1);
}

static void realtime_set_alarm(at91g20st_state *s)
{
    printf("\n****RealTimeSetAlarm****");
    fflush(0);
    uint64_t prescaler = s->rtt_mr & 0xFFFF;
    if(prescaler == 0) //eq 2^16
        prescaler = 0x10000;
    uint32_t hz = 0x8000/prescaler;
    uint32_t diff = s->rtt_ar - s->rtt_vr;
    if(diff>0){
        timer_mod(s->rt_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME)+(1000000*diff)/hz);
    }
}

static void realtime_increment_set_alarm(at91g20st_state *s)
{
    printf("\n****IncRealTimeSetAlarm****");
    fflush(0);
    uint64_t prescaler = s->rtt_mr & 0xFFFF;
    if(prescaler == 0) //eq 2^16
        prescaler = 0x10000;
    uint32_t hz = 0x8000/prescaler;
    timer_mod(s->rt_inc_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME)+(1000000/hz));
}

static void realtime_increment_handler(void *opaque)
{
    printf("\n****IncRealTimeHandler****");
    fflush(0);
    at91g20st_state *s = (at91g20st_state *)opaque;
    //s->rtt_sr |= 2;
    if(s->rtt_mr & (1<<17)){
        qemu_set_irq(s->irq, 1);
        realtime_increment_set_alarm(s);
    }
}


static void periodic_handler(void * opaque)
{
    //printf("\n--periodic Handler Aufgerufen");
    //fflush(0);
    at91g20st_state *s = (at91g20st_state *)opaque;
    int temp = s->pit_pivr>>20;
    temp++;
    s->pit_pivr=temp<<20|(s->pit_pivr&0xFFFFF);
    s->pit_sr=0x1;
    if(s->pit_mr&(1<<24)){ //PITEN Register set
        if(s->pit_mr&(1<<25)){ //PITIEN Register set
            qemu_set_irq(s->irq, 1);
        }
        periodic_set_alarm(s);
    }
}

static void periodic_set_alarm(at91g20st_state *s)
{

    int64_t now;
    uint32_t ticks;

    now = qemu_clock_get_ns(QEMU_CLOCK_REALTIME);

    ticks = ((s->pit_mr&0xFFFFF)/8250); //Ticks in Milliseconds
    timer_mod(s->pi_timer, now + ticks*1000000);
}

static void wd_set_alarm(at91g20st_state *s)
{
        int64_t now;
        uint32_t ticks;

        now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

        ticks = (((s->wdt_mr &0xFFF))/32768) + now / NANOSECONDS_PER_SECOND;
        timer_mod(s->wd_timer, now + ticks*NANOSECONDS_PER_SECOND/1000);
}

static void wd_interrupt(void * opaque)
{
    at91g20st_state *s = (at91g20st_state *)opaque;
    s->wdt_sr = 0x1;
    if(s->wdt_mr&0x1000){
    qemu_set_irq(s->irq,1);
    }
    if(s->wdt_mr&0x2000){
    wd_set_alarm(s);
    }
}


static uint64_t at91g20st_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    at91g20st_state *s = (at91g20st_state *)opaque;
    uint32_t temp, hz;
    uint64_t prescaler;
    if(offset < 0x10){
        prescaler = s->rtt_mr & 0xFFFF;
        if(prescaler == 0) //eq 2^16
            prescaler = 0x10000;
        hz = 0x8000/prescaler;
        temp = s->rtt_vr;
        uint64_t now = qemu_clock_get_ns(QEMU_CLOCK_REALTIME);
        s->rtt_vr+=(now -  s->time_last_change)/(hz*1000000000);
        if(temp<s->rtt_vr){
            s->rtt_sr|=2;
            s->time_last_change = now;
        }
    }
    switch (offset) {
    //Real Time Timer
    case 0x0:
        return s->rtt_mr;
    case 0x04:
        return s->rtt_ar;
    case 0x08:
        return s->rtt_vr;
    case 0x0C:
        temp = s->rtt_sr;
        s->rtt_sr &= ~0x3;
        return temp;
    //Periodic Interval Timer
    case 0x10:
        return s->pit_mr;
    case 0x14:
        return s->pit_sr;
    case 0x18:
        s->pit_sr &= ~0x1;
        temp = s->pit_pivr;
        s->pit_pivr &= 0xFFFF;
        return temp;
    case 0x1C:
        temp = s->pit_pivr;
        s->pit_pivr &= 0xFFFF;
        s->pit_sr =0x0;
        return temp;

    case 0x24:
        //TODO: READ ONCE!!
        return s->wdt_mr;
    case 0x28:
        return s->wdt_sr;
    default:
        fprintf(stderr, "at91g20st_read: Bad offset %x (returning zero)\n", (int)offset);
        return 0;
    }
}

static void at91g20st_write(void * opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    at91g20st_state *s = (at91g20st_state *)opaque;
    uint64_t tmp, prescaler;
    uint32_t hz;

    switch (offset) {
    //RealTime
    case 0x0: //Mode Register
        prescaler = s->rtt_mr & 0xFFFF;
        if(prescaler == 0) //eq 2^16
            prescaler = 0x10000;
        hz = 0x8000/prescaler;
        tmp=qemu_clock_get_ns(QEMU_CLOCK_REALTIME);
        s->rtt_vr+=(tmp -  s->time_last_change)/(hz*1000000);
        s->time_last_change=tmp;
        s->rtt_mr = value;
        if(value & (1<<18)) { //Reset and (maybe) restart clock
            s->startup_real_time = tmp;
            s->rtt_vr=0; // XXX this wasn't guarded by if, now is -- correct?
        }
        if(value & (1<<17))
            realtime_increment_set_alarm(s);
        if(value & (1<<16))
            realtime_set_alarm(s);
        break;
    case 0x4: // Alarm Register
        s->rtt_ar = value;
        realtime_set_alarm(s);
        break;
    // Periodic
    case 0x10: // Mode Register
        s->pit_mr = value;
        periodic_set_alarm(s);
        break;
    case 0x20:
        //0-Bit=1: Restarte den Timer
        //TODO: value >> 24 = 0xA5 sonst kein schreiben
        if((value&0x1)&&((value>>24)==0xA5)){
            wd_set_alarm(s);
        }
        break;
    case 0x24:
        //TODO Write ONCE
        //Writing this register starts the timer
        //TODO 13. Bit = 1 : Restarte den Timer wenn abgelaufen
        //TODO 12. Bit = 1 : Interrupt enabled on underflow
        s->wdt_mr=value;
        wd_set_alarm(s);
        break;
    default:
        fprintf(stderr, "at91g20st_write: Bad offset %x\n", (int)offset);
    }
}

static const MemoryRegionOps at91g20st_ops = {
    .read = at91g20st_read,
    .write = at91g20st_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int at91g20st_init(SysBusDevice *dev)
{
    at91g20st_state *s = AT91G20ST(dev);
    struct tm tm;

    memory_region_init_io(&s->iomem, OBJECT(s), &at91g20st_ops, s, "at91g20st", 0x30);
    sysbus_init_mmio(dev, &s->iomem);

    sysbus_init_irq(dev, &s->irq);

    s->startup_real_time = s->time_last_change = qemu_clock_get_ns(QEMU_CLOCK_REALTIME); //Save the current timestamp

    // Periodic Intervall Timer
    qemu_get_timedate(&tm, 0);
    s->tick_offset = mktimegm(&tm);

    s->pi_timer = timer_new_ns(QEMU_CLOCK_REALTIME, periodic_handler, s);
    s->wd_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, wd_interrupt, s);
    s->rt_timer = timer_new_ns(QEMU_CLOCK_REALTIME, realtime_handler, s);
    s->rt_inc_timer = timer_new_ns(QEMU_CLOCK_REALTIME, realtime_increment_handler, s);

    //RealTimeTimer
    s->rtt_mr=0x8000;
    s->rtt_ar=0xFFFFFFFF;
    s->rtt_vr=0x0;
    s->rtt_sr=0x0;

    //PeriodicIntervallTimer
    s->pit_mr=0xFFFFF;
    s->pit_sr=0x0;
    s->pit_pivr=0x0;
    s->pit_piir=0x0;

    //WatchdogTimer
    s->wdt_cr=0x0;
    s->wdt_mr=0x3FFF2FFF;
    s->wdt_sr=0x0;

    return 0;
}

static void at91g20st_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = at91g20st_init;
    dc->user_creatable = false; /* FIXME explain why */
    dc->vmsd = &vmstate_at91g20st;
}

static const TypeInfo at91g20st_info = {
    .name          = TYPE_AT91G20ST,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(at91g20st_state),
    .class_init    = at91g20st_class_init,
};

static void at91g20st_register_types(void)
{
    type_register_static(&at91g20st_info);
}

type_init(at91g20st_register_types)
