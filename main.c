// STM32F100 and SI4032 RTTY transmitter
// released under GPL v.2 by anonymous developer
// enjoy and have a nice day
// ver 1.5a
#include <stm32f10x_gpio.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_spi.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_usart.h>
#include <stm32f10x_adc.h>
#include <stm32f10x_rcc.h>
#include "stdlib.h"
#include <stdio.h>
#include <string.h>
#include <misc.h>
#include "init.h"
#include "config.h"
#include "radio.h"
#include "ublox.h"
#include "delay.h"
#include "aprs.h"
///////////////////////////// test mode /////////////
static __IO uint32_t DelayCounter;
volatile uint32_t last_sended = 0;
volatile uint32_t interval = APRS_INTERVAL;

//const unsigned char test = 0; // 0 - normal, 1 - short frame only cunter, height, flag

#define GREEN  GPIO_Pin_7
#define RED  GPIO_Pin_8

unsigned int send_cun;        //frame counter

char status[2] = {'N'};
int voltage;

volatile char flaga = 0;
uint16_t CRC_rtty = 0x12ab;  //checksum
char buf_rtty[200];

volatile unsigned char pun = 0;
volatile unsigned int cun = 10;
volatile unsigned char tx_on = 0;
volatile unsigned int tx_on_delay;
volatile unsigned char tx_enable = 0;
volatile unsigned int stat_sended = 0;
volatile uint16_t button_pressed = 0;
volatile uint8_t disable_armed = 0;
volatile unsigned int flexible_delay = 60;


/**
 * GPS data processing
 */
void USART1_IRQHandler(void) {
  if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
    ublox_handle_incoming_byte((uint8_t) USART_ReceiveData(USART1));
      }else if (USART_GetITStatus(USART1, USART_IT_ORE) != RESET) {
    USART_ReceiveData(USART1);
  } else {
    USART_ReceiveData(USART1);
  }
}

void TIM2_IRQHandler(void) {
  if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    if (aprs_is_active()){
      aprs_timer_handler();
    }
  }
}

int main(void) {
  RCC_Conf();
  NVIC_Conf();
  init_port();

  delay_init();
  ublox_init();

  GPIO_SetBits(GPIOB, RED);
  USART_SendData(USART3, 0xc);

  radio_soft_reset();
  // setting TX frequency
  radio_set_tx_frequency(APRS_FREQUENCY);

  // setting TX power
  radio_rw_register(0x6D, 00 | (TX_POWER & 0x0007), 1);

  radio_rw_register(0x71, 0x00, 1);
  radio_rw_register(0x12, 0x20, 1);
  radio_rw_register(0x13, 0x00, 1);
  radio_rw_register(0x12, 0x00, 1);
  radio_rw_register(0x0f, 0x80, 1);
  tx_on = 0;
  tx_enable = 1;

  aprs_init();
  while (1) {


    if (tx_on == 0 && tx_enable) {
    	// podciagamy dane z GPS-a
        GPSEntry gpsData;
        ublox_get_last_data(&gpsData);

        // zmiana timingu w zaleznosci od wybranego trybu
    	if(APRS_SMARTBIKON == 0){
    		interval = APRS_INTERVAL;	//sta�y timing
    	}
    	else{
            // zmiana czestotliwosci nadawania ramki na podstawie predkosci
            int predkosc = (gpsData.speed_raw)*0.036;
            if(predkosc < APRS_SB_LOW_SPEED){
            	flexible_delay = APRS_SB_LOW_RATE;
            } else if(predkosc > APRS_SB_LOW_SPEED && predkosc < APRS_SB_FAST_SPEED){
            	// return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
            	// in  = APRS_SB_LOW_SPEED, APRS_SB_FAST_SPEED
            	// out = APRS_SB_LOW_RATE, APRS_SB_FAST_RATE
            	flexible_delay = (predkosc - APRS_SB_LOW_SPEED) * (APRS_SB_FAST_SPEED - APRS_SB_LOW_SPEED) / (APRS_SB_FAST_SPEED - APRS_SB_LOW_SPEED) + APRS_SB_LOW_RATE;
            } else if(predkosc > APRS_SB_FAST_SPEED){
            	flexible_delay = APRS_SB_FAST_RATE;
            }
    		interval = flexible_delay;	//((flexible_delay*1000)-1000);
    	}


        if(gpsData.licznik_sekund > (last_sended+interval)){
        last_sended = gpsData.licznik_sekund;


        USART_Cmd(USART1, DISABLE);

        GPIO_SetBits(GPIOB, GREEN);
        GPIO_ResetBits(GPIOB, RED);
        if(ENABLE_TX == 1){
        	radio_enable_tx();
        }

        //tutaj zlecamy wysylka ramki
        if(gpsData.fix < 3){
        	aprs_send_status();
        	stat_sended = 1;
        }else{
        	if(stat_sended == 1){
        		aprs_send_status_ok();
        		stat_sended = 0;
        	}
        	aprs_send_position(gpsData);
        }
        if(ENABLE_TX == 1){
        	radio_disable_tx();
        }

        USART_Cmd(USART1, ENABLE);
        GPIO_SetBits(GPIOB, RED);
        GPIO_ResetBits(GPIOB, GREEN);
    }
    } else {
      NVIC_SystemLPConfig(NVIC_LP_SEVONPEND, DISABLE);
      __WFI();
    }
  }
}
