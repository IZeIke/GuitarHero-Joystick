#define IS_SW_PRESSED()   (!(PINC & (1<<PC3)))

#define LED_RED    0
#define LED_YELLOW 1
#define LED_GREEN  2

void init_peripheral();
void set_led(uint8_t pin, uint8_t state);
void set_led_value(uint8_t value);
uint16_t get_light();

