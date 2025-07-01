#include "oled_driver.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "ssd1306_font.h" 
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// --- A MUDANÇA CRÍTICA ESTÁ AQUI ---
#define I2C_PORT i2c1
// ------------------------------------

// --- Definições do Driver SSD1306 (do seu código) ---
#define SSD1306_HEIGHT              64
#define SSD1306_WIDTH               128
#define SSD1306_I2C_ADDR            _u(0x3C)
#define SSD1306_I2C_CLK             400
#define SSD1306_PAGE_HEIGHT         _u(8)
#define SSD1306_NUM_PAGES           (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUF_LEN             (SSD1306_NUM_PAGES * SSD1306_WIDTH)

// ... (Restante do arquivo é idêntico ao anterior) ...

// Comandos do SSD1306
#define SSD1306_SET_MEM_MODE        _u(0x20)
#define SSD1306_SET_COL_ADDR        _u(0x21)
#define SSD1306_SET_PAGE_ADDR       _u(0x22)
#define SSD1306_SET_DISP_START_LINE _u(0x40)
#define SSD1306_SET_CONTRAST        _u(0x81)
#define SSD1306_SET_CHARGE_PUMP     _u(0x8D)
#define SSD1306_SET_SEG_REMAP       _u(0xA0)
#define SSD1306_SET_ENTIRE_ON       _u(0xA4)
#define SSD1306_SET_NORM_DISP       _u(0xA6)
#define SSD1306_SET_MUX_RATIO       _u(0xA8)
#define SSD1306_SET_DISP            _u(0xAE)
#define SSD1306_SET_COM_OUT_DIR     _u(0xC0)
#define SSD1306_SET_DISP_OFFSET     _u(0xD3)
#define SSD1306_SET_DISP_CLK_DIV    _u(0xD5)
#define SSD1306_SET_PRECHARGE       _u(0xD9)
#define SSD1306_SET_COM_PIN_CFG     _u(0xDA)
#define SSD1306_SET_VCOM_DESEL      _u(0xDB)
#define SSD1306_SET_SCROLL          _u(0x2E)

static uint8_t oled_buffer[SSD1306_BUF_LEN];

static void send_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x80, cmd};
    i2c_write_blocking(I2C_PORT, SSD1306_I2C_ADDR, buf, 2, false);
}

static void send_cmd_list(uint8_t *buf, int num) {
    for (int i = 0; i < num; i++)
        send_cmd(buf[i]);
}

static void send_buffer_internal(uint8_t *buf, int buflen) {
    uint8_t *temp_buf = malloc(buflen + 1);
    temp_buf[0] = 0x40;
    memcpy(temp_buf + 1, buf, buflen);
    i2c_write_blocking(I2C_PORT, SSD1306_I2C_ADDR, temp_buf, buflen + 1, false);
    free(temp_buf);
}

static void ssd1306_init_sequence() {
    uint8_t cmds[] = {
        SSD1306_SET_DISP, SSD1306_SET_MEM_MODE, 0x00, SSD1306_SET_DISP_START_LINE,
        SSD1306_SET_SEG_REMAP | 0x01, SSD1306_SET_MUX_RATIO, SSD1306_HEIGHT - 1,
        SSD1306_SET_COM_OUT_DIR | 0x08, SSD1306_SET_DISP_OFFSET, 0x00,
        SSD1306_SET_COM_PIN_CFG, 0x12, SSD1306_SET_DISP_CLK_DIV, 0x80,
        SSD1306_SET_PRECHARGE, 0xF1, SSD1306_SET_VCOM_DESEL, 0x30,
        SSD1306_SET_CONTRAST, 0xFF, SSD1306_SET_ENTIRE_ON, SSD1306_SET_NORM_DISP,
        SSD1306_SET_CHARGE_PUMP, 0x14, SSD1306_SET_SCROLL | 0x00, SSD1306_SET_DISP | 0x01,
    };
    send_cmd_list(cmds, count_of(cmds));
}

static inline int get_font_index(uint8_t ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 1;
    if (ch >= '0' && ch <= '9') return ch - '0' + 27;
    return 0;
}

static void write_char_internal(int16_t x, int16_t y, uint8_t ch) {
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8) return;
    y /= 8;
    ch = toupper(ch);
    int idx = get_font_index(ch);
    int fb_idx = y * SSD1306_WIDTH + x;
    for (int i = 0; i < 8; i++) {
        if (fb_idx < SSD1306_BUF_LEN) {
            oled_buffer[fb_idx++] = font[idx * 8 + i];
        }
    }
}

void oled_init(void) {
    i2c_init(I2C_PORT, SSD1306_I2C_CLK * 1000);
    gpio_set_function(OLED_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_I2C_SDA_PIN);
    gpio_pull_up(OLED_I2C_SCL_PIN);
    ssd1306_init_sequence();
    oled_clear_buffer();
    oled_render();
}

void oled_clear_buffer(void) {
    memset(oled_buffer, 0, SSD1306_BUF_LEN);
}

void oled_write_string(int16_t x, int16_t y, char *str) {
    if (x > SSD1306_WIDTH || y > SSD1306_HEIGHT) return;
    while (*str) {
        write_char_internal(x, y, *str++);
        x += 8;
    }
}

void oled_render(void) {
    uint8_t cmds[] = {
        SSD1306_SET_COL_ADDR, 0, SSD1306_WIDTH - 1,
        SSD1306_SET_PAGE_ADDR, 0, SSD1306_NUM_PAGES - 1
    };
    send_cmd_list(cmds, count_of(cmds));
    send_buffer_internal(oled_buffer, SSD1306_BUF_LEN);
}
