#include "display_utils.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

void display_init_all(ssd1306_t *display_ssd)
{
    i2c_init(I2C_PORT_INIT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    ssd1306_init(display_ssd, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, DISPLAY_ADDR_INIT, I2C_PORT_INIT);
    ssd1306_config(display_ssd);
    ssd1306_fill(display_ssd, false);
    ssd1306_send_data(display_ssd);
}