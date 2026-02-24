#include "stm32f1xx.h"
#include "stm32f1xx_ll_rcc.h"
#include "stm32f1xx_ll_system.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_bus.h"
#include "VITC_usb.h"
volatile uint32_t microsec = 0;
void ITM_Enable(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Разрешить трассировку
    ITM->LAR = 0xC5ACCE55;                          // Разблокировать доступ к ITM
    ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_SYNCENA_Msk; 
    ITM->TER = 1 << 0;                              // Разрешить порт 0
}
int main(void) {
    //настройка тактирования на 72 мегагерца    
    LL_RCC_HSE_Enable();//включаем внешний генератор тактирования
    while(!LL_RCC_HSE_IsReady());//проверяем готов ли он, внеутрений генератор включать нет надобности по тому что он включается по умолчанию
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);//устанавливаем задержу чтения из памяти 2 для частоты 72 мегагерца это необходимо что бы память успевала за процессором
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE_DIV_1, LL_RCC_PLL_MUL_9);// устанавливаем источник PLL на HSE без делителя и умноэитель на 9 получаем 72 мгц
    LL_RCC_PLL_Enable();// включаем PLL
    while(!LL_RCC_PLL_IsReady());// ждем его запуска
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);//настройка делителя AHB
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);// устанавливем делитель на шину APB1 по скольку максимальная частота тактировани 36 
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);//настройка делителя APB2
    LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_PLL_DIV_1_5);// устанавливаме источник сигнала USB PLL с делителем 1.5 для частоты 48 мгц
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);//устанавливаем источник системого тактирования PLL
    while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);// ждем установки системного тактирования 
    SystemCoreClockUpdate();// обновление системного тактирования

    ITM_Enable();//включаем отладочную консоль SWV
    USB_DeviceConfig *config;
    InitUSB(config);
    while(1) {

    }
}