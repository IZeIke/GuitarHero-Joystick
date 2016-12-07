#include <Arduino.h>

void init_peripheral()
{
  DDRC |= (1<<PC2) | (1<<PC1) | (1<<PC0);
  DDRC &= ~( (1<<PC3) | (1<<PC4) );
  PORTC &= ~( (1<<PC2) | (1<<PC1) | (1<<PC0) | (1<<PC4) );
  PORTC |= (1<<PC3);
}

void set_led(uint8_t pin, uint8_t state)
{
  if (state)
    PORTC |= (1<<pin);
  else
    PORTC &= ~(1<<pin);
}

void set_led_value(uint8_t value)
{
  PORTC &= ~(0b00000111);
  PORTC |= (value & 0b00000111);
}

uint16_t get_light()
{
  return analogRead(PIN_PC4);
}

