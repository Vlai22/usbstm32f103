#ifndef VITC_USB_H
#define VITC_USB_H
typedef struct Endpoint {
    uint8_t  num;
    uint16_t size;
    uint16_t pmaaddr_tx;
    uint16_t pmaaddr_rx;
    uint32_t type;       // USB_EP_CONTROL, USB_EP_BULK и т.д.
    
    // Callback-функции для пользователя
    void (*out_callback)(struct Endpoint *self, uint16_t len); // Пришли данные
    void (*in_callback)(struct Endpoint *self);              // Данные ушли
} Endpoint;
typedef struct {
    uint8_t *device_descriptor;      // Указатель на 18 байт паспорта
    uint8_t *config_descriptor;      // Указатель на дерево конфигурации
    uint8_t *report_descriptor;  // <--- ДОБАВИТЬ ЭТО
    uint16_t report_len;         // Длина (для мышки обычно 52)
    uint8_t *string_descriptors[];   // Строки (имя, серийник)
} USB_DeviceConfig;
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SetupPacket;
void ActivateEP(Endpoint *ep);
void InitUSB();
#endif