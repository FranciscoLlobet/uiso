/*
 * network.h
 *
 *  Created on: 10 abr 2023
 *      Author: Francisco
 */

#ifndef NETWORK_H_
#define NETWORK_H_

#include "uiso.h"


#include "mbedtls/ssl.h"
/* Include Simplelink */
#include "simplelink.h"
#include "netapp.h"
#include "socket.h"

/* Include mbedTLS as TLS provider */
#include "mbedtls/ssl.h"


#define UISO_PROTOCOL_BIT		             (0)
#define UISO_PROTOCOL_BIT_MASK			     (1 << UISO_PROTOCOL_BIT)
#define UISO_TCP_UDP_SELECTION_BIT	         (1)
#define UISO_TCP_UDP_SELECTION_BIT_MASK      (1 << UISO_TCP_UDP_SELECTION_BIT)
#define UISO_IPV4_IPV6_SELECTION_BIT         (2)
#define UISO_IPV4_IPV6_SELECTION_BIT_MASK    (1 << UISO_IPV4_IPV6_SELECTION_BIT)
#define UISO_SECURITY_BIT				     (3)
#define UISO_SECURITY_BIT_MASK			     (1 << UISO_SECURITY_BIT)

#define UISO_PROTOCOL_MASK    (UISO_PROTOCOL_BIT_MASK | UISO_TCP_UDP_SELECTION_BIT_MASK | UISO_IPV4_IPV6_SELECTION_BIT_MASK | UISO_SECURITY_BIT_MASK)

enum uiso_protocol
{
	uiso_protocol_no_protocol = 0,

	uiso_protocol_udp_ip4 = (UISO_PROTOCOL_BIT_MASK |(0 << UISO_TCP_UDP_SELECTION_BIT)|(0 << UISO_IPV4_IPV6_SELECTION_BIT)|(0 << UISO_SECURITY_BIT)) & UISO_PROTOCOL_MASK,
	uiso_protocol_tcp_ip4 = (UISO_PROTOCOL_BIT_MASK |(1 << UISO_TCP_UDP_SELECTION_BIT)|(0 << UISO_IPV4_IPV6_SELECTION_BIT)|(0 << UISO_SECURITY_BIT)) & UISO_PROTOCOL_MASK,

	/* IPv6 - NOT SUPPORTED */
	uiso_protocol_udp_ip6 = (UISO_PROTOCOL_BIT_MASK |(0 << UISO_TCP_UDP_SELECTION_BIT)|(1 << UISO_IPV4_IPV6_SELECTION_BIT)|(0 << UISO_SECURITY_BIT)) & UISO_PROTOCOL_MASK,
	uiso_protocol_tcp_ip6 = (UISO_PROTOCOL_BIT_MASK |(1 << UISO_TCP_UDP_SELECTION_BIT)|(1 << UISO_IPV4_IPV6_SELECTION_BIT)|(0 << UISO_SECURITY_BIT)) & UISO_PROTOCOL_MASK,

	/* IPv4 TLS */
	uiso_protocol_dtls_ip4 = (uiso_protocol_udp_ip4 | (1 << UISO_SECURITY_BIT)) & UISO_PROTOCOL_MASK,
	uiso_protocol_tls_ip4 =  (uiso_protocol_tcp_ip4 | (1 << UISO_SECURITY_BIT)) & UISO_PROTOCOL_MASK,

	/* IPv6 TLS - NOT SUPPORTED */
	uiso_protocol_dtls_ip6 = (uiso_protocol_udp_ip6 | (1 << UISO_SECURITY_BIT)) & UISO_PROTOCOL_MASK,
	uiso_protocol_tls_ip6 =  (uiso_protocol_tcp_ip6 | (1 << UISO_SECURITY_BIT)) & UISO_PROTOCOL_MASK,

	uiso_protocol_max = UISO_PROTOCOL_MASK,
};

enum wifi_socket_id_e
{
	wifi_service_ntp_socket = 0,
	wifi_service_lwm2m_socket = 1,
	wifi_service_mqtt_socket = 2,
	wifi_service_http_socket = 3,
	wifi_service_max
};


#endif /* NETWORK_H_ */
