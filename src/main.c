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


__attribute__((aligned(4))) const uint8_t MyConfigDescriptor[] = {
    // --- Configuration Descriptor ---
    9,          // bLength
    0x02,       // bDescriptorType (Configuration)
    34, 0x00,   // wTotalLength (34 байта всего: Config + Interface + HID + EP)
    1,          // bNumInterfaces
    1,          // bConfigurationValue
    0,          // iConfiguration
    0xA0,       // bmAttributes (Bus powered + Remote Wakeup)
    0x32,       // bMaxPower (100 mA)

    // --- Interface Descriptor ---
    9, 0x04, 0, 0, 1, 0x03, 0x01, 0x02, 0, // Class 3 (HID), Subclass 1 (Boot), Protocol 2 (Mouse)

    // --- HID Descriptor ---
    9, 0x21, 0x11, 0x01, 0x00, 1, 0x22, 52, 0x00, // Ссылка на Report Descriptor (52 байта)

    // --- Endpoint Descriptor (EP1 IN) ---
    7, 0x05, 0x81, 0x03, 4, 0x00, 10 // EP1, Interrupt IN, 4 байта, 10 мс
};
__attribute__((aligned(4))) const uint8_t MyMouseReportDescriptor[] = {
        0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xA1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xA1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x09, 0x38,                    //     USAGE (Wheel)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7F,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xC0,                          //   END_COLLECTION
    0xC0                           // END_COLLECTION
}; 

RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; // Тактирование порта C
GPIOC->CRH &= ~GPIO_CRH_MODE13;     // Сброс настроек 13 пина
GPIOC->CRH |= GPIO_CRH_MODE13_1;    // Output 2MHz
GPIOC->CRH &= ~GPIO_CRH_CNF13;      // Push-pull
GPIOC->BSRR = GPIO_BSRR_BS13;       // Выключить (1)
ITM_Enable();
    USB_DeviceConfig config;
    config.device_descriptor = (uint8_t *)MyConfigDescriptor;
    config.config_descriptor = (uint8_t *)MyConfigDescriptor;
    config.report_descriptor = (uint8_t *)MyMouseReportDescriptor; // <--- СЮДА
    config.report_len = sizeof(MyMouseReportDescriptor);
    InitUSB(config);
    while(1) {

    }
}