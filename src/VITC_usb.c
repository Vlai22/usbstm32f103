#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "VITC_usb.h"
#include "stm32f1xx.h"
#include "stm32f1xx_ll_usb.h"
#include "stm32f1xx_ll_bus.h"
static USB_DeviceConfig *config = NULL;
Endpoint ep0;
void SWD_PrintChar(char c) {
    // Проверяем, разрешен ли порт и готов ли буфер (регистр ITM->TER и ITM->PORT[0])
    if (((ITM->TCR & ITM_TCR_ITMENA_Msk) != 0UL) && ((ITM->TER & 1UL) != 0UL)) {
        while (ITM->PORT[0].u32 == 0UL);
        ITM->PORT[0].u8 = (uint8_t)c;
    }
}

// Для вывода строк
void SWD_PrintString(char* str) {
    while(*str) SWD_PrintChar(*str++);
}
void Set_EP_Register(uint8_t ep_num, uint32_t target_stat_and_type) {
    // Получаем указатель на регистр конкретной точки
    volatile uint32_t *ep_reg = (uint32_t *)((uint32_t)USB_BASE + (ep_num * 4));
    // Биты CTR_RX и CTR_TX сбрасываются записью 0. 
    // Чтобы их не сбросить случайно, мы читаем их и записываем обратно.
    uint32_t fixed_bits = (*ep_reg & (USB_EP_CTR_RX | USB_EP_CTR_TX)) | ep_num;
    uint32_t current = *ep_reg;
    // Магия Toggle: биты STAT_RX/TX инвертируются, если записать 1.
    // Формула: (Текущее ^ Целевое) & Маска_Статуса
    *ep_reg = ((current ^ target_stat_and_type) & (USB_EPRX_STAT | USB_EPTX_STAT))  | (target_stat_and_type & USB_EP_T_FIELD) | fixed_bits;
}

void ActivateEP(Endpoint *ep){
    uint16_t *btable = (uint16_t *)USB_PMAADDR;
    uint32_t offset = ep->num * 4;
    // настройка TX
    btable[offset + 0] = ep->pmaaddr_tx;
    btable[offset + 1] = 0;
    //настройка RX
    btable[offset + 2] = ep->pmaaddr_rx;
    if (ep->size > 62) {
        btable[offset + 3] = (uint16_t)(0x8000 | (((ep->size / 32) - 1) << 10));
    } else {
        btable[offset + 3] = (uint16_t)((ep->size / 2) << 10);
    }
    volatile uint32_t *ep_reg = (uint32_t *)((uint32_t)USB_BASE + (ep->num * 4));
    uint32_t fixed_bits = USB_EP_CTR_RX | USB_EP_CTR_TX | (ep->num & USB_EPADDR_FIELD);
    // Выбираем статус в зависимости от типа
    // Если это не EP0, то TX статус обычно NAK (ждем данных от прогера)
    uint32_t target_status = USB_EP_RX_VALID | USB_EP_TX_NAK;
    if (ep->num == 0) target_status = USB_EP_RX_VALID | USB_EP_TX_STALL;
    uint32_t current = *ep_reg;
    // записываем значения не изменяя определённые биты в частности фиксет и биты обозначающие тип конечной точки
    *ep_reg = ((current ^ (target_status | ep->type)) & (USB_EPRX_STAT | USB_EPTX_STAT)) | ep->type | fixed_bits;
}
void VITC_USB_ReadPMA(uint16_t pma_offset, uint8_t *user_buf, uint16_t n_bytes) {
    // Вычисляем начальный адрес в PMA. 
    // USB_PMAADDR — это 0x40006000. Умножаем смещение на 2, 
    // так как каждое 16-битное слово занимает 32 бита в адресном пространстве.
    uint32_t *pma_ptr = (uint32_t *)(USB_PMAADDR + (pma_offset * 2));
    uint16_t *dest = (uint16_t *)user_buf;
    
    // Копируем по 2 байта за итерацию
    for (uint16_t i = (n_bytes + 1) >> 1; i > 0; i--) {
        *dest++ = (uint16_t)*pma_ptr++; 
        // pma_ptr++ здесь сдвигает адрес на 4 байта (32 бита), 
        // перепрыгивая через "мусорные" 16 бит.
    }
}
void VITC_USB_WritePMA(Endpoint *ep, uint8_t *user_buf, uint16_t n_bytes) {
    uint32_t *pma_ptr = (uint32_t *)(USB_PMAADDR + (ep->pmaaddr_tx * 2));
    uint16_t *src = (uint16_t *)user_buf;

    for (uint16_t i = (n_bytes + 1) >> 1; i > 0; i--) {
        *pma_ptr++ = (uint32_t)*src++;
    }
    uint16_t *btable = (uint16_t *)USB_PMAADDR;
    btable[ep->num * 4 + 1] = n_bytes;
    Set_EP_Register(ep->num, ep->type | USB_EP_TX_VALID); 
}

void USB_LP_CAN1_RX0_IRQHandler(void){
    uint32_t istr = USB->ISTR;
    uint8_t ep_num = (uint8_t)(istr & USB_ISTR_EP_ID);
    if(istr & USB_ISTR_RESET){
        USB->ISTR = (uint16_t)(~USB_ISTR_RESET);
        USB->DADDR = USB_DADDR_EF | 0;
        ep0.num = 0;
        ep0.pmaaddr_rx = 0x40;
        ep0.pmaaddr_tx = 0x80;
        ep0.size = 64;
        ep0.type = USB_EP_CONTROL;
        ActivateEP(&ep0);
        return;
    }
    if(istr & USB_ISTR_CTR){
        if(ep_num == 0){
            // 1. Читаем регистр конкретной точки
        uint16_t ep0r = *(volatile uint32_t *)(USB_BASE);

        // 2. ПРОВЕРЯЕМ БИТ SETUP
        if (ep0r & USB_EP_SETUP) { 
            USB_SetupPacket req;
            VITC_USB_ReadPMA(ep0.pmaaddr_rx, (uint8_t *)&req, 8);
            char buf[64];
            // Выводим сырые байты для анализа
            SWD_PrintString("\nRAW SETUP: ");
            for(int i=0; i<8; i++) {
                sprintf(buf, "%02X ", ((uint8_t*)&req)[i]);
                SWD_PrintString(buf);
            }

            // Выводим интерпретацию
            sprintf(buf, "\nReq: %02X, Val: %04X, Idx: %04X, Len: %04X\n", 
                    req.bRequest, req.wValue, req.wIndex, req.wLength);
            SWD_PrintString(buf);
                // 1. Запрос дескриптора (GET_DESCRIPTOR = 0x06)
            if (req.bRequest == 0x06) {
                uint8_t desc_type = (req.wValue >> 8); // Старший байт wValue - это тип
            
                if (desc_type == 0x01) { // Device Descriptor
                    // Отправляем 18 байт нашего "паспорта"
                    uint16_t len = (req.wLength < 18) ? req.wLength : 18;
                    VITC_USB_WritePMA(&ep0, config->device_descriptor, len);
                    Set_EP_Register(0, USB_EP_CONTROL | USB_EP_TX_VALID);
                }
                else if (desc_type == 0x02) { // Configuration Descriptor
                    // Отправляем дерево конфигурации
                    // ВАЖНО: тут нужно отправить ВСЁ дерево (Config + Interface + HID + EP)
                    // Его длина указана в самом дескрипторе (байты 2-3)
                    uint16_t total_len = config->config_descriptor[2] | (config->config_descriptor[3] << 8); 
                    uint16_t len = (req.wLength < total_len) ? req.wLength : total_len;

                    VITC_USB_WritePMA(&ep0, config->config_descriptor, len);
                    Set_EP_Register(0, USB_EP_CONTROL | USB_EP_TX_VALID);
                }else if (desc_type == 0x22) { // GET_REPORT_DESCRIPTOR
                        // Берем длину из новой переменной в структуре
                        uint16_t len = (req.wLength < config->report_len) ? req.wLength : config->report_len;
    
                        // ПЕРЕДАЕМ ИМЕННО REPORT, А НЕ DEVICE!
                        VITC_USB_WritePMA(&ep0, config->report_descriptor, len);
                        Set_EP_Register(0, USB_EP_CONTROL | USB_EP_TX_VALID);
                }
            }
            // 2. Установка адреса (SET_ADDRESS = 0x05)
            else if (req.bRequest == 0x05) {
                // Мы НЕ ставим адрес сразу! Сначала нужно подтвердить получение запроса.
                // Запоминаем адрес (он в req->wValue)
                uint8_t new_addr = (uint8_t)(req.wValue);

                // Отправляем пустой пакет (ZLP - Zero Length Packet) в ответ
                VITC_USB_WritePMA(&ep0, NULL, 0); 
                Set_EP_Register(0, USB_EP_CONTROL | USB_EP_TX_VALID);

                // Адрес применится в прерывании после успешной отправки этого ZLP
                // Или можно "рискнуть" и поставить сразу:
                USB->DADDR = USB_DADDR_EF | new_addr;
            }
            
            // После обработки SETUP ОБЯЗАТЕЛЬНО сбрасываем флаг CTR_RX
            // Используй свою функцию или прямую запись (сброс бита 15)
            Set_EP_Register(0, USB_EP_CONTROL | USB_EP_RX_VALID); 
        } 
        else if (ep0r & USB_EP_CTR_TX) {
            // Это прерывание говорит о том, что твои данные (IN) успешно ушли
            // Здесь обычно ничего делать не надо для EP0, просто сбросить флаг
            Set_EP_Register(0, USB_EP_CONTROL | USB_EP_RX_VALID);
        }
        else if (ep0r & USB_EP_CTR_RX) {
            // Это прерывание о получении обычных данных (OUT)
            Set_EP_Register(0, USB_EP_CONTROL | USB_EP_RX_VALID);
        }
    }
    }
}
void InitUSB(USB_DeviceConfig *configmy){
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USB);// включаем тактирование usb
    USB->CNTR = USB_CNTR_FRES;//принудительно держим в режиме сброса
    for(volatile int i=0;i<1000;i++);//ждем стабилизации usb аналоговой части 
    USB->CNTR = 0;// сбрасываем usb полность
    USB->ISTR = 0; // сбрасываем регистр прерваний 
    USB->BTABLE = 0;// сбрасываем адрес нахождения буфера PMA
    USB->CNTR = USB_CNTR_RESETM | USB_CNTR_CTRM; // выставляем биты прерываения по ресету и успешной транзакции
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    __enable_irq();
    config = configmy;
    ITM->PORT[0].u8 = '!'; // Отправить символ сразу при входе
}