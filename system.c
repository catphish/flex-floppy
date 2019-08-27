#include <stm32l433xx.h>
#include "util.h"

#define nop()  __asm__ __volatile__ ("nop" ::)

void SystemInitError(uint8_t error_source) {
  while(1);
}

void SystemInit() {

  // Enable FPU
  SCB->CPACR |= 0xf00000;

  RCC->CR |= (1<<8);
  /* Wait until HSI ready */
  while ((RCC->CR & RCC_CR_HSIRDY) == 0);

  /* Disable main PLL */
  RCC->CR &= ~(RCC_CR_PLLON);
  /* Wait until PLL ready (disabled) */
  while ((RCC->CR & RCC_CR_PLLRDY) != 0);

  /* Configure Main PLL */
  RCC->PLLCFGR = (1<<24)|(10<<8)|2;

  /* PLL On */
  RCC->CR |= RCC_CR_PLLON;
  /* Wait until PLL is locked */
  while ((RCC->CR & RCC_CR_PLLRDY) == 0);

  /*
   * FLASH configuration block
   * enable instruction cache
   * enable prefetch
   * set latency to 2WS (3 CPU cycles)
   */
  FLASH->ACR |= FLASH_ACR_ICEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_2WS;

  /* Set clock source to PLL */
  RCC->CFGR |= RCC_CFGR_SW_PLL;
  /* Check clock source */
  while ((RCC->CFGR & RCC_CFGR_SWS_PLL) != RCC_CFGR_SWS_PLL);

  // Enable GPIOA clock
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  // Enable Power control clock
  RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
  // Enable USB clock
  RCC->APB1ENR1 |= RCC_APB1ENR1_USBFSEN;
  // CRS Clock eneble
  RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN;

  // Enable HSI48
  RCC->CRRCR |= RCC_CRRCR_HSI48ON;
  while ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) != RCC_CRRCR_HSI48RDY);

  // A2 -> USART2_TX
  GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL2_Msk;
  GPIOA->AFR[0] |= GPIO_AFRL_AFSEL2_0 | GPIO_AFRL_AFSEL2_1 | GPIO_AFRL_AFSEL2_2;

  // A11-A12 -> USB
  GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL11_Msk;
  GPIOA->AFR[1] |= GPIO_AFRH_AFSEL11_3 | GPIO_AFRH_AFSEL11_1;
  GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL12_Msk;
  GPIOA->AFR[1] |= GPIO_AFRH_AFSEL12_3 | GPIO_AFRH_AFSEL12_1;

  // PORTA Modes
  GPIOA->OSPEEDR = 0xFFFFFFFF; // Port A very high speed
  GPIOA->MODER   = 0xEABFFFEF; // A2 as UART TX, A11-A12 USB, A13-14 SWD

  // Enable USART2 clock
  RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
  // Disable USART2
  USART2->CR1 = 0;
  // Set data rate
  //USART2->BRR = 694; // 115200
  USART2->BRR = 40; // 2M
  // Enable USART2
  USART2->CR1 |= USART_CR1_UE;
  // Enable transmit
  USART2->CR1 |= USART_CR1_TE;

  // Enable USB Power
  PWR->CR2 |= (1<<10);
  usleep(10);

  // Enable USB
  USB->CNTR &= ~USB_CNTR_PDWN;
  usleep(10);
  USB->CNTR &= ~USB_CNTR_FRES;
  usleep(10);
  USB->ISTR = 0;
  USB->CNTR |= USB_CNTR_RESETM;
  USB->BCDR |= USB_BCDR_DPPU;

  NVIC->ISER[2] = (1 << (USB_IRQn - 64));

}
