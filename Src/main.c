
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2020 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_hal.h"

/* USER CODE BEGIN Includes */
#include <stdint.h>
#define GPIOA_IDR 0x40010808
#define GPIOB_ODR 0x40010C0C
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
uint32_t phy_to_dll_rx_bus = 0; // saves the data recived from the phy to phy and moves it to the dll_rx

uint8_t phy_to_dll_rx_bus_valid = 0; // checks if there is data to be transferd to the dll_rx from the phy layer

uint32_t dll_to_phy_tx_bus = 0; // saves the original data we want to transfer to the phy layer from the dll tx.

uint8_t dll_to_phy_tx_bus_valid = 0; // checks if theres data allready transferd from the dll to the [hy layer

uint8_t phy_tx_busy = 0; // checks if there is data being transmited at the moment.

int delay =0;
static uint8_t pr_clock_val=0;
static uint8_t p_clock_val = 0; 
static uint32_t Rx_value = 0;
static uint32_t Tx_value = 0;
static uint8_t Rx_clock_value ;
static uint8_t Tx_clock_value ;
static uint16_t t_data =0;
static uint32_t c_clock=0;
int ft_flag=1;
static uint32_t prv_clock=0;
int ft2_flag=1;
uint8_t data=0;
uint8_t interface_tx_flag=0;
uint8_t interface_rx_flag=0;
static uint32_t *GPIOA_IDR_Pointer = (uint32_t*)GPIOA_IDR;
static uint32_t *GPIOB_ODR_Pointer = (uint32_t*)GPIOB_ODR;
static uint16_t odr_temp=0;
static uint32_t tempi = 0;
static uint32_t tempi2 = 0;

uint8_t senders[] = {201,0,192,0,223};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_NVIC_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
void phy_Tx()
{
	static uint16_t shifter =1; // for the masking in order to iso the bit
	static uint8_t transfer = 0; // var to the masked bit // saves the prev clock value in order to check that we are in rising edge
	static uint8_t temp=0;	// takes the data from the bus
	Tx_clock_value = HAL_GPIO_ReadPin(phy_tx_clock_GPIO_Port, phy_tx_clock_Pin); // reads the current value of the clcok
	if(interface_tx_flag){  // checkes if we are in a new byte
		HAL_GPIO_WritePin(phy_tx_busy_GPIO_Port, phy_tx_busy_Pin, GPIO_PIN_SET); //set phy busy to 1
		HAL_TIM_Base_Start(&htim3); //turn on timer
		HAL_TIM_Base_Start_IT(&htim3);
		interface_tx_flag=0;
	}
	else if (Tx_clock_value && (!p_clock_val) && shifter <= 256) // checks if we are in a rising edge 
	{
			transfer = ((dll_to_phy_tx_bus) &	(shifter));
			if (transfer == shifter)
			{
				HAL_GPIO_WritePin (phy_tx_data_GPIO_Port, phy_tx_data_Pin, GPIO_PIN_SET);	//inserts one to the data
				Tx_value =1;
			}	
			else
			{
				HAL_GPIO_WritePin (phy_tx_data_GPIO_Port, phy_tx_data_Pin, GPIO_PIN_RESET); // inserts zero to data 
				Tx_value =0;
			}
			shifter = shifter*2;		
	}
	else if ( shifter > 256) // after the last bit
	{
		HAL_TIM_Base_Stop(&htim3);
		HAL_TIM_Base_Stop_IT(&htim3);
		HAL_GPIO_WritePin(phy_tx_clock_GPIO_Port, phy_tx_clock_Pin ,GPIO_PIN_RESET); //set clock to zero
		HAL_GPIO_WritePin(phy_tx_busy_GPIO_Port, phy_tx_busy_Pin, GPIO_PIN_RESET); //set phy busy to 1
		shifter =1; // reset the masker 
	}
	p_clock_val = Tx_clock_value; // current clock into previos clock
}


void phy_Rx()
{
	static uint32_t tempi = 0;
	static uint32_t tempi2 = 0;
	static uint32_t s_counter =1; // counter for inserting the number  // prev clock
	Rx_clock_value = HAL_GPIO_ReadPin(phy_rx_clock_GPIO_Port, phy_rx_clock_Pin); // reads the current clock value
	if ( (!Rx_clock_value) && pr_clock_val) // checks that we are in falling edge
	{
		Rx_value = HAL_GPIO_ReadPin (phy_rx_data_GPIO_Port, phy_rx_data_Pin); // reads the data from the pin
		tempi = Rx_value * s_counter;
		tempi2 += tempi;
		s_counter = s_counter*2;
	}
	if (s_counter > 256)
	{
		s_counter =1;
		phy_to_dll_rx_bus = tempi2;
		interface_rx_flag =1;
		tempi2=0;
	}
	pr_clock_val = Rx_clock_value;
	
}
	


/*void phy_Rx()
{
	static uint32_t tempi = 0;
	static uint32_t tempi2 = 0;
	static uint32_t s_counter =1; // counter for inserting the number  // prev clock
	Rx_clock_value = HAL_GPIO_ReadPin(phy_rx_clock_GPIO_Port, phy_rx_clock_Pin); // reads the current clock value
	if ( (!Rx_clock_value) && pr_clock_val) // checks that we are in falling edge
	{
		Rx_value = HAL_GPIO_ReadPin (phy_rx_data_GPIO_Port, phy_rx_data_Pin); // reads the data from the pin
		tempi2 = Rx_value * s_counter;
		phy_to_dll_rx_bus += tempi2;// moves the bit for the correct spot and inserts in to the bus
		s_counter *=2;
	}
	pr_clock_val = Rx_clock_value;
	if (s_counter > 128){
		s_counter = 1;
		interface_rx_flag=1;
	}
}*/


void interface()
{
	uint32_t efi =0; // just kidding efi num 1 also oren
	if (ft_flag)
	{
		HAL_TIM_Base_Start(&htim2);
		HAL_TIM_Base_Start_IT(&htim2);
		ft_flag=0;
		prv_clock=0;
	}
	c_clock=HAL_GPIO_ReadPin(interface_clock_GPIO_Port, interface_clock_Pin);
	dll_to_phy_tx_bus_valid=HAL_GPIO_ReadPin(dll_to_phy_tx_bus_valid_GPIO_Port, dll_to_phy_tx_bus_valid_Pin);
	if(!c_clock&&prv_clock&&dll_to_phy_tx_bus_valid)
	{
		dll_to_phy_tx_bus=(*GPIOA_IDR_Pointer & 255);
		interface_tx_flag=1;
	}
	else if ((c_clock)&&(!prv_clock))
		{
			if (ft2_flag)
			{
				HAL_GPIO_WritePin(phy_alive_GPIO_Port, phy_alive_Pin,GPIO_PIN_SET);
				ft2_flag=0;
			}
			phy_to_dll_rx_bus_valid=HAL_GPIO_ReadPin(phy_to_dll_rx_bus_valid_GPIO_Port, phy_to_dll_rx_bus_valid_Pin);
			if (phy_to_dll_rx_bus_valid)
			{
				HAL_GPIO_WritePin(phy_to_dll_rx_bus_valid_GPIO_Port, phy_to_dll_rx_bus_valid_Pin,GPIO_PIN_RESET);
			}
			if (interface_rx_flag)
			{
				efi = (phy_to_dll_rx_bus<<8) + (*GPIOB_ODR_Pointer & 255);
				*GPIOB_ODR_Pointer = efi;
				HAL_GPIO_WritePin(phy_to_dll_rx_bus_valid_GPIO_Port,phy_to_dll_rx_bus_valid_Pin,GPIO_PIN_SET);
				phy_to_dll_rx_bus_valid =1;
				//phy_to_dll_rx_bus=0;
				interface_rx_flag=0;
			}
		}
	prv_clock=c_clock;
	
}

/*void interface()
{
	static uint32_t *GPIOA_IDR_Pointer = (uint32_t*)GPIOA_IDR;
	static uint32_t *GPIOB_ODR_Pointer = (uint32_t*)GPIOB_ODR;
	static uint16_t odr_temp=0;
	if (ft_flag)
	{
	HAL_TIM_Base_Start(&htim2); //turn on timer
	HAL_TIM_Base_Start_IT(&htim2);
	ft_flag=0;
	}
	c_clock=HAL_GPIO_ReadPin(interface_clock_GPIO_Port, interface_clock_Pin);	
	if ((c_clock)&&(!prv_clock) && (ft2_flag))
	{
	  ft2_flag=0;
		HAL_GPIO_WritePin(phy_alive_GPIO_Port,phy_alive_Pin,GPIO_PIN_SET);
	}
	dll_to_phy_tx_bus_valid = HAL_GPIO_ReadPin(dll_to_phy_tx_bus_valid_GPIO_Port, dll_to_phy_tx_bus_valid_Pin);
	if(!c_clock&&prv_clock&&dll_to_phy_tx_bus_valid)
	{
		dll_to_phy_tx_bus=(*GPIOA_IDR_Pointer & mask);
		interface_tx_flag=1;
	}
	else if (c_clock&&!prv_clock) 
	{
		
		*GPIOB_ODR_Pointer=dll_to_phy_tx_bus;
		phy_to_dll_rx_bus_valid=HAL_GPIO_ReadPin(phy_to_dll_rx_bus_valid_GPIO_Port,phy_to_dll_rx_bus_valid_Pin);
		if (phy_to_dll_rx_bus_valid)
		{
			HAL_GPIO_WritePin(phy_to_dll_rx_bus_valid_GPIO_Port,phy_to_dll_rx_bus_valid_Pin,GPIO_PIN_RESET);
		}
		if (interface_rx_flag)
		{
			odr_temp=(*GPIOB_ODR_Pointer & mask);
			phy_to_dll_rx_bus = (phy_to_dll_rx_bus<<3) + (odr_temp);
			*GPIOB_ODR_Pointer = phy_to_dll_rx_bus;
			HAL_GPIO_WritePin(phy_to_dll_rx_bus_valid_GPIO_Port,phy_to_dll_rx_bus_valid_Pin,GPIO_PIN_SET);
			interface_rx_flag=0;
		}
	}
	prv_clock=c_clock;
}*/

void phy_layer()
{
	phy_Tx();
	phy_Rx();
	
}

void sampleClocks()
{
	p_clock_val=Tx_clock_value;
	pr_clock_val=Rx_clock_value;
	//prv_clock=c_clock;
	Tx_clock_value=HAL_GPIO_ReadPin(phy_tx_clock_GPIO_Port, phy_tx_clock_Pin);
	Rx_clock_value=HAL_GPIO_ReadPin(phy_rx_clock_GPIO_Port, phy_rx_clock_Pin);
	//c_clock=HAL_GPIO_ReadPin(interface_clock_GPIO_Port, interface_clock_Pin);
	//dll_to_phy_tx_bus_valid=HAL_GPIO_ReadPin(dll_to_phy_tx_bus_valid_GPIO_Port, dll_to_phy_tx_bus_valid_Pin);
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  *
  * @retval None
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

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
  MX_TIM2_Init();
  MX_TIM3_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */
	HAL_GPIO_WritePin(phy_tx_clock_GPIO_Port, phy_tx_clock_Pin ,GPIO_PIN_RESET); //set clock to zero
 
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		//sampleClocks();
		phy_layer();	
		interface();

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

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV16;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* TIM2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  /* TIM3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

/* TIM2 init function */
static void MX_TIM2_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 19999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* TIM3 init function */
static void MX_TIM3_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 19999;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, phy_tx_clock_Pin|phy_tx_data_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, dll_rx_2_Pin|dll_rx_3_Pin|dll_rx_4_Pin|dll_rx_5_Pin 
                          |dll_rx_6_Pin|dll_rx_7_Pin|interface_clock_Pin|phy_alive_Pin 
                          |phy_tx_busy_Pin|phy_to_dll_rx_bus_valid_Pin|dll_rx_0_Pin|dll_rx_1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : phy_tx_clock_Pin phy_tx_data_Pin */
  GPIO_InitStruct.Pin = phy_tx_clock_Pin|phy_tx_data_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : phy_rx_clock_Pin */
  GPIO_InitStruct.Pin = phy_rx_clock_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(phy_rx_clock_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : dll_tx_0_Pin dll_tx_1_Pin dll_tx_2_Pin dll_tx_3_Pin 
                           dll_tx_4_Pin dll_tx_5_Pin dll_tx_6_Pin dll_tx_7_Pin */
  GPIO_InitStruct.Pin = dll_tx_0_Pin|dll_tx_1_Pin|dll_tx_2_Pin|dll_tx_3_Pin 
                          |dll_tx_4_Pin|dll_tx_5_Pin|dll_tx_6_Pin|dll_tx_7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : phy_rx_data_Pin dll_to_phy_tx_bus_valid_Pin */
  GPIO_InitStruct.Pin = phy_rx_data_Pin|dll_to_phy_tx_bus_valid_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : dll_rx_2_Pin dll_rx_3_Pin dll_rx_4_Pin dll_rx_5_Pin 
                           dll_rx_6_Pin dll_rx_7_Pin interface_clock_Pin phy_alive_Pin 
                           phy_tx_busy_Pin phy_to_dll_rx_bus_valid_Pin dll_rx_0_Pin dll_rx_1_Pin */
  GPIO_InitStruct.Pin = dll_rx_2_Pin|dll_rx_3_Pin|dll_rx_4_Pin|dll_rx_5_Pin 
                          |dll_rx_6_Pin|dll_rx_7_Pin|interface_clock_Pin|phy_alive_Pin 
                          |phy_tx_busy_Pin|phy_to_dll_rx_bus_valid_Pin|dll_rx_0_Pin|dll_rx_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  file: The file name as string.
  * @param  line: The line in file as a number.
  * @retval None
  */
void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
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
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
