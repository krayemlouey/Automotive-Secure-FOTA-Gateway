/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c  (NUCLEO-F446RE — ECU Simulateur Automobile & Passerelle OTA)
  * @brief   • CAN ID 0x100 @ 20ms  : Vitesse réelle (Potentiomètre PA0, 0..4095 -> 0..200 km/h)
  *          • CAN ID 0x101 @ 100ms : Environnement (Carburant 75%, Temp. 23°C, Humidité 42%)
  *          • CAN ID 0x102 @ 100ms : Radar Obstacle (Distance 200 cm)
  *          • Bouton B1 (PC13)     : Déclenche Alerte OTA v2.0.0 (CAN ID 0x180)
  *          • UART1 RX (PA10)      : Passerelle ESP32 OTA (115200 bauds)
  *          • LED LD2 (PA5)        : Clignotement 1 Hz (Témoin sain)
  *
  *  Brochage CAN  : PB8 = CAN1_RX, PB9 = CAN1_TX -> Transceiver SN65HVD230 (500 kbps)
  *  Potentiomètre : PA0 (ADC1_IN0)
  *  ESP32 UART    : PA10 = RX (fil depuis TX2 GPIO17 ESP32)
  *  Bouton B1     : PC13 (Interruption EXTI15_10)
  *  LED Témoin    : PA5 (LD2 Verte)
  *  Horloge       : HSI -> PLL -> 84 MHz (APB1 = 42 MHz)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include <string.h>
#include <stdio.h>

/* Buffers CAN */
CAN_HandleTypeDef        hcan1;
ADC_HandleTypeDef        hadc1;
TIM_HandleTypeDef        htim2;
static CAN_TxHeaderTypeDef can_tx_header;
static uint8_t             can_tx_data[8];
static uint32_t            can_tx_mailbox;

/* Déclencheur Alerte OTA via Bouton B1 ou ESP32 UART */
static volatile uint8_t  trigger_ota_alert = 0;
static volatile uint8_t  trigger_reset = 0;
static volatile uint8_t  trigger_direct_alert = 0;
static volatile uint32_t last_btn_press = 0;

/* Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART1_Init(void);
static uint16_t ADC_ReadPot(void);
static void CAN_Send(uint32_t id, uint8_t *data, uint8_t dlc);

/* Lecture ADC potentiomètre sur PA0 (<5 µs) */
static uint16_t ADC_ReadPot(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 2) == HAL_OK)
    {
        return (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    return 0;
}

/* Envoi CAN 2.0B non-bloquant : envoi direct si une boîte est libre */
static void CAN_Send(uint32_t id, uint8_t *data, uint8_t dlc)
{
    can_tx_header.StdId              = id;
    can_tx_header.ExtId              = 0;
    can_tx_header.RTR                = CAN_RTR_DATA;
    can_tx_header.IDE                = CAN_ID_STD;
    can_tx_header.DLC                = dlc;
    can_tx_header.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
    {
        HAL_CAN_AddTxMessage(&hcan1, &can_tx_header, data, &can_tx_mailbox);
    }
}

/* Interruption Bouton Bleu Utilisateur B1 (PC13) : Déclenche l'alerte OTA */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
    {
        uint32_t now = HAL_GetTick();
        if (now - last_btn_press > 300)
        {
            last_btn_press = now;
            trigger_ota_alert = 1;
        }
    }
}

/* ============================================================================
 * PROGRAMME PRINCIPAL
 * ============================================================================ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_CAN1_Init();
    MX_USART1_Init();

    HAL_Delay(50);
    HAL_CAN_Start(&hcan1);

    uint32_t last_tx_100 = 0;
    uint32_t last_tx_101 = 0;
    uint32_t last_led_toggle = 0;

    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* 1. Clignotement LED LD2 (PA5) à 1 Hz (ECU actif) */
        if (now - last_led_toggle >= 500)
        {
            last_led_toggle = now;
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        }

        /* 2. Émission CAN ID 0x100 (VITESSE RÉELLE) toutes les 20 ms (50 Hz = ZÉRO LATENCE) */
        if (now - last_tx_100 >= 20)
        {
            last_tx_100 = now;
            uint16_t pot = ADC_ReadPot();

            can_tx_data[0] = (uint8_t)(pot & 0xFF);
            can_tx_data[1] = (uint8_t)(pot >> 8);
            can_tx_data[2] = 0x00;
            can_tx_data[3] = 0x00;
            can_tx_data[4] = 0x00;
            can_tx_data[5] = 0x00;
            can_tx_data[6] = 0x00;
            can_tx_data[7] = can_tx_data[0] ^ can_tx_data[1];

            CAN_Send(0x100, can_tx_data, 8);
        }

        /* 3. Émission CAN ID 0x101 (ENVIRONNEMENT) et 0x102 (RADAR) toutes les 100 ms */
        if (now - last_tx_101 >= 100)
        {
            last_tx_101 = now;

            uint8_t env_data[8];
            env_data[0] = 75; // Carburant 75 %
            env_data[1] = 23; // Température 23 °C
            env_data[2] = 42; // Humidité 42 %
            env_data[3] = 0;
            env_data[4] = 0;
            env_data[5] = 0;
            env_data[6] = 0;
            env_data[7] = env_data[0] ^ env_data[1] ^ env_data[2];
            CAN_Send(0x101, env_data, 8);

            uint8_t obs_data[8];
            obs_data[0] = (uint8_t)(200 & 0xFF);
            obs_data[1] = (uint8_t)(200 >> 8);
            obs_data[2] = 0;
            obs_data[3] = 0;
            obs_data[4] = 0;
            obs_data[5] = 0;
            obs_data[6] = 0;
            obs_data[7] = obs_data[0] ^ obs_data[1];
            CAN_Send(0x102, obs_data, 8);
        }

        /* 4. Si Alerte OTA déclenchée : Rafale de 12 trames CAN ID 0x180 */
        if (trigger_ota_alert)
        {
            trigger_ota_alert = 0;
            uint8_t ota_alert_data[8];
            memset(ota_alert_data, 0, sizeof(ota_alert_data));
            ota_alert_data[0] = 0x02; // Version v2.0.0
            ota_alert_data[1] = 0x00;
            ota_alert_data[2] = 0x00;
            ota_alert_data[6] = 0xAA; // Signature valide
            ota_alert_data[7] = 0x55;

            for (int k = 0; k < 12; k++)
            {
                CAN_Send(0x180, ota_alert_data, 8);
                HAL_Delay(3);
            }
        }

        /* 5. Si Restauration V1 déclenchée : Rafale de trames CAN ID 0x190 */
        if (trigger_reset)
        {
            trigger_reset = 0;
            uint8_t reset_data[8] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0x55 };
            for (int k = 0; k < 12; k++)
            {
                CAN_Send(0x190, reset_data, 8);
                HAL_Delay(3);
            }
        }

        /* 6. Si Alerte Directe déclenchée : Rafale de trames CAN ID 0x170 */
        if (trigger_direct_alert)
        {
            trigger_direct_alert = 0;
            uint8_t alert_data[8] = { 0xAA, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
            for (int k = 0; k < 12; k++)
            {
                CAN_Send(0x170, alert_data, 8);
                HAL_Delay(3);
            }
        }
    }
}

/**
  * @brief System Clock Configuration (84 MHz)
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        RCC_OscInitStruct.HSIState = RCC_HSI_ON;
        RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
        RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
        RCC_OscInitStruct.PLL.PLLM = 16;
        RCC_OscInitStruct.PLL.PLLN = 336;
        RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
        RCC_OscInitStruct.PLL.PLLQ = 2;
        RCC_OscInitStruct.PLL.PLLR = 2;
        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        {
            Error_Handler();
        }
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_ADC1_Init(void)
{
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    ADC_ChannelConfTypeDef sc = {0};
    sc.Channel      = ADC_CHANNEL_0;
    sc.Rank         = 1;
    sc.SamplingTime = ADC_SAMPLETIME_28CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sc);
}

static void MX_CAN1_Init(void)
{
    hcan1.Instance = CAN1;
    hcan1.Init.Prescaler = 6;
    hcan1.Init.Mode = CAN_MODE_NORMAL;
    hcan1.Init.SyncJumpWidth = CAN_SJW_2TQ;
    hcan1.Init.TimeSeg1 = CAN_BS1_10TQ;
    hcan1.Init.TimeSeg2 = CAN_BS2_3TQ;
    hcan1.Init.TimeTriggeredMode = DISABLE;
    hcan1.Init.AutoBusOff = ENABLE;
    hcan1.Init.AutoWakeUp = DISABLE;
    hcan1.Init.AutoRetransmission = ENABLE;
    hcan1.Init.ReceiveFifoLocked = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;
    if (HAL_CAN_Init(&hcan1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* LED LD2 (PA5) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Bouton Bleu Utilisateur B1 (PC13) */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

static void MX_USART1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PA9 = TX, PA10 = RX */
    GPIOA->MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA->MODER |=  ((2U << (9 * 2)) | (2U << (10 * 2)));

    GPIOA->AFR[1] &= ~((0xFU << ((9 - 8) * 4)) | (0xFU << ((10 - 8) * 4)));
    GPIOA->AFR[1] |=  ((7U << ((9 - 8) * 4)) | (7U << ((10 - 8) * 4)));

    GPIOA->PUPDR |= (1U << (10 * 2));

    USART1->BRR = 84000000 / 115200;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);
}

/* Interruption Réception UART1 (Passerelle ESP32) */
void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE)
    {
        char c = (char)(USART1->DR & 0xFF);
        static char rx_buf[64];
        static uint8_t rx_idx = 0;

        if (rx_idx < sizeof(rx_buf) - 1)
        {
            rx_buf[rx_idx++] = c;
            rx_buf[rx_idx] = 0;
        }

        /* Détection immédiate du mot clé sans attendre \n */
        if (strstr(rx_buf, "RESET") != NULL || strstr(rx_buf, "reset") != NULL)
        {
            trigger_reset = 1;
            rx_idx = 0;
            rx_buf[0] = 0;
        }
        else if (strstr(rx_buf, "OTA") != NULL || strstr(rx_buf, "ota") != NULL ||
                 strstr(rx_buf, "v2.0.0") != NULL)
        {
            trigger_ota_alert = 1;
            rx_idx = 0;
            rx_buf[0] = 0;
        }
        else if (strstr(rx_buf, "ALERT") != NULL || strstr(rx_buf, "alert") != NULL)
        {
            trigger_direct_alert = 1;
            rx_idx = 0;
            rx_buf[0] = 0;
        }

        if (c == '\n' || c == '\r')
        {
            rx_idx = 0;
            rx_buf[0] = 0;
        }
    }
}
