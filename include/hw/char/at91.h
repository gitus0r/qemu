/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_AT91_H
#define HW_AT91_H

#include "hw/qdev-properties.h"

static inline DeviceState *at91dbgu_create(hwaddr addr,
                                        qemu_irq irq,
                                        Chardev *chr)
{
    DeviceState *dev;
    SysBusDevice *s;

    dev = qdev_new("at91dbgu");
    s = SYS_BUS_DEVICE(dev);
    qdev_prop_set_chr(dev, "chardev", chr);
    sysbus_mmio_map(s, 0, addr);
    sysbus_connect_irq(s, 0, irq);

    return dev;
}

static inline DeviceState *at91usart_create(hwaddr addr,
                                                 qemu_irq irq,
                                                 Chardev *chr)
{
    DeviceState *dev;
    SysBusDevice *s;

    dev = qdev_new("at91usart");
    s = SYS_BUS_DEVICE(dev);
    qdev_prop_set_chr(dev, "chardev", chr);
    sysbus_mmio_map(s, 0, addr);
    sysbus_connect_irq(s, 0, irq);

    return dev;
}

#endif
