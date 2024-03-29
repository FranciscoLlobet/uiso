#ifndef UISO_CONFIG_H_
#define UISO_CONFIG_H_

#define JSMN_PARENT_LINKS 1

#include "uiso.h"
#include "ff.h"

void uiso_load_config(void);

char* config_get_wifi_ssid(void);
char* config_get_wifi_key(void);
char* config_get_lwm2m_uri(void);
char* config_get_lwm2m_psk_id(void);
char* config_get_lwm2m_psk_key(void);

#endif /* UISO_CONFIG_H_ */
