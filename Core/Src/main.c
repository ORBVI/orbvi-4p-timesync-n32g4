#include "n32g4fr.h"
#include "n32g4fr_gpio.h"
#include "n32g4fr_rcc.h"
#include "n32g4fr_tim.h"
#include "n32g4fr_usart.h"
#include "misc.h"
#include "time_protocol.h"

/* TIM3 clock: APB1=36 MHz, APB1 prescaler != 1 -> TIM3=72 MHz.
 * 72 MHz / 1440 / 50000 = 1 Hz; 500 ticks = 10 ms high. */
#define TIM_PRESCALER 1439u
#define TIM_PERIOD    49999u
#define PPS_HIGH_TICKS 500u
#define RX_SIZE 64u /* Power of two; one slot remains unused. */

/* Bench-only GPS simulation: first PPS after reset is 2025-01-01 00:00:00 UTC.
 * This date is intentionally synthetic, NOT current wall-clock time. */
static const UtcTime demo_boot_utc = {2025u, 1u, 1u, 0u, 0u, 0u};

/* Observe via J-Link while diagnosing USART2 wiring; never printed on time UART. */
volatile uint32_t timesync_rx_bytes, timesync_rmc_sent;
static volatile uint8_t rx[RX_SIZE];
static volatile uint8_t rx_head, rx_tail, rx_overflow;
static volatile uint32_t edge_sequence;
static volatile uint8_t active, pending, edge_valid;
static volatile UtcTime pending_utc, current_utc, edge_utc;

static void uart_init(void)
{
    GPIO_InitType gpio;
    USART_InitType usart;
    NVIC_InitType nvic;
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_USART2, ENABLE);
    GPIO_InitStruct(&gpio);
    gpio.Pin = GPIO_PIN_2;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitPeripheral(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_3;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitPeripheral(GPIOA, &gpio);
    USART_StructInit(&usart);
    usart.BaudRate = 115200;
    usart.WordLength = USART_WL_8B;
    usart.StopBits = USART_STPB_1;
    usart.Parity = USART_PE_NO;
    usart.Mode = USART_MODE_TX | USART_MODE_RX;
    usart.HardwareFlowControl = USART_HFCTRL_NONE;
    USART_Init(USART2, &usart);
    USART_ConfigInt(USART2, USART_INT_RXDNE, ENABLE);
    USART_Enable(USART2, ENABLE);
    nvic.NVIC_IRQChannel = USART2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

static void pps_init(void)
{
    GPIO_InitType gpio;
    TIM_TimeBaseInitType base;
    OCInitType oc;
    NVIC_InitType nvic;
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_AFIO, ENABLE);
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM3, ENABLE);
    GPIO_InitStruct(&gpio);
    gpio.Pin = GPIO_PIN_1; /* PB1, TIM3_CH4, default (unremapped). */
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitPeripheral(GPIOB, &gpio);
    GPIO_ResetBits(GPIOB, GPIO_PIN_1);
    TIM_InitTimBaseStruct(&base);
    base.Prescaler = TIM_PRESCALER;
    base.Period = TIM_PERIOD;
    base.CntMode = TIM_CNT_MODE_UP;
    base.ClkDiv = TIM_CLK_DIV1;
    TIM_InitTimeBase(TIM3, &base);
    TIM_InitOcStruct(&oc);
    oc.OcMode = TIM_OCMODE_PWM1;
    oc.OutputState = TIM_OUTPUT_STATE_ENABLE;
    oc.Pulse = PPS_HIGH_TICKS;
    oc.OcPolarity = TIM_OC_POLARITY_HIGH;
    TIM_InitOc4(TIM3, &oc);
    TIM_ConfigOc4Preload(TIM3, TIM_OC_PRE_LOAD_ENABLE);
    TIM_ConfigArPreload(TIM3, ENABLE);
    /* Load PSC/ARR while interrupt disabled; start outside the PWM high window.
     * First physical rising edge is the first overflow, paired with the IRQ. */
    TIM_GenerateEvent(TIM3, TIM_EVT_SRC_UPDATE);
    TIM_SetCnt(TIM3, PPS_HIGH_TICKS);
    TIM_ClrIntPendingBit(TIM3, TIM_INT_UPDATE);
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitPeripheral(GPIOB, &gpio);
    nvic.NVIC_IRQChannel = TIM3_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    TIM_ConfigInt(TIM3, TIM_INT_UPDATE, ENABLE);
    TIM_Enable(TIM3, ENABLE);
}

void TIM3_IRQHandler(void)
{
    if (TIM_GetIntStatus(TIM3, TIM_INT_UPDATE) != RESET) {
        TIM_ClrIntPendingBit(TIM3, TIM_INT_UPDATE);
        if (pending) {
            current_utc = pending_utc;
            pending = 0;
            active = 1;
        } else if (active) {
            UtcTime t = current_utc;
            utc_next_second(&t);
            current_utc = t;
        }
        if (active) {
            edge_utc = current_utc;
            edge_valid = 1;
        } else {
            edge_valid = 0;
        }
        ++edge_sequence;
    }
}

void USART2_IRQHandler(void)
{
    if (USART_GetIntStatus(USART2, USART_INT_RXDNE) != RESET ||
        USART_GetFlagStatus(USART2, USART_FLAG_OREF) != RESET) {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART2);
        ++timesync_rx_bytes;
        uint8_t next = (uint8_t)((rx_head + 1u) & (RX_SIZE - 1u));
        if (next == rx_tail) rx_overflow = 1;
        else { rx[rx_head] = byte; rx_head = next; }
    }
}

static int rx_pop(void)
{
    if (rx_tail == rx_head) return -1;
    uint8_t c = rx[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1u) & (RX_SIZE - 1u));
    return c;
}

static void process_rx(void)
{
    static char line[20];
    static unsigned length;
    static uint8_t discard;
    if (rx_overflow) {
        uint32_t irq = __get_PRIMASK();
        __disable_irq();
        rx_tail = rx_head;
        rx_overflow = 0;
        if (!irq) __enable_irq();
        length = 0; discard = 1;
    }
    int c;
    while ((c = rx_pop()) >= 0) {
        if (c == '\n') {
            UtcTime t;
            if (!discard && utc_parse_command(line, length, &t)) {
                uint32_t irq = __get_PRIMASK();
                __disable_irq();
                pending_utc = t;
                pending = 1;
                if (!irq) __enable_irq();
            }
            length = 0; discard = 0;
        } else if (c == '\r') {
            /* Optional CR before LF. */
        } else if (!discard && length < sizeof(line)) {
            line[length++] = (char)c;
        } else {
            discard = 1;
        }
    }
}

static void send_rmc(const UtcTime *t)
{
    char sentence[41];
    size_t len = utc_format_rmc(t, sentence, sizeof sentence);
    for (size_t i = 0; i < len; ++i) {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXDE) == RESET) {}
        USART_SendData(USART2, (uint16_t)(uint8_t)sentence[i]);
    }
    if (len) ++timesync_rmc_sent;
}

int main(void)
{
    RCC_ClocksType clocks;
    SystemCoreClockUpdate();
    RCC_GetClocksFreqValue(&clocks);
    /* Never generate a misleading 1 Hz PPS when HSE/PLL failed. */
    if (clocks.SysclkFreq != 144000000u || clocks.Pclk1Freq != 36000000u) {
        for (;;) { __WFI(); }
    }
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    uart_init();
    /* Set the time BEFORE the hardware timer starts: first rising PPS is the
     * defined demo epoch even with no input from orbvi-4p. */
    pending_utc = demo_boot_utc;
    pending = 1;
    pps_init();
    uint32_t last_edge = 0;
    for (;;) {
        process_rx();
        UtcTime t;
        uint8_t valid;
        uint32_t seq, irq = __get_PRIMASK();
        __disable_irq();
        seq = edge_sequence;
        valid = edge_valid;
        if (valid) t = edge_utc;
        if (!irq) __enable_irq();
        if (seq != last_edge) {
            last_edge = seq;
            if (valid) send_rmc(&t);
        }
    }
}
