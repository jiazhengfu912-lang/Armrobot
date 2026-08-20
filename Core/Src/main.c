/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * 调试版说明：
  * 1. USART1 -> HTD-85H 调试板，115200
  * 2. USART2 -> 串口调试输出 / 蓝牙调试输出
  * 3. PA6 / PA7 -> L298N IN1 / IN2
  * 4. PB14 -> 推杆编码器白线 A 相中断输入
  * 5. 上电/复位后：
  *    先执行一次 Pushrod_Retract()，完成后再进入自动循环
  * 6. 联动逻辑：
  *    HOME -> STEP1 时推杆同步伸出
  *    STEP1 -> STEP2 时推杆同步缩回
  * 7. 连续执行 3 次，停顿 6 秒，再循环
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "htd85h_servo.h"
#include "pushrod.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 两个 HTD-85H 舵机 ID，根据你的实际 ID 修改 */
#define SERVO_ID_1                  1
#define SERVO_ID_2                  2

/* 舵机动作位置参数 */
#define SERVO1_POS_STEP1            700
#define SERVO2_POS_STEP1            590

#define SERVO1_POS_STEP2            900
#define SERVO2_POS_STEP2            533

#define SERVO1_POS_HOME             900
#define SERVO2_POS_HOME             533

/* 舵机动作时间与等待时间
 * 数值越大，舵机转动越慢
 */
#define SERVO_MOVE_TIME_MS          1500
#define SERVO_WAIT_MS               1800

/* 推杆单次联动位移脉冲数 */
#define PUSHROD_TARGET_PULSE        1750

/* 上电稳定等待 */
#define STARTUP_DELAY_MS            1000

/* 各动作之间的间隔 */
#define ACTION_GAP_MS               500

/* 新需求：每轮动作重复次数、轮间停顿 */
#define SERVO_REPEAT_TIMES          3
#define SERVO_CYCLE_DELAY_MS        6000

/* 上电首次回缩初始化参数
 * 上电阶段只允许回缩，不允许伸出
 */
#define PUSHROD_HOME_MIN_RETRACT_MS 1200U   /* 上电后至少先持续回缩这么久，确保确实执行了一次 Pushrod_Retract() */
#define PUSHROD_HOME_QUIET_MS       300U    /* 超过该静默时间且已过最小回缩时间，认为已经缩到 HOME 端 */
#define PUSHROD_HOME_TIMEOUT_MS     8000U   /* 回 HOME 总超时保护 */

/* 蓝牙/串口2命令定义（当前主循环未使用，仅保留） */
#define BT_CMD_HELP                 'H'
#define BT_CMD_EXTEND               'E'
#define BT_CMD_RETRACT              'R'
#define BT_CMD_STOP                 'S'
#define BT_CMD_ACTION               'A'

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* 串口2单字节接收缓冲 */
uint8_t bt_rx_data = 0;

/* 打印缓冲区 */
char debug_buf[128];

/* 推杆是否已完成上电首次回缩初始化 */
volatile uint8_t g_pushrod_inited = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static void Debug_Print(const char *str);
static void Debug_Printf(const char *fmt, ...);
static uint8_t Pushrod_Startup_Retract_Blocking(void);
static void Run_Startup_Prepare(void);
static void Run_ServoPushrod_Action_Once(void);
static void Run_ServoPushrod_Action_ThreeTimes(void);
static void Pushrod_RunToTarget(uint8_t dir, uint32_t target_pulse);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  串口2打印字符串
 * @note   当前默认 USART2 接串口助手/蓝牙
 */
static void Debug_Print(const char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, (uint16_t)strlen(str), 200);
}

/**
 * @brief  串口2格式化打印
 */
static void Debug_Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(debug_buf, sizeof(debug_buf), fmt, args);
    va_end(args);

    HAL_UART_Transmit(&huart2, (uint8_t *)debug_buf, (uint16_t)strlen(debug_buf), 200);
}

/**
 * @brief  按目标脉冲数驱动推杆运动
 * @param  dir          推杆方向：PUSHROD_DIR_EXTEND / PUSHROD_DIR_RETRACT
 * @param  target_pulse 目标脉冲数
 * @note
 * 1. 依赖 PB14 编码器中断累计 g_encoder_count
 * 2. 当前只使用 A 相白线，单通道计数
 * 3. 为防止编码器没进中断造成死循环，这里加入超时保护
 */
static void Pushrod_RunToTarget(uint8_t dir, uint32_t target_pulse)
{
    uint32_t tick_start = HAL_GetTick();
    uint32_t timeout_ms = 5000U;

    Pushrod_ResetEncoder();

    if (dir == PUSHROD_DIR_EXTEND)
    {
        Debug_Printf("[PUSHROD] Start EXTEND, target pulse = %lu\r\n", target_pulse);
        Pushrod_Extend();
    }
    else if (dir == PUSHROD_DIR_RETRACT)
    {
        Debug_Printf("[PUSHROD] Start RETRACT, target pulse = %lu\r\n", target_pulse);
        Pushrod_Retract();
    }
    else
    {
        Debug_Print("[PUSHROD] Invalid direction, stop\r\n");
        Pushrod_Stop();
        return;
    }

    while (g_pushrod_stop_flag == 0U)
    {
        if (g_encoder_count >= target_pulse)
        {
            g_pushrod_stop_flag = 1U;
        }

        if ((HAL_GetTick() - tick_start) > timeout_ms)
        {
            Debug_Printf("[PUSHROD] Timeout! encoder = %lu\r\n", g_encoder_count);
            break;
        }
    }

    Pushrod_Stop();
    Debug_Printf("[PUSHROD] Stop, final encoder = %lu\r\n", g_encoder_count);
}

/**
 * @brief  上电阶段执行一次 Pushrod_Retract()，并在完成后建立位置零点
 * @retval 1: 初始化成功  0: 初始化失败
 * @note
 * 1. 当前没有 HOME 限位开关，因此这里只能采用“调用 Pushrod_Retract() + 无脉冲静默”判定完成
 * 2. 本函数用于执行一次完整的 Pushrod_Retract() 启动回缩流程
 * 3. 回缩完成后，执行 Pushrod_ResetEncoder()，将当前位置定义为 HOME=0
 * 4. 本阶段未完成前，舵机禁止动作
 */
static uint8_t Pushrod_Startup_Retract_Blocking(void)
{
    uint32_t tick_start;
    uint32_t last_pulse_tick;
    uint32_t last_encoder;
    uint32_t now_tick;

    Debug_Print("[STARTUP] Pushrod_Retract begin\r\n");

    g_pushrod_inited = 0U;
    Pushrod_Stop();
    HAL_Delay(100);

    /* 先清空计数，后续用“计数是否继续变化”判断推杆是否还在运动 */
    Pushrod_ResetEncoder();

    tick_start = HAL_GetTick();
    last_pulse_tick = tick_start;
    last_encoder = g_encoder_count;

    /* 上电初始化只能往回缩，并且必须先连续执行一段时间 */
    Pushrod_Retract();

    while (1)
    {
        now_tick = HAL_GetTick();

        /* 编码器有变化，说明推杆还在运动 */
        if (g_encoder_count != last_encoder)
        {
            last_encoder = g_encoder_count;
            last_pulse_tick = now_tick;
        }

        /*
         * 先保证 Pushrod_Retract() 至少持续执行一段时间，
         * 防止上电初期还没来得及产生脉冲就被“静默判定”提前结束。
         */
        if ((now_tick - tick_start) >= PUSHROD_HOME_MIN_RETRACT_MS)
        {
            /* 一段时间没有新脉冲，认为已经到 HOME 端 */
            if ((now_tick - last_pulse_tick) > PUSHROD_HOME_QUIET_MS)
            {
                Pushrod_Stop();

                /* 关键：到 HOME 后，正式建立零位 */
                Pushrod_ResetEncoder();
                g_pushrod_inited = 1U;

                Debug_Print("[STARTUP] Pushrod_Retract done, HOME = 0\r\n");
                return 1U;
            }
        }

        /* 总超时保护：防止方向接反或编码器异常导致死循环 */
        if ((now_tick - tick_start) > PUSHROD_HOME_TIMEOUT_MS)
        {
            Pushrod_Stop();
            g_pushrod_inited = 0U;
            Debug_Print("[STARTUP] Pushrod_Retract timeout\r\n");
            return 0U;
        }
    }
}

/**
 * @brief  上电/复位后的启动准备
 * @note
 * 1. 先等待系统稳定
 * 2. 上电后先连续执行两次 Pushrod_Retract()
 * 3. 每次都通过“无脉冲静默 + 超时保护”判断本次回缩完成
 * 4. 第二次回缩完成后建立 HOME 零位
 * 5. 然后才允许舵机回 HOME 并进入自动循环
 */
static void Run_Startup_Prepare(void)
{
    Debug_Print("\r\n==============================\r\n");
    Debug_Print("[SYS] Power On / Reset\r\n");
    Debug_Print("[SYS] Wait startup delay...\r\n");

    HAL_Delay(STARTUP_DELAY_MS);

    /* 第一步：上电后先连续执行两次回缩 */
    Debug_Print("[SYS] Startup retract #1 begin\r\n");
    if (Pushrod_Startup_Retract_Blocking() == 0U)
    {
        Debug_Print("[SYS] ERROR: Pushrod startup retract #1 failed, servo locked\r\n");
        while (1)
        {
            Pushrod_Stop();
            HAL_Delay(1000);
        }
    }

    HAL_Delay(ACTION_GAP_MS);

    Debug_Print("[SYS] Startup retract #2 begin\r\n");
    if (Pushrod_Startup_Retract_Blocking() == 0U)
    {
        Debug_Print("[SYS] ERROR: Pushrod startup retract #2 failed, servo locked\r\n");
        while (1)
        {
            Pushrod_Stop();
            HAL_Delay(1000);
        }
    }

    /* 第二步：两次回缩完成后，舵机才允许运动 */
    Debug_Print("[SYS] Startup double retract OK, servo HOME begin\r\n");

    HTD85H_Move(&huart1, SERVO_ID_1, SERVO1_POS_HOME, SERVO_MOVE_TIME_MS);
    HTD85H_Move(&huart1, SERVO_ID_2, SERVO2_POS_HOME, SERVO_MOVE_TIME_MS);
    HAL_Delay(SERVO_WAIT_MS);

    HAL_Delay(ACTION_GAP_MS);

    Debug_Print("[SYS] Startup prepare done\r\n");
    Debug_Print("==============================\r\n");
}

/**
 * @brief  单次联动动作
 * @note
 * 1. 从 HOME 出发，两个舵机去 STEP1，同时推杆伸出
 * 2. 两个舵机从 STEP1 去 STEP2，同时推杆缩回 HOME
 * 3. 两个舵机再从 STEP2 回 HOME
 */
static void Run_ServoPushrod_Action_Once(void)
{
    uint32_t tick_start;
    uint32_t elapsed_ms;

    if (g_pushrod_inited == 0U)
    {
        Debug_Print("[SYNC] ERROR: Pushrod not initialized\r\n");
        return;
    }

    /* --------------------------------------------------
     * 第 1 步：HOME -> STEP1，同时推杆伸出
     * -------------------------------------------------- */
    Debug_Print("[SYNC] Step1 begin -> Servo HOME->STEP1, Pushrod EXTEND\r\n");

    HTD85H_Move(&huart1, SERVO_ID_1, SERVO1_POS_STEP1, SERVO_MOVE_TIME_MS);
    HTD85H_Move(&huart1, SERVO_ID_2, SERVO2_POS_STEP1, SERVO_MOVE_TIME_MS);

    tick_start = HAL_GetTick();
    Pushrod_RunToTarget(PUSHROD_DIR_EXTEND, PUSHROD_TARGET_PULSE);
    elapsed_ms = HAL_GetTick() - tick_start;

    if (elapsed_ms < SERVO_WAIT_MS)
    {
        HAL_Delay(SERVO_WAIT_MS - elapsed_ms);
    }

    Debug_Print("[SYNC] Step1 done\r\n");
    HAL_Delay(ACTION_GAP_MS);

    /* --------------------------------------------------
     * 第 2 步：STEP1 -> STEP2，同时推杆缩回 HOME
     * -------------------------------------------------- */
    Debug_Print("[SYNC] Step2 begin -> Servo STEP1->STEP2, Pushrod RETRACT\r\n");

    HTD85H_Move(&huart1, SERVO_ID_1, SERVO1_POS_STEP2, SERVO_MOVE_TIME_MS);
    HTD85H_Move(&huart1, SERVO_ID_2, SERVO2_POS_STEP2, SERVO_MOVE_TIME_MS);

    tick_start = HAL_GetTick();
    Pushrod_RunToTarget(PUSHROD_DIR_RETRACT, PUSHROD_TARGET_PULSE);
    elapsed_ms = HAL_GetTick() - tick_start;

    if (elapsed_ms < SERVO_WAIT_MS)
    {
        HAL_Delay(SERVO_WAIT_MS - elapsed_ms);
    }

    Debug_Print("[SYNC] Step2 done\r\n");
    HAL_Delay(ACTION_GAP_MS);

    /* --------------------------------------------------
     * 第 3 步：STEP2 -> HOME，仅舵机回 HOME
     * -------------------------------------------------- */
    Debug_Print("[SYNC] Step3 begin -> Servo STEP2->HOME\r\n");
    HTD85H_Move(&huart1, SERVO_ID_1, SERVO1_POS_HOME, SERVO_MOVE_TIME_MS);
    HTD85H_Move(&huart1, SERVO_ID_2, SERVO2_POS_HOME, SERVO_MOVE_TIME_MS);
    HAL_Delay(SERVO_WAIT_MS);
    Debug_Print("[SYNC] Step3 done\r\n");

    HAL_Delay(ACTION_GAP_MS);
}

/**
 * @brief  连续执行三次联动动作
 */
static void Run_ServoPushrod_Action_ThreeTimes(void)
{
    uint8_t i;

    for (i = 0U; i < SERVO_REPEAT_TIMES; i++)
    {
        Debug_Printf("[CYCLE] Repeat %d / %d begin\r\n", i + 1, SERVO_REPEAT_TIMES);
        Run_ServoPushrod_Action_Once();
        Debug_Printf("[CYCLE] Repeat %d / %d done\r\n", i + 1, SERVO_REPEAT_TIMES);
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  Debug_Print("\r\n[MAIN] Init done, enter auto cycle\r\n");

  /* 上电/复位后先做一次启动准备 */
  Run_Startup_Prepare();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      Debug_Print("[SYS] Auto cycle begin\r\n");
      Run_ServoPushrod_Action_ThreeTimes();
      Debug_Print("[SYS] Auto cycle done\r\n");

      Debug_Printf("[SYS] Delay %d ms\r\n", SERVO_CYCLE_DELAY_MS);
      HAL_Delay(SERVO_CYCLE_DELAY_MS);

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

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief  GPIO外部中断回调
 * @note   PB14 接推杆编码器白线 A 相
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_14)
    {
        if (g_pushrod_dir == PUSHROD_DIR_EXTEND || g_pushrod_dir == PUSHROD_DIR_RETRACT)
        {
            g_encoder_count++;
        }
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */