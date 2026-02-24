#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "VITC_usb.h"
#include "stm32f1xx.h"
#include "stm32f1xx_ll_usb.h"
#include "stm32f1xx_ll_bus.h"
static USB_DeviceConfig *config = NULL;
Endpoint ep0;
void ActivateEP(Endpoint *ep){
    uint32_t *btable = USB_PMAADDR;// получаем значения таблицы что бы задать в нужные в завимисости от точки данные смещения информации
    uint32_t *ep_reg = (uint32_t *)USB_BASE + ep->num;//получаем адрес в котором находяться настройки конечной точки по номеру точик
    uint32_t now = *ep_reg;
    now = (now ^ 0x3030)& 0x3030;
    uint32_t currect = (uint32_t) (ep->type << 9);//устанавливаем биты типа 
    if(ep->type == USB_EP_BULK){//устанавливаем бит king для соответсвущих конечнх точек
        currect = currect | (1 << 8);
    }
    currect = currect | (ep->num << 0);//устанавливаем номер конечной точки
    currect = currect | now;
    currect = currect | USB_EPREG_MASK;// защита от стирания битов статуса
    *ep_reg = currect;
    btable[ep->num * 4 + 0] = ep->pmaaddr_tx;
    btable[ep->num * 4 + 1] = 0;
    btable[ep->num * 4 + 2] = ep->pmaaddr_rx;
    if(ep->size <= 62){ 
        btable[ep->num * 4 + 3] = (uint16_t)(ep->size/2) << 10;
    }else{
        btable[ep->num * 4 + 3] = (uint16_t)(1 << 15) | ((ep->size/32) - 1) << 10;
    }
}
void VITC_USB_ReadPMA(Endpoint *ep, uint8_t *user_buf) {
    uint32_t *pma_data_ptr = (uint32_t *)USB_PMAADDR + ep->pmaaddr_rx;// получаем адрес нахождения данных
    uint32_t len = btable[ep->num * 4 + 3] & 0x03FF;//чтение количества байт принятых
    for(int i=0;i<len;i=i+2){
        user_buf[i] =(uint8_t)(pma_data_ptr[i/2]) & 0xFF;//читаем первый байт
        if(i + 1 < len ){
            user_buf[i+1] =(uint8_t)(pma_data_ptr[i/2] >> 8) & 0xFF;//если есть последний байт то читаем
        }
    }
    uint32_t *ep_reg = (uint32_t *)USB_BASE + ep->num;// регистр текущей конечной точки
    uint32_t current = *ep_reg;//записываем текущее значение регистра
    uint32_t to_write = (current ^ 0x3000) & (0x3000 | 0x070F);//получаем текущее значение и инвертируем биты для записи VALID в регистр только необходимые
    to_write |= USB_EPREG_MASK; // Защищаем прерывания
    *ep_reg = to_write;//записываем в регистр нужные значения 
}
void VITC_USB_WritePMA(Endpoint *ep, uint8_t *user_buf, uint16_t n_bytes) {
    uint32_t *pma_data_ptr = (uint32_t *)USB_PMAADDR + ep->pmaaddr_tx;// получаем адрес куда нужно положить данные
    for(int i=0;i<n_bytes;i=i+2){//запись данные в регистры
        uint16_t data = (uint16_t) user_buf[i];// переменная для временного хранения
        if(i + 1 < n_bytes ){// если можем записать ещё однео значение то пишем 
            data = (user_buf[i+1] << 8) | user_buf[i];
        }
        pma_data_ptr[i/2] = data;// записываем данные в регистр
    }
    uint32_t *btable = (uint32_t *)USB_PMAADDR + ep->num * 4;
    btable[1] = n_bytes;// записываем количество данных в регистр
    uint32_t *ep_reg = (uint32_t *)USB_BASE + ep->num;// регистр текущей конечной точки
    uint32_t current = *ep_reg;//записываем текущее значение регистра
    uint32_t to_write = (current ^ 0x0030) & (0x0030 | 0x070F);//получаем текущее значение и инвертируем биты для записи VALID в регистр только необходимые
    to_write |= USB_EPREG_MASK; // Защищаем прерывания
    *ep_reg = to_write;//записываем в регистр нужные значения 
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
}