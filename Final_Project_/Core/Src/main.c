/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "string.h"  // library for C strings
#include <stdbool.h> // library for bool data type
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define I2C_ADDR 0x27 // I2C address of the PCF8574
#define RS_BIT 0 // Register select bit
#define EN_BIT 2 // Enable bit
#define BL_BIT 3 // Backlight bit
#define D4_BIT 4 // Data 4 bit
#define D5_BIT 5 // Data 5 bit
#define D6_BIT 6 // Data 6 bit
#define D7_BIT 7 // Data 7 bit
#define LCD_ROWS 2 // Number of rows on the LCD
#define LCD_COLS 16 // Number of columns on the LCD
#define UART_DELAY 100 // wait max of 100 ms between frames in message
#define MAX_MESSAGE_SIZE 100 // 100 characters maximum message size

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t backlight_state = 1;  // State of the black light of the LCD
uint8_t message[MAX_MESSAGE_SIZE]  = {0}; // char array to store message received
uint8_t response[MAX_MESSAGE_SIZE] = {0}; // char array to store response message
uint8_t uart2_byte; // byte received from UART2
uint8_t buffer_position = 0; // how many bytes received so far

// Variables for volate and percetange calculation
float battery_voltage;
float battery_percentage;
float nivev_measurement_max = 3640.1;
float onefivev_measurement_max = 603.0;
float threev_measurement_max = 1204.5;

// Variables for selecting the different types of batterys that will be measured
bool ninev_battery = false;
bool one_fivev_battery = false;
bool threev_battery = false;

// Variable that helps in resetting the system
bool reset_system = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM16_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

// Functions that help displaying the voltage and percentage
void display_voltage(float battery_voltage);
void display_charge(float battery_percentage);

// Functions that help calculating the voltage
float threev_voltage(float battery_percentage);
float onefivev_voltage(float battery_percentage);
float ninev_voltage(float battery_percentage);

// Functions that help calculating the percentage
float ninev_percentage(uint16_t analog_measurement);
float onefivev_percentage(uint16_t analog_measurement);
float threev_percentage(uint16_t analog_measurement);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* THESE FUNCTIONS HELP IN SETTING UP THE LCD */

// This function helps in creating the data or command htat will be sent
void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
	uint8_t data = nibble << D4_BIT;
	data |= rs << RS_BIT;
	data |= backlight_state << BL_BIT; // Include backlight state in data
	data |= 1 << EN_BIT;
	HAL_I2C_Master_Transmit(&hi2c1, I2C_ADDR << 1, &data, 1, 100);
	HAL_Delay(1);
	data &= ~(1 << EN_BIT);
	HAL_I2C_Master_Transmit(&hi2c1, I2C_ADDR << 1, &data, 1, 100);
}

// THis function helps in sending the command into the LCD
void lcd_send_cmd(uint8_t cmd)
{
	uint8_t upper_nibble = cmd >> 4;
	uint8_t lower_nibble = cmd & 0x0F;
	lcd_write_nibble(upper_nibble, 0);
	lcd_write_nibble(lower_nibble, 0);

	if (cmd == 0x01 || cmd == 0x02)
	{
		HAL_Delay(2);
	}

}

// This function helps in sending the data into the LCD
void lcd_send_data(uint8_t data)
{
	uint8_t upper_nibble = data >> 4;
	uint8_t lower_nibble = data & 0x0F;
	lcd_write_nibble(upper_nibble, 1);
	lcd_write_nibble(lower_nibble, 1);
}

// This function helps in initialzing the LCD
void lcd_init()
{
	HAL_Delay(50);
	lcd_write_nibble(0x03, 0);
	HAL_Delay(5);
	lcd_write_nibble(0x03, 0);
	HAL_Delay(1);
	lcd_write_nibble(0x03, 0);
	HAL_Delay(1);
	lcd_write_nibble(0x02, 0);
	lcd_send_cmd(0x28);
	lcd_send_cmd(0x0C);
	lcd_send_cmd(0x06);
	lcd_send_cmd(0x01);
	HAL_Delay(2);
}

// This function helps in writing into the LCD
void lcd_write_string(char *str)
{
	while (*str)
	{
		lcd_send_data(*str++);
	}
}

// This function helps in the setting the cursor on the LCD
void lcd_set_cursor(uint8_t row, uint8_t column)
{
	uint8_t address;
	switch (row)
	{
		case 0:
		address = 0x00;
		break;
		case 1:
		address = 0x40;
		break;
		default:
		address = 0x00;
	}

	address += column;
	lcd_send_cmd(0x80 | address);
}

// This function clears the LCD
void lcd_clear(void)
{
	lcd_send_cmd(0x01);
	HAL_Delay(2);
}

// This function sets the state of the backlight
void lcd_backlight(uint8_t state)
{
	if (state)
	{
		backlight_state = 1;
	}

	else
	{
		backlight_state = 0;
	}
}

// LCD DISPLAY FUNCTIONS

// Function to display the voltage on the LCD
void display_voltage(float battery_voltage) {
    char buffer[16];
    sprintf(buffer, "Voltage: %.1fV", battery_voltage);  // Format the voltage string
    lcd_set_cursor(0, 0);  // Set cursor to the first row
    lcd_write_string(buffer);  // Display the voltage value on the LCD

}

// Function to display the charge percentage on the LCD
void display_charge(float battery_percentage) {
    char buffer[16];
    sprintf(buffer, "Charge: %.1f%%", battery_percentage);  // Format the charge percentage string
    lcd_set_cursor(1, 0);  // Set cursor to the second row
    lcd_write_string(buffer);  // Display the charge percentage on the LCD

}

// END LCD DISPLAY FUNCTIONS

// 9 VOLTS BATTERY CALCULATION FUNCTIONS //

// This function calculates the percentage for nine volt batteries
float ninev_percentage(uint16_t analog_measurement)
{
	  // Calculating percentage
	  battery_percentage = (analog_measurement / nivev_measurement_max ) * 100;

	  // Return value
	  return battery_percentage;

} // End of function

// This function calculates the volts for nine volt batteries
float ninev_voltage(float battery_percentage)
{
	  // Calculating voltage
	  battery_voltage = (battery_percentage * 9.0) / 100;

	  // Return value
	  return battery_voltage;

}	// End of function

// END OF 9 VOLTS BATTERY CALCULATION FUNCTIONS //

// 1.5 VOLTS BATTERY CALCULATION FUNCTIONS //

// This function calculates the percentage for 1.5 volt batteries
float onefivev_percentage(uint16_t analog_measurement)
{
	  // Calculating percentage
	  battery_percentage = (analog_measurement / onefivev_measurement_max ) * 100;

	  // Return value
	  return battery_percentage;

}	// End of function

// This function calculates the volts for 1.5 volt batteries
float onefivev_voltage(float battery_percentage)
{
	  // Calculating voltage
	  battery_voltage = (battery_percentage * 1.5) / 100;

	  // Return value
	  return battery_voltage;

}	// End of function

// END OF 1.5 VOLTS BATTERY CALCULATION FUNCTIONS //

// 3 VOLTS BATTERY CALCULATION FUNCTIONS //

// This function calculates the percentage for 3 volt batteries
float threev_percentage(uint16_t analog_measurement)
{
	  // Calculating percentage
	  battery_percentage = (analog_measurement / threev_measurement_max ) * 100;

	  // Return value
	  return battery_percentage;

} // End of function

// This function calculates the voltage for 3 volt batteries
float threev_voltage(float battery_percentage)
{
	  // Calculating voltage
	  battery_voltage = (battery_percentage * 3.0) / 100;

	  // Return value
	  return battery_voltage;

} // End of function

// END OF 3 VOLTS BATTERY CALCULATION FUNCTIONS //

// This function makes sure that the percentage values are valid percentages
float percentage_bounds(float battery_percentage)
{
	  // If that checks if the battery percentage is less than a valid number
	  if (battery_percentage < 0)
	  {
		  // Percentage is 0
		  battery_percentage = 0.0;

		  // Return correct value
		  return battery_percentage;

	  } // End of if

	  // If that checks if the battery percentage is greater than a valid number
	  else if (battery_percentage > 100)
	  {
		  // Percentage is 100
		  battery_percentage = 100.0;

		  // Return the correct value
		  return battery_percentage;

	  } // End of else if

	  // Return the good value
	  return battery_percentage;

} // End of function

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM16_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_Base_Start_IT(&htim16); // Start Timer16

  // I2C pull-up resistors
  GPIOB->PUPDR |= 0b01 << (8*2);
  GPIOB->PUPDR |= 0b01 << (9*2);

  // Initialize the LCD
  lcd_init();
  lcd_backlight(1); // Turn on backlight

  HAL_UART_Receive_IT(&huart2, &uart2_byte, 1); // put byte from UART2 in "uart2_byte"

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  // Start ADC Conversion
	  HAL_ADC_Start(&hadc1);

	  // Wait for ADC conversion to complete
	  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);

	  // Read ADC value
	  uint16_t analog_measurement = HAL_ADC_GetValue(&hadc1);

	  // Delay 1 ms
	  HAL_Delay(1);


	  // If the view 9v battery message was displayed
	  if (ninev_battery)
	  {
		  // Calculate the percentage
		  battery_percentage = ninev_percentage(analog_measurement);

		  // Correct bounds of battery percentage
		  battery_percentage = percentage_bounds(battery_percentage);

		  // Calculate the voltage
		  battery_voltage = ninev_voltage(battery_percentage);

		  // if the other types were activated
		  if (one_fivev_battery || threev_battery)
		  {
			  // Clear LCD and turn off other battery types
			  lcd_clear();
			  one_fivev_battery = false;
			  threev_battery = false;

		  } // End of if

		  // Show the voltage on LCD
	      display_voltage(battery_voltage);
		  // Show the charge percentage on LCD
		  display_charge(battery_percentage);

	  } // End of if

	  // If the view 1.5v battery message was displayed
	  if (one_fivev_battery)
	  {
		  // Calculate the percentage
		  battery_percentage = onefivev_percentage(analog_measurement);

		  // Correct bounds of battery percentage
		  battery_percentage = percentage_bounds(battery_percentage);

		  // Calculate the voltage
		  battery_voltage = onefivev_voltage(battery_percentage);

		  // if the other types were activated
		  if (ninev_battery || threev_battery)
		  {
			  // Clear LCD and turn off other battery types
			  lcd_clear();
			  ninev_battery = false;
			  threev_battery = false;

		  } // End of if

		  // SHow the voltage on LCD
	      display_voltage(battery_voltage);
		  // Show the charge percentage on LCD
		  display_charge(battery_percentage);

	  } // End of if

	  // If the view 9v battery message was displayed
	  if (threev_battery)
	  {
		  // Calculate the percentage
		  battery_percentage = threev_percentage(analog_measurement);

		  // Correct bounds of battery percentage
		  battery_percentage = percentage_bounds(battery_percentage);

		  // Calculate the voltage
		  battery_voltage = threev_voltage(battery_percentage);

		  // if the other types were activated
		  if (one_fivev_battery || ninev_battery)
		  {
			  // Clear LCD and turn off other battery types
			  lcd_clear();
			  one_fivev_battery = false;
			  ninev_battery = false;

		  } // End of if

		  // Show the voltage on LCD
	      display_voltage(battery_voltage);
		  // Show the charge percentage on LCD
		  display_charge(battery_percentage);

	  } // End of if

	  // If the reset sequence was activated
	  if(reset_system)
	  {
		  // Clear the screen
		  lcd_clear();

		  // Stop reset sequence
		  reset_system = false;

	  } // End of if

  } // End of while loop

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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10D19CE4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 7999;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 999;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, NINEV_LED_Pin|ONEFIVE_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, THREEV_LED_Pin|CHARGE_LED_Pin|VOLTAGE_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : NINEV_LED_Pin ONEFIVE_LED_Pin */
  GPIO_InitStruct.Pin = NINEV_LED_Pin|ONEFIVE_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : THREEV_LED_Pin CHARGE_LED_Pin VOLTAGE_LED_Pin */
  GPIO_InitStruct.Pin = THREEV_LED_Pin|CHARGE_LED_Pin|VOLTAGE_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){

    // Check if byte received was on UART2 (from laptop)
    if (huart == &huart2)
    {

        // If we get here, it means we received a byte from UART2 and it was placed in "uart2_byte"

        // If uart2_byte isn't \r, \n, or \0, it means the message isn't over yet
        if ((uart2_byte != '\r') && (uart2_byte != '\n') && (uart2_byte != '\0')){

            // Add uart2_byte to the message
            message[buffer_position] = uart2_byte;
            buffer_position++;

     }

     else
     {
            // If we get here, it means we received \r, \n, or \0 and the message is over

            // Check message
            if (strcmp((char*)message, "VIEW_9V") == 0)
            {
            	// Activate 9v battery calculation sequence
            	ninev_battery = true;

                // Deactivate not needed LEDs and activate LED needed
            	HAL_GPIO_WritePin(NINEV_LED_GPIO_Port, NINEV_LED_Pin, 1);
            	HAL_GPIO_WritePin(ONEFIVE_LED_GPIO_Port, ONEFIVE_LED_Pin, 0);
            	HAL_GPIO_WritePin(THREEV_LED_GPIO_Port, THREEV_LED_Pin, 0);

                // Prepare response message
                strncpy((char*)response, "Task complete - 9V Battery values displayed.\n\r", MAX_MESSAGE_SIZE);

            } // End of if

            // Check message
            else if (strcmp((char*)message, "VIEW_1.5V") == 0)
            {
            	// Activate 1.5v battery calculation sequence
            	one_fivev_battery = true;

                // Deactivate not needed LEDs and activate LED needed
            	HAL_GPIO_WritePin(NINEV_LED_GPIO_Port, NINEV_LED_Pin, 0);
            	HAL_GPIO_WritePin(ONEFIVE_LED_GPIO_Port, ONEFIVE_LED_Pin, 1);
            	HAL_GPIO_WritePin(THREEV_LED_GPIO_Port, THREEV_LED_Pin, 0);

            	// Prepare response message
                strncpy((char*)response, "Task complete - 1.5V Battery values displayed.\n\r", MAX_MESSAGE_SIZE);

            } // End of else if

            // Check message
			else if (strcmp((char*)message, "VIEW_3V") == 0)
			{
            	// Activate 3v battery calculation sequence
            	threev_battery = true;

                // Deactivate not needed LEDs and activate LED needed
            	HAL_GPIO_WritePin(NINEV_LED_GPIO_Port, NINEV_LED_Pin, 0);
            	HAL_GPIO_WritePin(ONEFIVE_LED_GPIO_Port, ONEFIVE_LED_Pin, 0);
            	HAL_GPIO_WritePin(THREEV_LED_GPIO_Port, THREEV_LED_Pin, 1);

				// Prepare response message
				strncpy((char*)response, "Task complete - 3V Battery values displayed.\n\r", MAX_MESSAGE_SIZE);

			} // End of else if

            // Check message
			else if (strcmp((char*)message, "RESET") == 0)
			{
            	// Deactivate 3v battery calculation sequence
            	threev_battery = false;
            	// Deactivate 1.5v battery calculation sequence
            	one_fivev_battery = false;
            	// Deactivate 9v battery calculation sequence
            	ninev_battery = false;

                // Deactivate all LEDs
            	HAL_GPIO_WritePin(NINEV_LED_GPIO_Port, NINEV_LED_Pin, 0);
            	HAL_GPIO_WritePin(ONEFIVE_LED_GPIO_Port, ONEFIVE_LED_Pin, 0);
            	HAL_GPIO_WritePin(THREEV_LED_GPIO_Port, THREEV_LED_Pin, 0);

            	// Activate reset sequence
            	reset_system = true;

				// Prepare response message
				strncpy((char*)response, "Task complete - System successfully reset.\n\r", MAX_MESSAGE_SIZE);

			} // End of else if

            // Else if the message was not valid
             else {

                // Else message was not recognized
                strncpy((char*)response, "Task not recognized.\n\r", MAX_MESSAGE_SIZE);

             } // End of else

            // Send the response message to laptop
            HAL_UART_Transmit(&huart2, response, strlen(response), UART_DELAY);

            // Zero out message array and response array to get ready for the next message
            memset(message,  0, sizeof(message));
            memset(response, 0, sizeof(response));
            buffer_position = 0;

        }

        // Restart UART2's receive interrupt to wait for next byte
        HAL_UART_Receive_IT(&huart2, &uart2_byte, 1); //start next byte receive interrupt
    }
}


/* USER CODE END 4 */

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
