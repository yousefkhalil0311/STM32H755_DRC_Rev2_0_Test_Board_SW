/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct {
	I2C_HandleTypeDef*	handle;
	uint8_t				address;
	uint32_t			pinMode;	//store mode (input = 1/output = 0) of P7-0, P17-10 and P27-20
	uint32_t			state;		//store state (if output) of P7-0, P17-10 and P27-20
} IOEXP_t;

typedef struct {
	uint8_t			isPin; //Not controllable pin => 0; Pin connected directly to STM32 => 1; Pin connected to IO expander => IO expander pin
	GPIO_TypeDef*	Port;
	uint16_t		Pin;
	bool			has_SPDT;     //true if pin on DRC is multifunctional; false otherwise
	int				IOX_SPDT_Pin; //Pin used to control SPDT Switch; -1 for no SPDT switch on pin
	int				IOX_LED_Pin; //Pin used to control LED (DAC, ADC, Digital) associated with pin; -1 for no SPDT switch on pin
	int				defaultAnalog;
} Pin_t;

typedef struct {
	SPI_HandleTypeDef*	handle;
	GPIO_TypeDef*		cs_port;	//chip select pin port of IO expander
	uint16_t			cs_pin;		//chip select pin number of IO expander
	uint16_t			pinMode;	//store mode (input = 1/output = 0) of Bank A and Bank B IO
	uint16_t			state;		//store state (if output) of Bank A and Bank B IO
} SPIIOEXP_t;

typedef struct {
	uint8_t			is90Pin;	//IO expander pin directly controlling DRC -> 1; IO expander pin controlling switch on DVT interposer -> 0
	SPIIOEXP_t*		IOEXP;		//Reference IO expander connected to this pin;
	uint16_t		Pin; 		//Pin connected to on IO expander (16 bits -> 16 pins) Ex: pin 7 -> Pin = 1 << 7
	bool			state;		//current state of the pin
} SPIIOXPin_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */

#define IOEXP_Config_CMD	0x8C
#define IOEXP_PinSet_CMD	0x84
#define IOEXP_ReadAcc_CMD	0x80

#define LED_DAC_MODE 		0
#define LED_ADC_MODE		1
#define LED_DIG_MODE		2

#define PIN_OUTPUT			0
#define PIN_INPUT			1

IOEXP_t IOEXP_SPDT = {&hi2c2, 0x23, 0x00000C, 0x000000};	//Set all pins to output except for P22 and P23, which are directly connected to 90P connector
															//Set all SPDT control outputs to 0 (sets analog path) and LED outputs to whatever (A in this case)
IOEXP_t IOEXP_LED  = {&hi2c2, 0x22, 0x000080, 0x000070};	//Set all pins to output except for V5P0_sense
															//Set CS pins high, TFT_RST Low, and leds to DAC mode (set Low) if supported. ADC mode (set High) otherwise

SPIIOEXP_t SPIEXP0 = {&hspi2, GPIOD, GPIO_PIN_11, 0xFFFF, 0x0000};
SPIIOEXP_t SPIEXP1 = {&hspi2, GPIOD, GPIO_PIN_12, 0xFFFF, 0x0000};
SPIIOEXP_t SPIEXP2 = {&hspi2, GPIOD, GPIO_PIN_13, 0xFFFF, 0x0000};
SPIIOEXP_t SPIEXP3 = {&hspi2, GPIOD, GPIO_PIN_14, 0xFFFF, 0x0000};
SPIIOEXP_t SPIEXP4 = {&hspi2, GPIOD, GPIO_PIN_10, 0xFFFF, 0x0000};
SPIIOEXP_t SPIEXP5 = {&hspi2, GPIOA, GPIO_PIN_8 , 0xFFFF, 0x0000};
SPIIOEXP_t SPIEXP6 = {&hspi2, GPIOD, GPIO_PIN_8 , 0xFFFF, 0x0000};


const Pin_t PIN0  = {0},
			PIN1  = {0},
			PIN2  = {1, GPIOD, GPIO_PIN_3 , true, 0x000002 /*21*/, 0x010000 /*0*/, LED_DAC_MODE},
			PIN3  = {1, GPIOD, GPIO_PIN_1 , true, 0x000001 /*20*/, 0x020000 /*1*/, LED_DAC_MODE},
			PIN4  = {1, GPIOC, GPIO_PIN_12, false},
			PIN5  = {1, GPIOC, GPIO_PIN_10, false},
			PIN6  = {1, GPIOA, GPIO_PIN_15, true, 0x004000 /*16*/, 0x040000 /*2*/, LED_DAC_MODE},
			PIN7  = {1, GPIOA, GPIO_PIN_8 , false},
			PIN8  = {1, GPIOC, GPIO_PIN_8 , false},
			PIN9  = {1, GPIOG, GPIO_PIN_8 , false},
			PIN10 = {1, GPIOG, GPIO_PIN_7 , true, 0x000100 /*10*/, 0x080000 /*3*/, LED_DAC_MODE},
			PIN11 = {1, GPIOG, GPIO_PIN_6 , false},
			PIN12 = {1, GPIOE, GPIO_PIN_14, false},
			PIN13 = {0},
			PIN14 = {1, GPIOB, GPIO_PIN_12, false},
			PIN15 = {0},
			PIN16 = {1, GPIOE, GPIO_PIN_10, false},
			PIN17 = {1, GPIOE, GPIO_PIN_9 , true, 0x200000 /*5*/ , 0x100000 /*4*/, LED_DAC_MODE},
			PIN18 = {1, GPIOE, GPIO_PIN_7 , false},
			PIN19 = {1, GPIOF, GPIO_PIN_15, false},
			PIN20 = {1, GPIOB, GPIO_PIN_2 , false},
			PIN21 = {1, GPIOB, GPIO_PIN_1 , true, 0x040000 /*2*/ , 0x200000 /*5*/, LED_DAC_MODE},
			PIN22 = {1, GPIOC, GPIO_PIN_5 , false},
			PIN23 = {1, GPIOG, GPIO_PIN_9 , false},
			PIN24 = {1, GPIOG, GPIO_PIN_11, false},
			PIN25 = {1, GPIOD, GPIO_PIN_7 , false},
			PIN26 = {1, GPIOB, GPIO_PIN_3 , false},
			PIN27 = {1, GPIOB, GPIO_PIN_6 , false},
			PIN28 = {1, GPIOB, GPIO_PIN_9 , false},
			PIN29 = {1, GPIOA, GPIO_PIN_3 , false},
			PIN30 = {0},
			PIN31 = {0},
			PIN32 = {1, GPIOD, GPIO_PIN_2 , true, 0x008000 /*17*/, 0x400000 /*6*/, LED_DAC_MODE},
			PIN33 = {1, GPIOD, GPIO_PIN_0 , true, 0x002000 /*15*/, 0x800000 /*7*/, LED_DAC_MODE},
			PIN34 = {1, GPIOE, GPIO_PIN_2 , true, 0x001000 /*14*/, 0x000100 /*10*/, LED_DAC_MODE},
			PIN35 = {1, GPIOC, GPIO_PIN_11, true, 0x000800 /*13*/, 0x000200 /*11*/, LED_DAC_MODE},
			PIN36 = {1, GPIOE, GPIO_PIN_5 , true, 0x000200 /*11*/, 0x000400 /*12*/, LED_DAC_MODE},
			PIN37 = {1, GPIOC, GPIO_PIN_9 , false},
			PIN38 = {1, GPIOC, GPIO_PIN_7 , false},
			PIN39 = {1, GPIOC, GPIO_PIN_6 , false},
			PIN40 = {1, GPIOD, GPIO_PIN_15, true, 0x000400 /*12*/, 0x000800 /*13*/, LED_DAC_MODE},
			PIN41 = {1, GPIOB, GPIO_PIN_13, false},
			PIN42 = {1, GPIOB, GPIO_PIN_14, false},
			PIN43 = {1, GPIOE, GPIO_PIN_15, false},
			PIN44 = {1, GPIOE, GPIO_PIN_13, false},
			PIN45 = {1, GPIOE, GPIO_PIN_12, false},
			PIN46 = {1, GPIOE, GPIO_PIN_11, true, 0x800000 /*7*/ , 0x001000 /*14*/, LED_DAC_MODE},
			PIN47 = {1, GPIOE, GPIO_PIN_8 , false},
			PIN48 = {1, GPIOF, GPIO_PIN_9 , true, 0x080000 /*3*/ , 0x002000 /*15*/, LED_DAC_MODE},
			PIN49 = {1, GPIOF, GPIO_PIN_14, false},
			PIN50 = {1, GPIOF, GPIO_PIN_11, true, 0x100000 /*4*/ , 0x004000 /*16*/, LED_ADC_MODE},
			PIN51 = {1, GPIOB, GPIO_PIN_0 , true, 0x010000 /*0*/ , 0x008000 /*17*/, LED_DAC_MODE},
			PIN52 = {1, GPIOC, GPIO_PIN_4 , false},
			PIN53 = {1, GPIOG, GPIO_PIN_10, false},
			PIN54 = {1, GPIOD, GPIO_PIN_5 , false},
			PIN55 = {1, GPIOD, GPIO_PIN_4 , true, 0x020000 /*1*/ , 0x000001 /*20*/, LED_ADC_MODE},
			PIN56 = {1, GPIOG, GPIO_PIN_14, false},
			PIN57 = {1, GPIOB, GPIO_PIN_8 , false},
			PIN58 = {1, GPIOG, GPIO_PIN_12, false},
			PIN59 = {1, GPIOE, GPIO_PIN_1 , false},
			PIN60 = {0},
			PIN61 = {0},
			PIN62 = {0x000008},
			PIN63 = {0x000004},
			PIN64 = {1, GPIOE, GPIO_PIN_4 , false},
			PIN65 = {1, GPIOE, GPIO_PIN_3 , false},
			PIN66 = {1, GPIOE, GPIO_PIN_6 , false},
			PIN67 = {1, GPIOC, GPIO_PIN_13, false},
			PIN68 = {1, GPIOF, GPIO_PIN_6 , false},
			PIN69 = {1, GPIOF, GPIO_PIN_7 , false},
			PIN70 = {1, GPIOD, GPIO_PIN_14, false},
			PIN71 = {1, GPIOD, GPIO_PIN_13, false},
			PIN72 = {1, GPIOD, GPIO_PIN_12, false},
			PIN73 = {1, GPIOD, GPIO_PIN_11, false},
			PIN74 = {1, GPIOD, GPIO_PIN_9 , true, 0x400000 /*6*/ , 0x000002 /*21*/, LED_DAC_MODE},
			PIN75 = {1, GPIOD, GPIO_PIN_8 , false},
			PIN76 = {1, GPIOD, GPIO_PIN_10, false},
			PIN77 = {1, GPIOF, GPIO_PIN_8 , false},
			PIN78 = {1, GPIOF, GPIO_PIN_10, false},
			PIN79 = {1, GPIOC, GPIO_PIN_0 , false},
			PIN80 = {1, GPIOC, GPIO_PIN_1 , false},
			PIN81 = {1, GPIOA, GPIO_PIN_0 , false},
			PIN82 = {1, GPIOA, GPIO_PIN_1 , false},
			PIN83 = {1, GPIOA, GPIO_PIN_2 , false},
			PIN84 = {1, GPIOD, GPIO_PIN_6 , false},
			PIN85 = {1, GPIOG, GPIO_PIN_13, false},
			PIN86 = {1, GPIOB, GPIO_PIN_4 , false},
			PIN87 = {1, GPIOB, GPIO_PIN_5 , false},
			PIN88 = {1, GPIOB, GPIO_PIN_7 , false},
			PIN89 = {1, GPIOE, GPIO_PIN_0 , false},
			PIN90 = {0};

const Pin_t* pins[91] = {
		&PIN0,
		&PIN1,  &PIN2,  &PIN3,  &PIN4,  &PIN5,  &PIN6,  &PIN7,  &PIN8,  &PIN9,  &PIN10,
		&PIN11, &PIN12, &PIN13, &PIN14, &PIN15, &PIN16, &PIN17, &PIN18, &PIN19, &PIN20,
		&PIN21, &PIN22, &PIN23, &PIN24, &PIN25, &PIN26, &PIN27, &PIN28, &PIN29, &PIN30,
		&PIN31, &PIN32, &PIN33, &PIN34, &PIN35, &PIN36, &PIN37, &PIN38, &PIN39, &PIN40,
		&PIN41, &PIN42, &PIN43, &PIN44, &PIN45, &PIN46, &PIN47, &PIN48, &PIN49, &PIN50,
		&PIN51, &PIN52, &PIN53, &PIN54, &PIN55, &PIN56, &PIN57, &PIN58, &PIN59, &PIN60,
		&PIN61, &PIN62, &PIN63, &PIN64, &PIN65, &PIN66, &PIN67, &PIN68, &PIN69, &PIN70,
		&PIN71, &PIN72, &PIN73, &PIN74, &PIN75, &PIN76, &PIN77, &PIN78, &PIN79, &PIN80,
		&PIN81, &PIN82, &PIN83, &PIN84, &PIN85, &PIN86, &PIN87, &PIN88, &PIN89, &PIN90
};


SPIIOXPin_t SPIPIN0  = {0},
			SPIPIN1  = {0},
			SPIPIN2  = {1, &SPIEXP5, 1 << 7},
			SPIPIN3  = {2, &SPIEXP6, 1 << 9},
			SPIPIN4  = {1, &SPIEXP0, 1 << 11},
			SPIPIN5  = {1, &SPIEXP0, 1 << 13},
			SPIPIN6  = {1, &SPIEXP5, 1 << 13},
			SPIPIN7  = {1, &SPIEXP0, 1 << 0},
			SPIPIN8  = {1, &SPIEXP0, 1 << 4},
			SPIPIN9  = {1, &SPIEXP1, 1 << 8},
			SPIPIN10 = {1, &SPIEXP5, 1 << 8},
			SPIPIN11 = {1, &SPIEXP1, 1 << 10},
			SPIPIN12 = {1, &SPIEXP1, 1 << 14},
			SPIPIN13 = {0},
			SPIPIN14 = {1, &SPIEXP1, 1 << 2},
			SPIPIN15 = {0},
			SPIPIN16 = {1, &SPIEXP1, 1 << 5},
			SPIPIN17 = {1, &SPIEXP4, 1 << 1},
			SPIPIN18 = {1, &SPIEXP2, 1 << 10},
			SPIPIN19 = {1, &SPIEXP2, 1 << 12},
			SPIPIN20 = {1, &SPIEXP2, 1 << 15},
			SPIPIN21 = {1, &SPIEXP4, 1 << 12},
			SPIPIN22 = {1, &SPIEXP2, 1 << 2},
			SPIPIN23 = {1, &SPIEXP2, 1 << 5},
			SPIPIN24 = {1, &SPIEXP2, 1 << 7},
			SPIPIN25 = {1, &SPIEXP3, 1 << 11},
			SPIPIN26 = {1, &SPIEXP3, 1 << 13},
			SPIPIN27 = {1, &SPIEXP3, 1 << 0},
			SPIPIN28 = {1, &SPIEXP3, 1 << 2},
			SPIPIN29 = {1, &SPIEXP3, 1 << 6},
			SPIPIN30 = {0},
			SPIPIN31 = {0},
			SPIPIN32 = {1, &SPIEXP5, 1 << 2},
			SPIPIN33 = {2, &SPIEXP6, 1 << 9},
			SPIPIN34 = {1, &SPIEXP5, 1 << 1},
			SPIPIN35 = {1, &SPIEXP5, 1 << 12},
			SPIPIN36 = {2, &SPIEXP6, 1 << 9},
			SPIPIN37 = {1, &SPIEXP0, 1 << 2},
			SPIPIN38 = {1, &SPIEXP0, 1 << 3},
			SPIPIN39 = {1, &SPIEXP0, 1 << 6},
			SPIPIN40 = {1, &SPIEXP4, 1 << 7},
			SPIPIN41 = {1, &SPIEXP1, 1 << 12},
			SPIPIN42 = {1, &SPIEXP1, 1 << 13},
			SPIPIN43 = {1, &SPIEXP1, 1 << 0},
			SPIPIN44 = {1, &SPIEXP1, 1 << 1},
			SPIPIN45 = {1, &SPIEXP1, 1 << 6},
			SPIPIN46 = {1, &SPIEXP4, 1 << 2},
			SPIPIN47 = {1, &SPIEXP2, 1 << 9},
			SPIPIN48 = {2, &SPIEXP6, 1 << 9},
			SPIPIN49 = {1, &SPIEXP2, 1 << 14},
			SPIPIN50 = {1, &SPIEXP4, 1 << 13},
			SPIPIN51 = {2, &SPIEXP6, 1 << 9},
			SPIPIN52 = {1, &SPIEXP2, 1 << 3},
			SPIPIN53 = {1, &SPIEXP3, 1 << 9},
			SPIPIN54 = {1, &SPIEXP3, 1 << 8},
			SPIPIN55 = {1, &SPIEXP4, 1 << 8},
			SPIPIN56 = {1, &SPIEXP3, 1 << 14},
			SPIPIN57 = {1, &SPIEXP3, 1 << 4},
			SPIPIN58 = {1, &SPIEXP3, 1 << 3},
			SPIPIN59 = {1, &SPIEXP3, 1 << 7},
			SPIPIN60 = {0},
			SPIPIN61 = {0},
			SPIPIN62 = {1, &SPIEXP0, 1 << 8},
			SPIPIN63 = {1, &SPIEXP0, 1 << 9},
			SPIPIN64 = {1, &SPIEXP0, 1 << 10},
			SPIPIN65 = {1, &SPIEXP0, 1 << 12},
			SPIPIN66 = {1, &SPIEXP0, 1 << 14},
			SPIPIN67 = {1, &SPIEXP0, 1 << 15},
			SPIPIN68 = {1, &SPIEXP0, 1 << 1},
			SPIPIN69 = {1, &SPIEXP0, 1 << 5},
			SPIPIN70 = {1, &SPIEXP0, 1 << 7},
			SPIPIN71 = {1, &SPIEXP1, 1 << 9},
			SPIPIN72 = {1, &SPIEXP1, 1 << 11},
			SPIPIN73 = {1, &SPIEXP1, 1 << 15},
			SPIPIN74 = {2, &SPIEXP6, 1 << 9},
			SPIPIN75 = {1, &SPIEXP1, 1 << 3},
			SPIPIN76 = {1, &SPIEXP1, 1 << 4},
			SPIPIN77 = {1, &SPIEXP1, 1 << 7},
			SPIPIN78 = {1, &SPIEXP2, 1 << 8},
			SPIPIN79 = {1, &SPIEXP2, 1 << 11},
			SPIPIN80 = {1, &SPIEXP2, 1 << 13},
			SPIPIN81 = {1, &SPIEXP2, 1 << 0},
			SPIPIN82 = {1, &SPIEXP2, 1 << 1},
			SPIPIN83 = {1, &SPIEXP2, 1 << 4},
			SPIPIN84 = {1, &SPIEXP2, 1 << 6},
			SPIPIN85 = {1, &SPIEXP3, 1 << 10},
			SPIPIN86 = {1, &SPIEXP3, 1 << 12},
			SPIPIN87 = {1, &SPIEXP3, 1 << 15},
			SPIPIN88 = {1, &SPIEXP3, 1 << 1},
			SPIPIN89 = {1, &SPIEXP3, 1 << 5},
			SPIPIN90 = {0},

			SPIPIN_SEL17  = {2, &SPIEXP4, 1 << 0},
			SPIPIN_SEL46  = {2, &SPIEXP4, 1 << 3},
			SPIPIN_SEL74  = {2, &SPIEXP4, 1 << 4},
			SPIPIN_SEL40  = {2, &SPIEXP4, 1 << 5},
			SPIPIN_SEL55  = {2, &SPIEXP4, 1 << 9},
			SPIPIN_SEL51  = {2, &SPIEXP4, 1 << 10},
			SPIPIN_SEL21  = {2, &SPIEXP4, 1 << 11},
			SPIPIN_SEL50  = {2, &SPIEXP4, 1 << 14},
			SPIPIN_SEL48  = {2, &SPIEXP4, 1 << 15},
			SPIPIN_SEL34  = {2, &SPIEXP5, 1 << 0},
			SPIPIN_SEL32  = {2, &SPIEXP5, 1 << 3},
			SPIPIN_SEL3   = {2, &SPIEXP5, 1 << 4},
			SPIPIN_SEL2   = {2, &SPIEXP5, 1 << 5},
			SPIPIN_SPDTEN = {2, &SPIEXP5, 1 << 6},
			SPIPIN_SPDTEN2 = {2, &SPIEXP4, 1 << 6},
			SPIPIN_SEL10  = {2, &SPIEXP5, 1 << 9},
			SPIPIN_SEL36  = {2, &SPIEXP5, 1 << 10},
			SPIPIN_SEL35  = {2, &SPIEXP5, 1 << 11},
			SPIPIN_SEL6   = {2, &SPIEXP5, 1 << 14},
			SPIPIN_SEL33  = {2, &SPIEXP5, 1 << 15},
			SPIPIN_MDAC0_A0 = {2, &SPIEXP6, 1 << 4},
			SPIPIN_MDAC0_A1 = {2, &SPIEXP6, 1 << 5},
			SPIPIN_MDAC0_A2 = {2, &SPIEXP6, 1 << 6},
			SPIPIN_nMEN0    = {2, &SPIEXP6, 1 << 7},
			SPIPIN_MDAC1_A0 = {2, &SPIEXP6, 1 << 8},
			SPIPIN_MDAC1_A1 = {2, &SPIEXP6, 1 << 9},
			SPIPIN_MDAC1_A2 = {2, &SPIEXP6, 1 << 10},
			SPIPIN_nMEN1    = {2, &SPIEXP6, 1 << 11},
			SPIPIN_MADC_A0  = {2, &SPIEXP6, 1 << 12},
			SPIPIN_MADC_A1  = {2, &SPIEXP6, 1 << 13},
			SPIPIN_MADC_A2  = {2, &SPIEXP6, 1 << 14},
			SPIPIN_nMEN2    = {2, &SPIEXP6, 1 << 15};

SPIIOXPin_t* SPIPins[91] = {
		&SPIPIN0,
		&SPIPIN1,  &SPIPIN2,  &SPIPIN3,  &SPIPIN4,  &SPIPIN5,  &SPIPIN6,  &SPIPIN7,  &SPIPIN8,  &SPIPIN9,  &SPIPIN10,
		&SPIPIN11, &SPIPIN12, &SPIPIN13, &SPIPIN14, &SPIPIN15, &SPIPIN16, &SPIPIN17, &SPIPIN18, &SPIPIN19, &SPIPIN20,
		&SPIPIN21, &SPIPIN22, &SPIPIN23, &SPIPIN24, &SPIPIN25, &SPIPIN26, &SPIPIN27, &SPIPIN28, &SPIPIN29, &SPIPIN30,
		&SPIPIN31, &SPIPIN32, &SPIPIN33, &SPIPIN34, &SPIPIN35, &SPIPIN36, &SPIPIN37, &SPIPIN38, &SPIPIN39, &SPIPIN40,
		&SPIPIN41, &SPIPIN42, &SPIPIN43, &SPIPIN44, &SPIPIN45, &SPIPIN46, &SPIPIN47, &SPIPIN48, &SPIPIN49, &SPIPIN50,
		&SPIPIN51, &SPIPIN52, &SPIPIN53, &SPIPIN54, &SPIPIN55, &SPIPIN56, &SPIPIN57, &SPIPIN58, &SPIPIN59, &SPIPIN60,
		&SPIPIN61, &SPIPIN62, &SPIPIN63, &SPIPIN64, &SPIPIN65, &SPIPIN66, &SPIPIN67, &SPIPIN68, &SPIPIN69, &SPIPIN70,
		&SPIPIN71, &SPIPIN72, &SPIPIN73, &SPIPIN74, &SPIPIN75, &SPIPIN76, &SPIPIN77, &SPIPIN78, &SPIPIN79, &SPIPIN80,
		&SPIPIN81, &SPIPIN82, &SPIPIN83, &SPIPIN84, &SPIPIN85, &SPIPIN86, &SPIPIN87, &SPIPIN88, &SPIPIN89, &SPIPIN90
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */
void pin_Init(const Pin_t* pin);
void pin_SetMode(const Pin_t* pin, int mode);
void pin_Write(const Pin_t* pin, int state);
int  pin_Read(const Pin_t* pin);

void IOEXP_Init(IOEXP_t* IOX);
void IOEXP_SetModes(IOEXP_t* IOX, uint32_t pinMask, int mode);
void IOEXP_SetPins(IOEXP_t* IOX, uint32_t pinMask, int state);
//int IOEXP_ReadAll(IOEXP_t* IOX);
//uint8_t IOEXP_ReadPort(IOEXP_t* IOX, int port);
//bool IOEXP_ReadPin(IOEXP_t* IOX, int pin);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*/////////////////////////////////////////////////////////////////////////////

Pin control functions Start

*//////////////////////////////////////////////////////////////////////////////

void pin_Init(const Pin_t* pin){
	if (pin->isPin > 1){
		IOEXP_SetModes(&IOEXP_SPDT, pin->IOX_SPDT_Pin, PIN_INPUT);
	}
	if (pin->isPin == 1){
		IOEXP_SetPins(&IOEXP_SPDT, pin->IOX_SPDT_Pin, 0);
		IOEXP_SetPins(&IOEXP_LED, pin->IOX_LED_Pin, pin->defaultAnalog);
	}
	return;
}

void pin_SetMode(const Pin_t* pin, int mode){
	GPIO_InitTypeDef initStruct = {0};


	if (pin->isPin > 1){
		IOEXP_SetModes(&IOEXP_SPDT, pin->isPin, mode);
		return;
	}

	if (pin->isPin == 0){
		return;
	}

	initStruct.Pin = pin->Pin;
	if (mode == PIN_OUTPUT) {
		initStruct.Mode = GPIO_MODE_OUTPUT_PP;
		initStruct.Pull = GPIO_NOPULL;
		initStruct.Speed = GPIO_SPEED_FREQ_LOW;
	}
	else if (mode == PIN_INPUT){
		initStruct.Mode = GPIO_MODE_INPUT;
		initStruct.Pull = GPIO_NOPULL;
	}

	HAL_GPIO_Init(pin->Port, &initStruct);

	if(pin->has_SPDT){
		IOEXP_SetPins(&IOEXP_SPDT, pin->IOX_SPDT_Pin, 1);
		IOEXP_SetPins(&IOEXP_LED, pin->IOX_LED_Pin, pin->defaultAnalog);
	}

	return;
}

void pin_Write(const Pin_t* pin, int state){

	if (pin->isPin > 1){
		IOEXP_SetPins(&IOEXP_SPDT, pin->isPin, state);
		return;
	}

	HAL_GPIO_WritePin(pin->Port, pin->Pin, state);

	return;
}


int pin_Read(const Pin_t* pin){
	int val = HAL_GPIO_ReadPin(pin->Port, pin->Pin);
	return val;
}

/*/////////////////////////////////////////////////////////////////////////////

Pin control functions End

*//////////////////////////////////////////////////////////////////////////////

/*/////////////////////////////////////////////////////////////////////////////

IO expander functions Start

*//////////////////////////////////////////////////////////////////////////////

//Initialize IO expander from its parameters
void IOEXP_Init(IOEXP_t* IOX){
	//config IO expander

	uint8_t ConfigBytes[4] = {
			IOEXP_Config_CMD,
			(IOX->pinMode >> 16) & 0xFF,
			(IOX->pinMode >>  8) & 0xFF,
			(IOX->pinMode)		 & 0xFF
	};

	HAL_I2C_Master_Transmit(IOX->handle, IOX->address << 1, ConfigBytes, 4, 50);

	uint8_t pinStateBytes[4] = {
			IOEXP_PinSet_CMD,
			(IOX->state >> 16) & 0xFF,
			(IOX->state >>  8) & 0xFF,
			(IOX->state)	   & 0xFF
	};

	HAL_I2C_Master_Transmit(IOX->handle, IOX->address << 1, pinStateBytes, 4, 50);

	return;
}

//Set pin Mode of IO expander given a pin or pinMask and Mode  (1 is input, 0 is output)
void IOEXP_SetModes(IOEXP_t* IOX, uint32_t pinMask, int mode){
	uint32_t modeMask = 0;
	if (mode) {
		modeMask = 0xFFFFFF;
	}

	IOX->pinMode = (pinMask & modeMask) | (~pinMask & IOX->pinMode);

	uint8_t ConfigBytes[4] = {
			IOEXP_Config_CMD,
			(IOX->pinMode >> 16) & 0xFF,
			(IOX->pinMode >>  8) & 0xFF,
			(IOX->pinMode)		 & 0xFF
	};

	HAL_I2C_Master_Transmit(IOX->handle, IOX->address << 1, ConfigBytes, 4, 50);

	return;
}


//Set pin State of IO expander given a pin or pinMask and state
void IOEXP_SetPins(IOEXP_t* IOX, uint32_t pinMask, int state){
	uint32_t stateMask = 0;
	if (state){
		stateMask = 0xFFFFFF;
	}
	IOX->state = (pinMask & stateMask) | (~pinMask & IOX->state);

	uint8_t pinStateBytes[4] = {
			IOEXP_PinSet_CMD,
			(IOX->state >> 16) & 0xFF,
			(IOX->state >>  8) & 0xFF,
			(IOX->state)	   & 0xFF
	};

	HAL_I2C_Master_Transmit(IOX->handle, IOX->address << 1, pinStateBytes, 4, 50);

	return;
}

void SPIIOEXP_Init(SPIIOEXP_t* SPIEXP){
	uint8_t spiWords[] = {0x40, 0x00, 0xFF, 0xFF}; //Device OPCODE -> 0x40, All port A pins to inputs, all port B pins to inputs
	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(SPIEXP->handle, spiWords, 4, 100);
	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_SET);
	return;
}

//Sets modes of SPI IO Expander pins. 1 -> Input; 0 -> Output
void SPIIOEXP_SetModes(SPIIOEXP_t* SPIEXP, uint16_t mask, uint16_t mode){

	uint8_t currentPortA_Mode = (SPIEXP->pinMode >> 8) & 0xFF;
	uint8_t currentPortB_Mode = SPIEXP->pinMode & 0xFF;

	uint8_t newPortA_Mode = (currentPortA_Mode & ~((mask >> 8) & 0xFF)) | ((mode >> 8) & (mask >> 8) & 0xFF);
	uint8_t newPortB_Mode = (currentPortB_Mode & ~(mask & 0xFF)) | (mode & mask & 0xFF);

	SPIEXP->pinMode = (newPortA_Mode << 8) | newPortB_Mode;

	uint8_t spiWords[] = {0x40, 0x00, newPortA_Mode, newPortB_Mode};

	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(SPIEXP->handle, spiWords, 4, 100);
	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_SET);

	return;
}

void SPIIOEXP_SetMode(SPIIOXPin_t* SPIPin, bool mode){

	uint16_t mode_16bits = mode ? 0xFFFF : 0x0000;

	SPIIOEXP_SetModes(SPIPin->IOEXP, SPIPin->Pin, mode_16bits);

}

void SPIIOEXP_WritePins(SPIIOEXP_t* SPIEXP, uint16_t mask, uint16_t state){

	uint8_t currentPortA_State = (SPIEXP->state >> 8) & 0xFF;
	uint8_t currentPortB_State = SPIEXP->state & 0xFF;

	uint8_t newPortA_State = (currentPortA_State & ~((mask >> 8) & 0xFF)) | ((state >> 8) & (mask >> 8) & 0xFF);
	uint8_t newPortB_State = (currentPortB_State & ~(mask & 0xFF)) | (state & mask & 0xFF);

	SPIEXP->state = (newPortA_State << 8) | newPortB_State;

	uint8_t spiWords[] = {0x40, 0x12, newPortA_State, newPortB_State};

	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(SPIEXP->handle, spiWords, 4, 100);
	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_SET);

	return;
}

void SPIIOEXP_SetPin(SPIIOXPin_t* SPIPin, bool state){

	uint16_t state_16bits = state ? 0xFFFF : 0x0000;

	SPIIOEXP_WritePins(SPIPin->IOEXP, SPIPin->Pin, state_16bits);

}

//Polls all 16 pins of IO expander and saves state into IOEXP struct object. Returns state of specified pin of IO expander
bool SPIIOEXP_readPin(SPIIOEXP_t* SPIEXP, SPIIOXPin_t* SPIPin){

	uint8_t readWords[2];

	//Receive Port A and B data into readWords array
	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Receive(&hspi2, readWords, 2, 100);
	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_SET);

	SPIEXP->state = (readWords[0] << 8) | readWords[1];								//combine 2 read bytes into single 16 bit value

	SPIPin->state =SPIEXP->state | SPIPin->Pin;

	return SPIPin->state;
}

//Polls all 16 pins of IO expander and saves state into IOEXP struct object. Returns state
uint16_t SPIIOEXP_Poll(SPIIOEXP_t* SPIEXP){

	uint8_t readWords[2];

	//Receive Port A and B data into readWords array
	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Receive(&hspi2, readWords, 2, 100);
	HAL_GPIO_WritePin(SPIEXP->cs_port, SPIEXP->cs_pin, GPIO_PIN_SET);

	SPIEXP->state = (readWords[0] << 8) | readWords[1];								//combine 2 read bytes into single 16 bit value

	return SPIEXP->state;
}

bool SPIIOEXP_getPinState(SPIIOXPin_t* SPIPin){

	SPIPin->state = SPIPin->IOEXP->state | SPIPin->Pin;
	return SPIPin->state;

}





/*
int IOEXP_ReadAll(IOEXP_t* IOX){
	uint8_t tempBuff[3];
	HAL_I2C_Master_Transmit(IOX->handle, IOX->address << 1, IOEXP_ReadAcc_CMD, 1, 50);
	HAL_I2C_Master_Receive(IOX->handle, (IOX->address << 1) | 1, tempBuff, 3, 50);
	return (tempBuff[0] << 16) | (tempBuff[1] << 8) | tempbuff[2];
}

uint8_t IOEXP_ReadPort(IOEXP_t* IOX, int port){
	uint8_t tempBuff[1];
	HAL_I2C_Master_Transmit(IOX->handle, IOX->address << 1, port, 1, 50);
	HAL_I2C_Masrer_Receive(IOX->handle, (IOX->address << 1) | 1, tempBuff, 1, 50);
	return tempBuff[1];
}

bool IOEXP_ReadPin(IOEXP_t* IOX, int pin){
	uint8_t port_data = IOEXP_ReadPort(IOX, pin/10);
	return (port_data >> (pin%10)) & 1;
}
*/


/*/////////////////////////////////////////////////////////////////////////////

IO expander functions End

*//////////////////////////////////////////////////////////////////////////////

/*/////////////////////////////////////////////////////////////////////////////

Test Mode functions Begin

*//////////////////////////////////////////////////////////////////////////////


void TestMode1(){
	////////////////////////////////////////////////////////////////////
	// Test Mode 1 Begin

	// Edge mounted LEDs on DRC will cycle. No need to do anything.

	//clear status LEDs
	IOEXP_SetPins(&IOEXP_SPDT, 0x0000F0, 0);

	//Set Status LEDs to indicate mode 1
	IOEXP_SetPins(&IOEXP_SPDT, 0x000010, 1);

	// Test Mode 1 End
	////////////////////////////////////////////////////////////////////
}

void TestMode2(){
	////////////////////////////////////////////////////////////////////
	// Test Mode 2 Begin

	// Set all pins to digital mode, and all pins will be input. DRC will cycle across all LEDs



	//90 pin IO Init to Inputs
	for (int i = 0; i < 90; i++){
		pin_SetMode(pins[i], PIN_INPUT);
	}

	//Set all LEDs to indicate digital mode by setting as inputs
	IOEXP_SetModes(&IOEXP_LED, 0xFFFF03, 1);

	//Set IO pins 62 and 63 to inputs.
	IOEXP_SetModes(&IOEXP_SPDT, 0x00000C, 1);

	//Set SPDT switches to analog mode.
	IOEXP_SetPins(&IOEXP_SPDT, 0xFFFF03, 0);

	//clear status LEDs
	IOEXP_SetPins(&IOEXP_SPDT, 0x0000F0, 0);

	//Set Status LEDs to indicate mode 2
	IOEXP_SetPins(&IOEXP_SPDT, 0x000020, 1);

	// Test Mode 2 End
	////////////////////////////////////////////////////////////////////
}

void TestMode3(){
	////////////////////////////////////////////////////////////////////
	// Test Mode 3 Begin

	// DRC Read IO Test. Test card will cyle all pins HIGH and LOW for DRC to capture.

	//90 pin IO Init to Outputs
	for (int i = 0; i < 90; i++){
		pin_SetMode(pins[i], PIN_OUTPUT);
	}

	//Set all LEDs to indicate digital mode by setting as inputs
	IOEXP_SetModes(&IOEXP_LED, 0xFFFF03, 1);

	//Set IO pins 62 and 63 to outputs.
	IOEXP_SetModes(&IOEXP_SPDT, 0x00000C, 0);

	//Set SPDT switches to analog mode.
	IOEXP_SetPins(&IOEXP_SPDT, 0xFFFF03, 0);

	//clear status LEDs
	IOEXP_SetPins(&IOEXP_SPDT, 0x0000F0, 0);

	//Set Status LEDs to indicate mode 3
	IOEXP_SetPins(&IOEXP_SPDT, 0x000030, 1);

	//90 pin IO LOW to HIGH transition
	for (int i = 0; i < 90; i++){
		pin_Write(pins[i], GPIO_PIN_SET);
		HAL_Delay(200);
	}

	//90 pin IO HIGH to LOW transition
	for (int i = 0; i < 90; i++){
		pin_Write(pins[i], GPIO_PIN_RESET);
		HAL_Delay(200);
	}



	// Test Mode 3 End
	////////////////////////////////////////////////////////////////////
}

void TestMode4(){
	////////////////////////////////////////////////////////////////////
	// Test Mode 4 Begin

	// DRC LSDAC Triangle Wave Test. Test Board will set all SPDT switches to analog mode and SPDT LEDs to DAC Mode


	//Set all LEDs to indicate analog mode by setting as outputs
	IOEXP_SetModes(&IOEXP_LED, 0xFFFF03, 0);

	//Set all LEDs to indicate Analog DAC mode by setting as 0
	IOEXP_SetPins(&IOEXP_LED, 0xFFFF03, LED_DAC_MODE);

	//Set all LEDs to indicate Analog ADC mode by setting as inputs. Pins 50 and 55 not connected to LS DACs
	IOEXP_SetPins(&IOEXP_LED, 0x004001, LED_ADC_MODE);

	//Set SPDT switches to analog mode.
	IOEXP_SetPins(&IOEXP_SPDT, 0xFFFF03, 0);

	//clear status LEDs
	IOEXP_SetPins(&IOEXP_SPDT, 0x0000F0, 0);

	//Set Status LEDs to indicate mode 4
	IOEXP_SetPins(&IOEXP_SPDT, 0x000040, 1);

	//90 pin IO HIGH to LOW transition
	for (int i = 0; i < 90; i++){
		pin_Write(pins[i], GPIO_PIN_RESET);
	}

	// Test Mode 4 End
	////////////////////////////////////////////////////////////////////
}

void TestMode5(){
	////////////////////////////////////////////////////////////////////
	// Test Mode 5 Begin

	// LSDAC Fast Square Wave Test. Test Board will set all SPDT switches to analog mode and SPDT LEDs to DAC Mode


	//Set all LEDs to indicate analog mode by setting as outputs
	IOEXP_SetModes(&IOEXP_LED, 0xFFFF03, 0);

	//Set all LEDs to indicate Analog DAC mode by setting as inputs
	IOEXP_SetPins(&IOEXP_LED, 0xFFFF03, LED_DAC_MODE);

	//Set all LEDs to indicate Analog ADC mode by setting as inputs. Pins 50 and 55 not connected to LS DACs
	IOEXP_SetPins(&IOEXP_LED, 0x004001, LED_ADC_MODE);

	//Set SPDT switches to analog mode.
	IOEXP_SetPins(&IOEXP_SPDT, 0xFFFF03, 0);

	//clear status LEDs
	IOEXP_SetPins(&IOEXP_SPDT, 0x0000F0, 0);

	//Set Status LEDs to indicate mode 5
	IOEXP_SetPins(&IOEXP_SPDT, 0x000050, 1);

	//90 pin IO HIGH to LOW transition
	for (int i = 0; i < 90; i++){
		pin_Write(pins[i], GPIO_PIN_RESET);
	}

	// Test Mode 5 End
	////////////////////////////////////////////////////////////////////
}

void TestMode6(){
	////////////////////////////////////////////////////////////////////
	// Test Mode 6 Begin

	// HS ADC/DAC loopback test for channel A. Test Board will set all SPDT switches to analog mode and SPDT LEDs to DRC ADC/DAC Mode


	//Set all LEDs to indicate analog mode by setting as outputs
	IOEXP_SetModes(&IOEXP_LED, 0xFFFF03, 0);

	//Set all LEDs to indicate ADC Mode
	IOEXP_SetPins(&IOEXP_LED, 0xFFFF03, LED_ADC_MODE);

	//Set Pins 21, 17, 33, 2 for DAC mode
	IOEXP_SetPins(&IOEXP_LED, 0xB10000, LED_DAC_MODE);

	//Set SPDT switches to analog mode.
	IOEXP_SetPins(&IOEXP_SPDT, 0xFFFF03, 0);

	//clear status LEDs
	IOEXP_SetPins(&IOEXP_SPDT, 0x0000F0, 0);

	//Set Status LEDs to indicate mode 6
	IOEXP_SetPins(&IOEXP_SPDT, 0x000060, 1);

	//90 pin IO HIGH to LOW transition
	for (int i = 0; i < 90; i++){
		pin_Write(pins[i], GPIO_PIN_RESET);
	}

	// Test Mode 6 End
	////////////////////////////////////////////////////////////////////
}

void TestMode7(){
	////////////////////////////////////////////////////////////////////
	// Test Mode 7 Begin

	// HS ADC/DAC loopback test for channel B. Test Board will set all SPDT switches to analog mode and SPDT LEDs to DRC ADC/DAC Mode


	//Set all LEDs to indicate analog mode by setting as outputs
	IOEXP_SetModes(&IOEXP_LED, 0xFFFF03, 0);

	//Set all LEDs to indicate ADC Mode
	IOEXP_SetPins(&IOEXP_LED, 0xFFFF03, LED_ADC_MODE);

	//Set Pins 21, 17, 33, 2 for DAC mode
	IOEXP_SetPins(&IOEXP_LED, 0x40B000, LED_DAC_MODE);

	//Set SPDT switches to analog mode.
	IOEXP_SetPins(&IOEXP_SPDT, 0xFFFF03, 0);

	//clear status LEDs
	IOEXP_SetPins(&IOEXP_SPDT, 0x0000F0, 0);

	//Set Status LEDs to indicate mode 7
	IOEXP_SetPins(&IOEXP_SPDT, 0x000070, 1);

	//90 pin IO HIGH to LOW transition
	for (int i = 0; i < 90; i++){
		pin_Write(pins[i], GPIO_PIN_RESET);
	}

	// Test Mode 7 End
	////////////////////////////////////////////////////////////////////
}

/*/////////////////////////////////////////////////////////////////////////////

Test Mode functions Begin

*//////////////////////////////////////////////////////////////////////////////



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
  int32_t timeout;
/* USER CODE END Boot_Mode_Sequence_0 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();
/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
if ( timeout < 0 )
{
Error_Handler();
}
/* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

  //IO expander initialization
  IOEXP_Init(&IOEXP_SPDT);
  IOEXP_Init(&IOEXP_LED);

  SPIIOEXP_Init(&SPIEXP0);
  SPIIOEXP_Init(&SPIEXP1);
  SPIIOEXP_Init(&SPIEXP2);
  SPIIOEXP_Init(&SPIEXP3);
  SPIIOEXP_Init(&SPIEXP4);
  SPIIOEXP_Init(&SPIEXP5);
  SPIIOEXP_Init(&SPIEXP6);
//  IOEXP_SetModes(&IOEXP_SPDT, 0x00000C, 0);
//  IOEXP_SetPins(&IOEXP_SPDT, 0x00000C, 0);
//  IOEXP_SetModes(&IOEXP_LED, 0xFFFF03, 1);
//  IOEXP_SetPins(&IOEXP_LED, 0xFFFF03, 1);

  SPIIOEXP_SetModes(&SPIEXP0, 0xFFFF, 0x0000);
  SPIIOEXP_SetModes(&SPIEXP1, 0xFFFF, 0x0000);
  SPIIOEXP_SetModes(&SPIEXP2, 0xFFFF, 0x0000);
  SPIIOEXP_SetModes(&SPIEXP3, 0xFFFF, 0x0000);
  SPIIOEXP_SetModes(&SPIEXP4, 0xFFFF, 0x0000);
  SPIIOEXP_SetModes(&SPIEXP5, 0xFFFF, 0x0000);
  SPIIOEXP_SetModes(&SPIEXP6, 0xFFFF, 0x0000);
  SPIIOEXP_WritePins(&SPIEXP0, 0xFFFF, 0x0000);
  SPIIOEXP_WritePins(&SPIEXP1, 0xFFFF, 0x0000);
  SPIIOEXP_WritePins(&SPIEXP2, 0xFFFF, 0x0000);
  SPIIOEXP_WritePins(&SPIEXP3, 0xFFFF, 0x0000);
  SPIIOEXP_WritePins(&SPIEXP4, 0xFFFF, 0x0000);
  SPIIOEXP_WritePins(&SPIEXP5, 0xFFFF, 0x0000);
  SPIIOEXP_WritePins(&SPIEXP6, 0xFFFF, 0x0000);

  SPIIOEXP_SetPin(&SPIPIN_SEL17, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL46, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL74, 1);//1
  SPIIOEXP_SetPin(&SPIPIN_SEL40, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL55, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL51, 1);//1
  SPIIOEXP_SetPin(&SPIPIN_SEL21, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL50, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL48, 1);//1
  SPIIOEXP_SetPin(&SPIPIN_SEL34, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL32, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL3, 1);//1
  SPIIOEXP_SetPin(&SPIPIN_SEL2, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL10, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL36, 1);//1
  SPIIOEXP_SetPin(&SPIPIN_SEL35, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL6, 0);
  SPIIOEXP_SetPin(&SPIPIN_SEL33, 1);//1

  //SPDT enable signals (ACTIVE LOW)
  SPIIOEXP_SetPin(&SPIPIN_SPDTEN, 0);
  SPIIOEXP_SetPin(&SPIPIN_SPDTEN2, 0);



  /*
   * SP8T select and enable signals for DAC0 port
   *
   * A2 A1 A0 Pin
   *  0  0  0 32
   *  0  0  1 3
   *  0  1  0 34
   *  0  1  1 2
   *  1  0  0 33
   *  1  0  1 36
   *  1  1  0 6
   *  1  1  1 35
   *
   */
  SPIIOEXP_SetPin(&SPIPIN_MDAC0_A0, 0);
  SPIIOEXP_SetPin(&SPIPIN_MDAC0_A1, 0);
  SPIIOEXP_SetPin(&SPIPIN_MDAC0_A2, 0);
  SPIIOEXP_SetPin(&SPIPIN_nMEN0, 0);


  /*
   * SP8T select and enable signals for DAC1 port
   *
   * A2 A1 A0 Pin
   *  0  0  0 40
   *  0  0  1 74
   *  0  1  0 46
   *  0  1  1 10
   *  1  0  0 17
   *  1  0  1 51
   *  1  1  0 48
   *  1  1  1 21
   *
   */
  SPIIOEXP_SetPin(&SPIPIN_MDAC1_A0, 0);
  SPIIOEXP_SetPin(&SPIPIN_MDAC1_A1, 0);
  SPIIOEXP_SetPin(&SPIPIN_MDAC1_A2, 0);
  SPIIOEXP_SetPin(&SPIPIN_nMEN1, 0);


  /*
   * SP8T select and enable signals for ADC port
   *
   * A2 A1 A0 Pin
   *  0  0  0 46
   *  0  0  1 17
   *  0  1  0 48
   *  0  1  1 33
   *  1  0  0 50
   *  1  0  1 55
   *  1  1  0 51
   *  1  1  1 21
   *
   */
  SPIIOEXP_SetPin(&SPIPIN_MADC_A0, 0);
  SPIIOEXP_SetPin(&SPIPIN_MADC_A1, 0);
  SPIIOEXP_SetPin(&SPIPIN_MADC_A2, 0);
  SPIIOEXP_SetPin(&SPIPIN_nMEN2, 0);

  int i = 0;
  while(1){
	  if(SPIPins[i]->is90Pin != 1) {
		  i++;
		  continue;
	  }
	  SPIIOEXP_SetPin(SPIPins[i], 0);
	  HAL_Delay(100);
	  i++;
	  i%=90;
  }

  //while(1);
  //90 pin connector IO initialization (
//  for (int i = 0; i < 90; i++){
//	  pin_Init(pins[i]);
//	  pin_SetMode(pins[i], PIN_OUTPUT);
//	  pin_Write(pins[i], GPIO_PIN_RESET);
//  }




  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  int modeCount = 0;
	  pin_SetMode(&PIN88, PIN_INPUT);
	  pin_SetMode(&PIN89, PIN_INPUT);
	  while(!pin_Read(&PIN89)){
		  while(!pin_Read(&PIN88)){
			  if(pin_Read(&PIN89)){
				  goto endCount;
			  }
		  }
		  while(pin_Read(&PIN88)){}
		  modeCount++;
	  }
	  endCount:

	  switch(modeCount){
	  case 0:
		  break;
	  case 1:
		  TestMode1();
		  break;
	  case 2:
		  TestMode2();
		  break;
	  case 3:
		  TestMode3();
		  break;
	  case 4:
		  TestMode4();
		  break;
	  case 5:
		  TestMode5();
		  break;
	  case 6:
		  TestMode6();
		  break;
	  case 7:
		  TestMode7();
		  break;
	  default:
		  break;
	  }
	  //printf("Wrong parameters value: file ");
//	  for(int i = 0; i < 90; i++){
//		  pin_Write(pins[i], GPIO_PIN_SET);
//		  HAL_Delay(100);
//	  }
//	  for(int i = 0; i < 90; i++){
//		  pin_Write(pins[i], GPIO_PIN_RESET);
//		  HAL_Delay(100);
//	  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 13;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOMEDIUM;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00707CBB;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x0;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 0x0;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi2.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi2.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi2.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi2.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi2.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi2.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi2.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi2.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi2.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 9;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.battery_charging_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */

  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12
                          |GPIO_PIN_13|GPIO_PIN_14, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pins : PE2 PE3 PE4 PE5
                           PE6 PE7 PE8 PE9
                           PE10 PE11 PE12 PE13
                           PE14 PE15 PE0 PE1 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PC13 PC0 PC4 PC5
                           PC6 PC7 PC8 PC9
                           PC10 PC11 PC12 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PF6 PF7 PF8 PF9
                           PF10 PF11 PF14 PF15 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA2 PA3
                           PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA4 PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 PB12
                           PB3 PB4 PB5 PB6
                           PB7 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_12
                          |GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PD8 PD10 PD11 PD12
                           PD13 PD14 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12
                          |GPIO_PIN_13|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PD9 PD15 PD0 PD1
                           PD2 PD3 PD4 PD5
                           PD6 PD7 */
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_15|GPIO_PIN_0|GPIO_PIN_1
                          |GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PG6 PG7 PG8 PG9
                           PG10 PG11 PG12 PG13
                           PG14 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
