/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "can_ids_model.c"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

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

FDCAN_HandleTypeDef hfdcan1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();
  /* Activate HSEM notification for Cortex-M4*/
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
  /*
  Domain D2 goes to STOP mode (Cortex-M4 in deep-sleep) waiting for Cortex-M7 to
  perform system initialization (system clock config, external memory configuration.. )
  */
  HAL_PWREx_ClearPendingEvent();
  HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);
  /* Clear HSEM flag */
  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));

/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();
  /* USER CODE BEGIN 2 */
  typedef struct {
    volatile uint32_t pot_raw;       /* Vitesse ou valeur pot */
    volatile uint32_t frame_count;   /* Nombre total de trames */
    volatile uint32_t status;        /* 0: Normal, 1: DoS, 2: Spoofing, 3: Fuzzing */
    volatile uint32_t last_tick;     /* Horodatage ms */
    volatile uint32_t can_id;        /* Dernier ID reçu */
    volatile uint32_t dlc;           /* Longueur reçue */
    volatile uint8_t  payload[8];    /* Données reçues en direct */
    volatile uint32_t delta_t;       /* Intervalle de temps en ms */
  } CAN_Shared_t;
  CAN_Shared_t *shared = (CAN_Shared_t *)0x38000000;
  shared->status = 0;
  shared->frame_count = 0;
  shared->pot_raw = 0;
  shared->can_id = 0x100;
  shared->delta_t = 50;

  FDCAN_RxHeaderTypeDef rx_header;
  uint8_t rx_data[8];
  uint32_t prev_tick = 0;
  uint32_t last_can_rx = 0;     /* Tick du dernier message CAN recu */
  uint32_t demo_tick = 0;       /* Tick derniere update demo */
  uint32_t demo_pot = 0;        /* Valeur simulee 0..4095 */
  int32_t  demo_dir = 1;        /* +1 montant, -1 descendant */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0)
    {
      if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK)
      {
        uint32_t now = HAL_GetTick();
        uint32_t dt = now - prev_tick;
        prev_tick = now;
        last_can_rx = now;

        uint32_t id = rx_header.Identifier;
        uint16_t pot = (uint16_t)rx_data[0] | ((uint16_t)rx_data[1] << 8);

        /* ========================================================
         * INFERENCE DU MODÈLE TINYML EN TEMPS RÉEL (CORTEX-M4)
         * Réseau de neurones multi-couches (Architecture Cube.AI)
         * ======================================================== */
        float in_dt  = (float)dt;
        float in_id  = (float)id;
        /* DataLength field: upper 16 bits = DLC code. Convert to byte count. */
        uint32_t dlc_raw = (rx_header.DataLength >> 16) & 0xF;
        float in_dlc = (float)dlc_raw;
        float in_pot = (float)pot;
        float in_tag = (float)rx_data[6];

        /* Inférence neurale directe ultra-rapide (<15 µs @ 240 MHz) */
        uint32_t attack_detected = (uint32_t)tinyml_predict_can_ids(in_dt, in_id, in_dlc, in_pot, in_tag);

        /* Enregistrement dans la mémoire partagée SRAM4 pour TouchGFX */
        shared->can_id = id;
        shared->dlc = dlc_raw;
        for (int i = 0; i < 8; i++) {
          shared->payload[i] = rx_data[i];
        }
        shared->delta_t = dt;
        shared->status = attack_detected;

        if (attack_detected == 0) {
          shared->pot_raw = pot;
        }

        shared->frame_count++;
        shared->last_tick = now;
      }
    }
    else
    {
      /* ============================================================
       * MODE DÉMO : aucune trame reçue depuis 500 ms
       * Génère un signal triangulaire 0→4095→0 toutes les ~4 s
       * pour valider l'affichage CM7 sans la Nucleo branchée.
       * ============================================================ */
      uint32_t now = HAL_GetTick();
      if ((now - last_can_rx) > 500U)
      {
        /* Mise à jour toutes les 20 ms → 200 pas → 4 secondes */
        if ((now - demo_tick) >= 20U)
        {
          demo_tick = now;
          demo_pot = (uint32_t)((int32_t)demo_pot + demo_dir * 20);
          if ((int32_t)demo_pot >= 4095) { demo_pot = 4095; demo_dir = -1; }
          if ((int32_t)demo_pot <= 0)    { demo_pot = 0;    demo_dir =  1; }

          shared->pot_raw     = demo_pot;
          shared->status      = 0;         /* Normal — fond vert */
          shared->can_id      = 0x100;
          shared->delta_t     = 50;
          shared->frame_count = (shared->frame_count + 1) & 0xFFFFFFFF;
          shared->last_tick   = now;
        }
      }
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = ENABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  /* 500kbps @ HSE=25MHz (FDCANSEL=00 -> HSE) :
   * NominalPrescaler = 1, SyncSeg = 1, TimeSeg1 = 39, TimeSeg2 = 10
   * Total Bit Time = 1 + 39 + 10 = 50 Tq.
   * BitRate = 25 000 000 / (1 * 50) = 500 000 bps = 500 kbps (Sample Point = 80%) */
  hfdcan1.Init.NominalPrescaler = 1;
  hfdcan1.Init.NominalSyncJumpWidth = 4;
  hfdcan1.Init.NominalTimeSeg1 = 39;
  hfdcan1.Init.NominalTimeSeg2 = 10;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 1;
  hfdcan1.Init.DataTimeSeg1 = 1;
  hfdcan1.Init.DataTimeSeg2 = 1;
  hfdcan1.Init.MessageRAMOffset = 0;
  hfdcan1.Init.StdFiltersNbr = 1;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.RxFifo0ElmtsNbr = 16;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxFifo1ElmtsNbr = 0;
  hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxBuffersNbr = 0;
  hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxEventsNbr = 0;
  hfdcan1.Init.TxBuffersNbr = 0;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 0;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */
  FDCAN_FilterTypeDef filter = {0};
  filter.IdType       = FDCAN_STANDARD_ID;
  filter.FilterIndex  = 0;
  filter.FilterType   = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1    = 0x000;
  filter.FilterID2    = 0x000;
  HAL_FDCAN_ConfigFilter(&hfdcan1, &filter);
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);
  HAL_FDCAN_Start(&hfdcan1);
  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
#ifdef USE_FULL_ASSERT
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
