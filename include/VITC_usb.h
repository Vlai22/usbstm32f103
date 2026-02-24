#ifndef VITC_USB_H
#define VITC_USB_H
typedef struct Endpoint {
    uint8_t  num;
    uint16_t size;
    uint16_t pmaaddr_tx;
    uint16_t pmaaddr_rx;
    uint32_t type;
    void (*out_callback)(struct Endpoint *self, uint16_t len); // Пришли данные
    void (*in_callback)(struct Endpoint *self);              // Данные ушли
} Endpoint;
typedef struct {
    uint8_t *device_descriptor;
    uint8_t *config_descriptor;   
} USB_DeviceConfig;
void VITC_USB_ReadPMA(Endpoint *ep, uint8_t *user_buf);
void VITC_USB_WritePMA(Endpoint *ep, uint8_t *user_buf, uint16_t n_bytes);
void ActivateEP(Endpoint *ep);
void InitUSB(USB_DeviceConfig *configmy);
#endif