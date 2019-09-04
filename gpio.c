#include <stdint.h>
#include <stm32l433xx.h>

void gpio_init() {
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
  GPIOA->OSPEEDR = 0xFFFFFFFF;
  GPIOB->OSPEEDR = 0xFFFFFFFF;
  GPIOA->ODR = 0xFFFFFFFF;
  GPIOB->ODR = 0xFFFFFFFF;
}

void gpio_port_mode(GPIO_TypeDef * port, uint32_t pin, uint32_t mode, uint32_t af, uint32_t pupdr, uint32_t otyper) {
  port->MODER &= ~( 3 << (pin * 2));
  port->MODER |= (mode << (pin * 2));
  if(pin > 8) {
    port->AFR[1] &= ~(0xF << ((pin-8) * 4));
    port->AFR[1] |= af << ((pin-8) * 4);
  } else {
    port->AFR[0] &= ~(0xF << (pin * 4));
    port->AFR[0] |= af << (pin * 4);
  }
  if(otyper) {
    port->OTYPER |= (1<<pin);
  } else {
    port->OTYPER &= ~(1<<pin);
  }
  port->PUPDR &= ~(3 << (pin * 2));
  port->PUPDR |= pupdr << (pin * 2);
}
