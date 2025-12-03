#ifndef NETWORKVIDEOCONFIG_H
#define NETWORKVIDEOCONFIG_H

#include "OrionPublicPacket.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if an IP address is in the multicast range (224.0.0.0 - 239.255.255.255)
 * 
 * @param Ip IP address in network byte order (as stored in OrionNetworkVideo_t.DestIp)
 * @return 1 if multicast, 0 if unicast
 */
static inline int IsMulticastIp(uint32_t Ip)
{
    // Extract the first octet from the IP address (network byte order)
    uint8_t FirstOctet = (Ip >> 24) & 0xFF;
    
    // Check if first octet is in multicast range (224-239)
    return (FirstOctet >= 224 && FirstOctet <= 239);
}

/**
 * @brief Configure OrionNetworkVideo_t structure with proper settings for multicast/unicast
 * 
 * This function automatically detects if the destination IP is multicast and configures
 * the TTL appropriately. It should be called before encodeOrionNetworkVideoPacketStructure().
 * 
 * @param pSettings Pointer to OrionNetworkVideo_t structure to configure
 * 
 * @note This function:
 *   - Sets Ttl = 1 for multicast addresses (224.0.0.0 - 239.255.255.255)
 *   - Leaves Ttl = -1 (default) for unicast (gimbal will use default TTL=64)
 *   - Can be called multiple times safely
 * 
 * @example
 *   OrionNetworkVideo_t Settings;
 *   memset(&Settings, 0, sizeof(Settings));
 *   Settings.DestIp = <your_ip>;
 *   Settings.Port = 15004;
 *   Settings.StreamType = STREAM_TYPE_H264;
 *   
 *   // Apply multicast-aware configuration
 *   ConfigureNetworkVideoForMulticast(&Settings);
 *   
 *   // Now encode and send
 *   encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
 *   OrionCommSend(&PktOut);
 */
static inline void ConfigureNetworkVideoForMulticast(OrionNetworkVideo_t *pSettings)
{
    if (pSettings == NULL)
        return;
    
    // If this is a multicast address, explicitly set TTL to 1 for proper multicast behavior
    // (The gimbal would auto-detect this, but being explicit ensures correct configuration)
    if (IsMulticastIp(pSettings->DestIp))
    {
        pSettings->Ttl = 1;  // Multicast TTL
    }
    // For unicast, we can leave TTL at -1 (default) or explicitly set to 64
    // Leaving at -1 lets the gimbal use its default unicast TTL
    // If you want to be explicit: pSettings->Ttl = 64;
}

#ifdef __cplusplus
}
#endif

#endif // NETWORKVIDEOCONFIG_H
