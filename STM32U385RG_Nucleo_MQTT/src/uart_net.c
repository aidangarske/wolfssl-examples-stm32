/* uart_net.c
 *
 * Copyright (C) 2006-2026 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/*
 * USART1 transport layer for wolfMQTT on STM32U385RG.
 *
 * Implements the MqttNet callbacks (connect, read, write, disconnect)
 * using an IRQ-driven ring buffer on USART1.
 */

#include "stm32u3xx_hal.h"
#include <string.h>
#include <stdio.h>

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfmqtt/mqtt_client.h"
#include "uart_net.h"

/* ------------------------------------------------------------------ */
/* Ring buffer for USART1 RX                                          */
/* ------------------------------------------------------------------ */
#define UART_RX_BUF_SIZE 4096

static volatile uint8_t rx_buf[UART_RX_BUF_SIZE];
static volatile uint32_t rx_head;  /* written by IRQ */
static volatile uint32_t rx_tail;  /* read by app */

UART_HandleTypeDef huart1;
static volatile uint8_t rx_byte;   /* single-byte IRQ target */

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* Called from HAL_UART_IRQHandler when one byte arrives */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uint32_t next;
    if (huart->Instance == USART1) {
        next = (rx_head + 1) % UART_RX_BUF_SIZE;
        if (next != rx_tail) {
            rx_buf[rx_head] = rx_byte;
            rx_head = next;
        }
        /* re-arm single-byte interrupt RX */
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1);
    }
}

static uint32_t rx_available(void)
{
    uint32_t h = rx_head;
    uint32_t t = rx_tail;
    return (h >= t) ? (h - t) : (UART_RX_BUF_SIZE - t + h);
}

static int rx_read(uint8_t *dst, int len, int timeout_ms)
{
    int read_total = 0;
    uint32_t start;

    start = HAL_GetTick();
    while (read_total < len) {
        if (rx_available() > 0) {
            dst[read_total++] = rx_buf[rx_tail];
            rx_tail = (rx_tail + 1) % UART_RX_BUF_SIZE;
        }
        else {
            if ((int)(HAL_GetTick() - start) >= timeout_ms) {
                break;
            }
        }
    }
    return read_total;
}

/* ------------------------------------------------------------------ */
/* USART1 init — call from hw_init.c after clocks are up              */
/* ------------------------------------------------------------------ */
void UartNet_HW_Init(void)
{
    GPIO_InitTypeDef gpio;

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* USART1: PA9 TX, PA10 RX (NUCLEO-U385RG-Q default) */
    gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* Zero the whole handle first so unset Init/AdvancedInit fields
     * (FIFO, ClockPrescaler, OneBitSampling, AdvFeatureInit, ...) start
     * at their default values rather than indeterminate stack data. */
    XMEMSET(&huart1, 0, sizeof(huart1));
    huart1.Instance               = USART1;
    huart1.Init.BaudRate          = 115200;
    huart1.Init.WordLength        = UART_WORDLENGTH_8B;
    huart1.Init.StopBits          = UART_STOPBITS_1;
    huart1.Init.Parity            = UART_PARITY_NONE;
    huart1.Init.Mode              = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl         = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling      = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling    = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler    = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        while (1) { __NOP(); }
    }

    /* Enable USART1 IRQ and arm the first single-byte RX */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1);
}

/* ------------------------------------------------------------------ */
/* MqttNet callbacks                                                  */
/* ------------------------------------------------------------------ */
static int UartNet_Connect(void *context, const char *host,
                           word16 port, int timeout_ms)
{
    (void)context; (void)host; (void)port; (void)timeout_ms;
    /* UART is always "connected" — no-op */
    return MQTT_CODE_SUCCESS;
}

static int UartNet_Read(void *context, byte *buf, int buf_len,
                        int timeout_ms)
{
    int n;
    (void)context;

    if (buf == NULL || buf_len <= 0) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    n = rx_read(buf, buf_len, timeout_ms);
    if (n == 0) {
        return MQTT_CODE_ERROR_TIMEOUT;
    }
    return n;
}

static int UartNet_Write(void *context, const byte *buf, int buf_len,
                         int timeout_ms)
{
    HAL_StatusTypeDef st;
    (void)context;

    if (buf == NULL || buf_len <= 0) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    st = HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)buf_len,
                           (uint32_t)timeout_ms);
    if (st == HAL_TIMEOUT) {
        return MQTT_CODE_ERROR_TIMEOUT;
    }
    if (st != HAL_OK) {
        return MQTT_CODE_ERROR_NETWORK;
    }
    return buf_len;
}

static int UartNet_Disconnect(void *context)
{
    (void)context;
    /* nothing to do — UART stays open */
    return MQTT_CODE_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Public init / deinit                                               */
/* ------------------------------------------------------------------ */
int UartNet_Init(MqttNet *net)
{
    if (net == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    memset(net, 0, sizeof(*net));
    net->connect    = UartNet_Connect;
    net->read       = UartNet_Read;
    net->write      = UartNet_Write;
    net->disconnect = UartNet_Disconnect;
    net->context    = NULL; /* single UART, no per-connection state */
    return MQTT_CODE_SUCCESS;
}

int UartNet_DeInit(MqttNet *net)
{
    if (net != NULL) {
        memset(net, 0, sizeof(*net));
    }
    return MQTT_CODE_SUCCESS;
}
