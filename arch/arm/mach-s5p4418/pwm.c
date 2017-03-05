/*
 * Charleye <wangkartx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * Refer to arch/arm/cpu/armv7/s5p-common/pwm.c
 *
 */

#include <common.h>
#include <pwm.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/pwm.h>
#include <asm/arch/clk.h>

int pwm_enable(int pwm_id)
{
    const struct s5p4418_timer *pwm =
            (struct s5p4418_timer *)s5p4418_get_base_timer();
    unsigned long tcon;

    tcon = readl(&pwm->tcon);
    tcon |= TCON_START(pwm_id);

    writel(tcon, &pwm->tcon);

    return 0;
}

void pwm_disable(int pwm_id)
{
    const struct s5p4418_timer *pwm =
        (struct s5p4418_timer *)s5p4418_get_base_timer();
    unsigned long tcon;

    tcon = readl(&pwm->tcon);
    tcon &= ~TCON_START(pwm_id);

    writel(tcon, &pwm->tcon);
}

static unsigned long pwm_calc_tin(int pwd_id, unsigned long freq)
{
    unsigned long tin_parent_rate;
    unsigned int div;

    tin_parent_rate = get_pwm_clk();

    for (div = 2; div <= 16; div *= 2) {
        if ((tin_parent_rate / (div << 16)) < freq)
            return tin_parent_rate / div;
    }

    return tin_parent_rate / 16;
}

#define NS_IN_SEC   1000000000UL

int pwm_config(int pwm_id, int duty_ns, int period_ns)
{
    const struct s5p4418_timer *pwm =
        (struct s5p4418_timer *)s5p4418_get_base_timer();
    unsigned int offset;
    unsigned long tin_rate;
    unsigned long tin_ns;
    unsigned long frequency;
    unsigned long tcon;
    unsigned long tcnt;
    unsigned long tcmp;

    if (period_ns > NS_IN_SEC || duty_ns > NS_IN_SEC || period_ns == 0)
        return -ERANGE;

    if (duty_ns > period_ns)
        return -EINVAL;

    frequency = NS_IN_SEC / period_ns;

    tin_rate = pwm_calc_tin(pwm_id, frequency);

    tin_ns = NS_IN_SEC / tin_rate;
    tcnt = period_ns / tin_ns;

    tcmp = duty_ns / tin_ns;
    tcmp = tcnt - tcmp;

    offset = pwm_id * 3;
    if (pwm_id < 4) {
        writel(tcnt, &pwm->tcntb0 + offset);
        writel(tcmp, &pwm->tcmpb0 + offset);
    }

    tcon = readl(&pwm->tcon);
    tcon |= TCON_UPDATE(pwm_id);
    if (pwm_id < 4)
        tcon |= TCON_AUTO_RELOAD(pwm_id);
    else
        tcon |= TCON4_AUTO_RELOAD;
    writel(tcon, &pwm->tcon);

    tcon &= ~TCON_UPDATE(pwm_id);
    writel(tcon, &pwm->tcon);

    return 0;
}

int pwm_init(int pwm_id, int div, int invert)
{
    u32 val;
    const struct s5p4418_timer *pwm =
        (struct s5p4418_timer *)s5p4418_get_base_timer();
    unsigned long ticks_per_period;
    unsigned int offset, prescaler;

    val = readl(&pwm->tcfg0);
    if (pwm_id < 2) {
        prescaler = PRESCALER_0;
        val &= ~0xff;
        val |= (prescaler & 0xff);
    } else {
        prescaler = PRESCALER_1;
        val &= ~(0xff << 8);
        val |= (prescaler & 0xff) << 8;
    }
    writel(val, &pwm->tcfg0);
    val = readl(&pwm->tcfg1);
    val &= ~(0xf << MUX_DIV_SHIFT(pwm_id));
    val |= (div & 0xf) << MUX_DIV_SHIFT(pwm_id);
    writel(val, &pwm->tcfg1);

    if (pwm_id == 4) {
        ticks_per_period = -1UL;
    } else {
        const unsigned long pwm_hz = 1000;
        unsigned long timer_rate_hz = get_pwm_clk() /
            ((prescaler + 1) * (1 << div));

        ticks_per_period = timer_rate_hz / pwm_hz;
    }

    offset = pwm_id * 3;
    writel(ticks_per_period, &pwm->tcntb0 + offset);

    val = readl(&pwm->tcon) & ~(0xf << TCON_OFFSET(pwm_id));
    if (invert && (pwm_id < 4))
        val |= TCON_INVERTER(pwm_id);
    writel(val, &pwm->tcon);

    pwm_enable(pwm_id);

    return 0;
}
