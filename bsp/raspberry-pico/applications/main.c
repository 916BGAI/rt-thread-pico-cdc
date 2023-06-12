#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>
#include "board.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define MISO_PIN 0
#define MOSI_PIN 3
#define SCK_PIN 2
#define CS_PIN 1

static rt_thread_t tid1 = RT_NULL;
static rt_thread_t tid2 = RT_NULL;

void spi_start(void)
{
    gpio_put(CS_PIN, false);
}

void spi_stop(void)
{
    gpio_put(CS_PIN, true);
}

rt_uint8_t spi_swap_byte(rt_uint8_t send_data)
{
    rt_uint16_t i;
    rt_uint8_t read_data = 0x00;

    for(i=0; i<8; i++)
    {
        if(send_data & (0x80 >> i))
        {
            gpio_put(MOSI_PIN, true);
        }
        else
        {
            gpio_put(MOSI_PIN, false);
        }
        gpio_put(SCK_PIN, true);
        if(gpio_get(MISO_PIN))
        {
            read_data |= (0x80 >> i);
        }
        gpio_put(SCK_PIN, false);
    }

    return read_data;
}

void w25q128_read_id(rt_uint8_t *MID, rt_uint16_t *DID)
{
    spi_start();
    spi_swap_byte(0x9F);
    *MID = spi_swap_byte(0xFF);
    *DID = spi_swap_byte(0xFF);
    *DID <<= 8;
    *DID |= spi_swap_byte(0xFF);
    spi_stop();
}

static void thread1_entry(void *parameter)
{
    rt_pin_mode(25, PIN_MODE_OUTPUT);

    gpio_init(MISO_PIN);
    gpio_set_dir(MISO_PIN, false);
    gpio_pull_up(MISO_PIN);

    gpio_init(MOSI_PIN);
    gpio_set_dir(MOSI_PIN, true);
    gpio_put(MOSI_PIN, true);

    gpio_init(SCK_PIN);
    gpio_set_dir(SCK_PIN, true);
    gpio_put(SCK_PIN, true);

    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, true);
    gpio_put(CS_PIN, true);

    while (1)
    {
        rt_pin_write(25, 1);
        rt_thread_mdelay(1000);
        rt_pin_write(25, 0);
        rt_thread_mdelay(1000);
    }
}

static void thread2_entry(void *parameter)
{
    rt_uint8_t MID;
    rt_uint16_t DID;
    w25q128_read_id(&MID, &DID);
    rt_kprintf("MID is %#x\r\nDID is %#x\n", MID, DID);
}

int task(void)
{
    tid2 = rt_thread_create("thread2",thread2_entry, RT_NULL, 1024, 10, 5);

    if (tid2 != RT_NULL)
        rt_thread_startup(tid2);

    return 0;
}

MSH_CMD_EXPORT(task, task);

int main(void)
{
    tid1 = rt_thread_create("thread1",thread1_entry, RT_NULL, 1024, 11, 5);
    if (tid1 != RT_NULL)
        rt_thread_startup(tid1);

    return 0;
}
