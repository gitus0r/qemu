/*
 * AT91 Interrupt Logic OR
 *
 * Copyright (c) 2009 Filip Navara
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"

#define TYPE_AT91_INTOR "at91_intor"
#define AT91_INTOR(obj) OBJECT_CHECK(at91_intor_state, (obj), TYPE_AT91_INTOR)

typedef struct {
    SysBusDevice busdev;
    qemu_irq parent_irq;
    uint32_t sources;
} at91_intor_state;

static void at91_intor_set_irq(void *opaque, int irq, int level)
{
    at91_intor_state *s = opaque;

    if (level) {
        s->sources |= 1 << irq;
    } else {
        s->sources &= ~(1 << irq);
    }
    qemu_set_irq(s->parent_irq, !!s->sources);
}

static void at91_intor_reset(DeviceState *dev)
{
    at91_intor_state *s = AT91_INTOR(dev);

    s->sources = 0;
}

static void at91_intor_init(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    at91_intor_state *s = AT91_INTOR(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    qdev_init_gpio_in(dev, at91_intor_set_irq, 32);
    sysbus_init_irq(sbd, &s->parent_irq);

    at91_intor_reset(dev);    // FIXME: Is this necessary?
}

static const VMStateDescription vmstate_at91_intor = {
    .name = "at91_intor",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(sources, at91_intor_state),
        VMSTATE_END_OF_LIST()
    }
};

static void at91_intor_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    
    dc->user_creatable = false; /* FIXME explain why */
    dc->reset = at91_intor_reset;
    dc->vmsd = &vmstate_at91_intor;
}

static const TypeInfo at91_intor_info = {
    .name = TYPE_AT91_INTOR,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(at91_intor_state),
    .instance_init = at91_intor_init,
    .class_init = at91_intor_class_init,
};

static void at91_intor_register_types(void)
{
    type_register_static(&at91_intor_info);
}

type_init(at91_intor_register_types)

