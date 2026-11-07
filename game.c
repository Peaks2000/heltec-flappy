#include <stdint.h>

extern void ets_delay_us(uint32_t);
extern int ets_printf(const char *, ...);
extern void disable_default_watchdog(void);
extern int uart_rx_one_char(uint8_t *);
extern int uart_tx_one_char(uint8_t);
extern void rom_gpio_pad_select_gpio(uint32_t);
extern void rom_gpio_matrix_out(uint32_t, uint32_t, int, int);
extern void rom_gpio_pad_pullup(uint32_t);
extern void rom_gpio_pad_input_enable(uint32_t);

#define REG(offset) (*(volatile uint32_t *)(0x60004000u + (offset)))
#define SDA 17
#define SCL 18
#define RESET 21
#define VEXT 36
#define BUTTON 0

static uint8_t pixels[1024];
static int bird, velocity, score, best, state;
static int pipe_x[2], pipe_top[2], passed[2];
static uint32_t rng;
static int button_raw, button_stable, button_pending;
static uint32_t button_changed;
extern uint32_t _bss_start, _bss_end;

void *memcpy(void *destination, const void *source, unsigned size) {
    uint8_t *d = destination;
    const uint8_t *s = source;
    for (unsigned i = 0; i < size; ++i) d[i] = s[i];
    return destination;
}

static void pin_low(int pin) {
    REG(0x0c) = 1u << pin;
    REG(0x24) = 1u << pin;
}
static void pin_release(int pin) { REG(0x28) = 1u << pin; }
static void pause_bit(void) { ets_delay_us(1); }
static uint32_t cycles(void) {
    uint32_t ticks;
    __asm__ volatile("rsr.ccount %0" : "=a"(ticks));
    return ticks;
}
static void sample_button(void) {
    int down = !(REG(0x3c) & (1u << BUTTON));
    uint32_t now = cycles();
    if (down != button_raw) {
        button_raw = down;
        button_changed = now;
    }
    if (down != button_stable && (uint32_t)(now - button_changed) >= 240000) {
        button_stable = down;
        if (down) button_pending = 1;
    }
}
static void bus_start(void) {
    pin_release(SDA); pin_release(SCL); pause_bit();
    pin_low(SDA); pause_bit(); pin_low(SCL);
}
static void bus_stop(void) {
    pin_low(SDA); pin_release(SCL); pause_bit();
    pin_release(SDA); pause_bit();
}
static int bus_byte(uint8_t value) {
    sample_button();
    for (int bit = 7; bit >= 0; --bit) {
        if ((value >> bit) & 1) pin_release(SDA); else pin_low(SDA);
        pause_bit(); pin_release(SCL); pause_bit(); pin_low(SCL);
    }
    pin_release(SDA); pause_bit(); pin_release(SCL); pause_bit();
    int ack = !(REG(0x3c) & (1u << SDA));
    pin_low(SCL);
    return ack;
}
static int oled_commands(const uint8_t *data, int size) {
    bus_start(); int ack = bus_byte(0x78); bus_byte(0);
    for (int i = 0; i < size; ++i) bus_byte(data[i]);
    bus_stop(); return ack;
}
static void display(void) {
    const uint8_t address[] = {0x21, 0, 127, 0x22, 0, 7};
    oled_commands(address, sizeof address);
    bus_start(); bus_byte(0x78); bus_byte(0x40);
    for (int i = 0; i < 1024; ++i) bus_byte(pixels[i]);
    bus_stop();
}
static int oled_init(void) {
    int pins[] = {SDA, SCL, RESET, VEXT};
    for (int i = 0; i < 4; ++i) {
        rom_gpio_pad_select_gpio(pins[i]);
        rom_gpio_matrix_out(pins[i], 256, 0, 0);
    }
    rom_gpio_pad_pullup(SDA); rom_gpio_pad_pullup(SCL);
    rom_gpio_pad_input_enable(SDA);
    REG(0x18) = 1u << (VEXT - 32);
    REG(0x30) = 1u << (VEXT - 32);
    pin_release(SDA); pin_release(SCL);
    pin_low(RESET); ets_delay_us(20000);
    REG(0x08) = 1u << RESET; ets_delay_us(50000);
    const uint8_t setup[] = {
        0xae, 0xd5, 0x80, 0xa8, 0x3f, 0xd3, 0, 0x40,
        0x8d, 0x14, 0x20, 0, 0xa1, 0xc8, 0xda, 0x12,
        0x81, 0x7f, 0xd9, 0xf1, 0xdb, 0x40, 0xa4, 0xa6, 0xaf
    };
    return oled_commands(setup, sizeof setup);
}
static void pixel(int x, int y, int on) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    uint8_t mask = 1u << (y & 7);
    if (on) pixels[x + (y >> 3) * 128] |= mask;
    else pixels[x + (y >> 3) * 128] &= (uint8_t)~mask;
}
static void box(int x, int y, int w, int h, int on) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i) pixel(i, j, on);
}
static uint16_t glyph(char c) {
    switch(c) {
    case '0': return 0x7b6f; case '1': return 0x2492;
    case '2': return 0x73e7; case '3': return 0x73cf;
    case '4': return 0x5bc9; case '5': return 0x79cf;
    case '6': return 0x79ef; case '7': return 0x7249;
    case '8': return 0x7bef; case '9': return 0x7bcf;
    case 'A': return 0x2bed; case 'B': return 0x6bae;
    case 'E': return 0x79a7; case 'F': return 0x79a4;
    case 'G': return 0x39eb; case 'I': return 0x7497;
    case 'L': return 0x4927; case 'M': return 0x5fed;
    case 'O': return 0x2b6a; case 'P': return 0x7be4;
    case 'R': return 0x6bad; case 'S': return 0x79cf;
    case 'T': return 0x7492; case 'V': return 0x5b6a;
    case 'Y': return 0x5a92; default: return 0;
    }
}
static void text(int x, int y, const char *s, int scale) {
    for (; *s; ++s, x += 4 * scale) {
        uint16_t bits = glyph(*s);
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (bits & (1u << (14 - row * 3 - col)))
                    box(x + col * scale, y + row * scale, scale, scale, 1);
    }
}
static void number(int x, int y, int n) {
    char digits[4];
    digits[0] = '0' + (n / 100) % 10;
    digits[1] = '0' + (n / 10) % 10;
    digits[2] = '0' + n % 10; digits[3] = 0;
    text(x, y, digits, 1);
}
static int new_top(void) {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return 14 + (int)(rng % 15);
}
static void restart(void) {
    bird = 29 * 256; velocity = -380; score = 0; state = 1;
    pipe_x[0] = 128; pipe_x[1] = 208;
    for (int i = 0; i < 2; ++i) { pipe_top[i] = new_top(); passed[i] = 0; }
    ets_printf("RAM_FLAPPY_SCORE 0\r\n");
}
static void draw(void) {
    for (int i = 0; i < 1024; ++i) pixels[i] = 0;
    text(2, 2, "FLAPPY", 1); number(113, 2, score);
    box(0, 9, 128, 1, 1); box(0, 62, 128, 2, 1);
    for (int i = 0; i < 2; ++i) {
        int x = pipe_x[i], top = pipe_top[i];
        box(x, 10, 12, top - 10, 1);
        box(x - 1, top - 3, 14, 3, 1);
        box(x, top + 27, 12, 62 - top - 27, 1);
        box(x - 1, top + 27, 14, 3, 1);
    }
    int y = bird / 256;
    box(20, y + 1, 7, 4, 1); box(21, y, 5, 6, 1);
    pixel(25, y + 1, 0); box(27, y + 3, 2, 1, 1);
    if (state != 1) {
        box(16, 20, 99, 30, 0);
        text(state == 0 ? 40 : 29, 23, state == 0 ? "FLAPPY" : "GAME OVER", 2);
        text(state == 0 ? 42 : 40, 39,
             state == 0 ? "PRG TO PLAY" : "PRG TO RETRY", 1);
    }
    display();
}
static void step(void) {
    velocity += 50;
    if (velocity > 600) velocity = 600;
    bird += velocity;
    int y = bird / 256;
    int crash = y <= 10 || y + 6 >= 62;
    for (int i = 0; i < 2; ++i) {
        pipe_x[i] -= 2;
        if (pipe_x[i] < -14) {
            pipe_x[i] = 146; pipe_top[i] = new_top(); passed[i] = 0;
        }
        if (pipe_x[i] - 1 < 29 && pipe_x[i] + 13 > 20 &&
            (y < pipe_top[i] || y + 6 > pipe_top[i] + 27)) crash = 1;
        if (!passed[i] && pipe_x[i] + 13 < 20) {
            passed[i] = 1; ++score;
            if (score > best) best = score;
            ets_printf("RAM_FLAPPY_SCORE %d\r\n", score);
        }
    }
    if (crash) { state = 2; ets_printf("RAM_FLAPPY_OVER %d\r\n", score); }
}
void __attribute__((noreturn)) game_main(void) {
    ets_delay_us(1);
    for (uint32_t *p = &_bss_start; p < &_bss_end; ++p) *p = 0;
    disable_default_watchdog();
    rom_gpio_pad_select_gpio(BUTTON);
    rom_gpio_pad_input_enable(BUTTON);
    rom_gpio_pad_pullup(BUTTON);
    pin_release(BUTTON);
    button_raw = button_stable = !(REG(0x3c) & (1u << BUTTON));
    button_changed = cycles();
    rng = 0x91a7bc35;
    int ack = oled_init();
    bird = 29 * 256; pipe_x[0] = 128; pipe_x[1] = 208;
    pipe_top[0] = 24; pipe_top[1] = 20;
    ets_printf("\r\nRAM_FLAPPY_READY OLED_ACK=%d\r\n", ack);
    for (;;) {
        sample_button();
        if (button_pending) {
            button_pending = 0;
            if (state != 1) restart();
            velocity = -380;
            ets_printf("RAM_FLAPPY_PRG_JUMP\r\n");
        }
        uint8_t c;
        while (uart_rx_one_char(&c) == 0) {
            if (c == 'p' || c == 'P') {
                if (state != 1) restart();
                velocity = -380;
                ets_printf("RAM_FLAPPY_JUMP\r\n");
            } else if (c == 's') {
                ets_printf("RAM_FRAME\n");
                for (int i = 0; i < 1024; ++i) uart_tx_one_char(pixels[i]);
            } else if (c == 't') {
                ets_printf("RAM_FLAPPY_STATE %d %d %d %d %d %d %d %d\r\n",
                           state, bird, velocity, pipe_x[0], pipe_top[0],
                           pipe_x[1], pipe_top[1], score);
            } else if (c == 'q') {
                ets_printf("RAM_FLAPPY_PAUSED\r\n"); state = 0;
            }
        }
        if (state == 1) step();
        draw();
        for (int i = 0; i < 12; ++i) {
            sample_button(); ets_delay_us(1000);
        }
    }
}
