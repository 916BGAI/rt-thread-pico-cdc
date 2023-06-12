/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2021-01-28     flybreak       first version
 * 2023-01-22     rose_man       add RT_USING_SMP
 */

#include <rthw.h>
#include <rtthread.h>

#include <stdio.h>

#include "board.h"
#include "hardware/structs/systick.h"
#include "pico/stdlib.h"
#include "pico/stdio/driver.h"
#include "pico/stdio_usb.h"

#define PLL_SYS_KHZ (133 * 1000)

void isr_systick(void)
{
    /* enter interrupt */
#ifndef RT_USING_SMP
    rt_interrupt_enter();
#endif

    rt_tick_increase();

    /* leave interrupt */
#ifndef RT_USING_SMP
    rt_interrupt_leave();
#endif
}

uint32_t systick_config(uint32_t ticks)
{
  if ((ticks - 1UL) > M0PLUS_SYST_RVR_RELOAD_BITS)
  {
    return (1UL);                                                   /* Reload value impossible */
  }

  mpu_hw->rvr    = (uint32_t)(ticks - 1UL);                         /* set reload register */
  mpu_hw->csr  = M0PLUS_SYST_CSR_CLKSOURCE_BITS |
                   M0PLUS_SYST_CSR_TICKINT_BITS   |
                   M0PLUS_SYST_CSR_ENABLE_BITS;                     /* Enable SysTick IRQ and SysTick Timer */
  return (0UL);                                                     /* Function successful */
}

void rt_hw_board_init()
{
    set_sys_clock_khz(PLL_SYS_KHZ, true);

#ifdef RT_USING_HEAP
    rt_system_heap_init(HEAP_BEGIN, HEAP_END);
#endif

#ifdef RT_USING_SMP
    extern rt_hw_spinlock_t _cpus_lock;
    rt_hw_spin_lock_init(&_cpus_lock);
#endif

    alarm_pool_init_default();

    // Start and end points of the constructor list,
    // defined by the linker script.
    extern void (*__init_array_start)();
    extern void (*__init_array_end)();

    // Call each function in the list.
    // We have to take the address of the symbols, as __init_array_start *is*
    // the first function pointer, not the address of it.
    for (void (**p)() = &__init_array_start; p < &__init_array_end; ++p) {
        (*p)();
    }

    /* Configure the SysTick */
    systick_config(clock_get_hz(clk_sys)/RT_TICK_PER_SECOND);

#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif

#ifdef RT_USING_SERIAL
    stdio_init_all();
    rt_hw_uart_init();
#endif

#ifdef RT_USING_CONSOLE
   rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
#endif
}

void rt_hw_console_output(const char *str)
{
    rt_size_t i = 0, size = 0;

    size = rt_strlen(str);
    for (i = 0; i < size; i++)
    {
        if (*(str + i) == '\n')
        {
            stdio_usb.out_chars("\r", 1);
        }
        stdio_usb.out_chars(str + i, 1);
    }
}

char rt_hw_console_getchar(void)
{
    char ch[1] = {-1};
    int rc = PICO_ERROR_NO_DATA;
    rc = stdio_usb.in_chars(ch, 1);

    if (rc == PICO_ERROR_NO_DATA)
    {
        rt_thread_mdelay(10);
    }
    
    return ch[0];
}

void rt_hw_us_delay(rt_uint32_t us)
{
    rt_uint32_t ticks;
    rt_uint32_t told, tnow, tcnt = 0;
    rt_uint32_t reload = mpu_hw->rvr;

    /* 获得延时经过的 tick 数 */
    ticks = us * reload / (1000000 / RT_TICK_PER_SECOND);
    /* 获得当前时间 */
    told = mpu_hw->cvr;
    while (1)
    {
        /* 循环获得当前时间，直到达到指定的时间后退出循环 */
        tnow = mpu_hw->cvr;
        if (tnow != told)
        {
            if (tnow < told)
            {
                tcnt += told - tnow;
            }
            else
            {
                tcnt += reload - tnow + told;
            }
            told = tnow;
            if (tcnt >= ticks)
            {
                break;
            }
        }
    }
}
