#ifndef BOARD_DEF_H
#define BOARD_DEF_H

/*************************************************************  I/O  *************************************************************/
#define TXD1 17                 //The TXD of UART1 corresponds to GPIO
#define RXD1 18                 //The RXD of UART1 corresponds to GPIO

#define GPIO_PIN_CH1      1     // CH1 Control GPIO
#define GPIO_PIN_CH2      2     // CH2 Control GPIO
#define GPIO_PIN_CH3      41    // CH3 Control GPIO
#define GPIO_PIN_CH4      42    // CH4 Control GPIO
#define GPIO_PIN_CH5      45    // CH5 Control GPIO
#define GPIO_PIN_CH6      46    // CH6 Control GPIO

#define GPIO_PIN_RGB      38    // RGB Control GPIO

#define GPIO_PIN_Buzzer   21    // Buzzer Control GPIO

/***********************************************************  Buzzer  ***********************************************************/
#define PWM_Channel     1       // PWM Channel   
#define Frequency       1000     // PWM frequencyconst
#define Resolution      8       // PWM resolution ratio
#define Dutyfactor      200     // PWM Dutyfactor

#endif