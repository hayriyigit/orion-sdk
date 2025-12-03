# Using NetworkVideoConfig.h in Your Application

## Overview

The `NetworkVideoConfig.h` header provides a reusable utility function that ensures `OrionNetworkVideo_t` is properly configured for multicast before encoding. This can be used by **any application** (including Skylink) that needs to configure video streaming.

## Quick Start

### Step 1: Include the Header

```c
#include "NetworkVideoConfig.h"
```

### Step 2: Use Before Encoding

```c
OrionNetworkVideo_t Settings;
OrionPkt_t PktOut;

// Initialize settings
memset(&Settings, 0, sizeof(Settings));
Settings.DestIp = <your_destination_ip>;  // Can be unicast or multicast
Settings.Port = 15004;
Settings.StreamType = STREAM_TYPE_H264;

// Apply multicast-aware configuration (automatically sets TTL)
ConfigureNetworkVideoForMulticast(&Settings);

// Now encode and send
encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
OrionCommSend(&PktOut);
```

## What It Does

The `ConfigureNetworkVideoForMulticast()` function:

1. **Detects multicast IPs** (224.0.0.0 - 239.255.255.255)
2. **Sets TTL = 1** for multicast addresses
3. **Leaves TTL = -1** (default) for unicast addresses

This ensures:
- ✅ Multicast streams use proper TTL=1
- ✅ Unicast streams use gimbal's default TTL=64
- ✅ No manual TTL configuration needed

## Complete Example

### Example 1: Multicast Configuration

```c
#include "OrionPublicPacket.h"
#include "OrionComm.h"
#include "NetworkVideoConfig.h"

void ConfigureMulticastStream(uint32_t MulticastIp, uint16_t Port)
{
    OrionNetworkVideo_t Settings;
    OrionPkt_t PktOut;
    
    // Zero out settings
    memset(&Settings, 0, sizeof(Settings));
    
    // Set destination
    Settings.DestIp = MulticastIp;  // e.g., 239.255.1.1
    Settings.Port = Port;            // e.g., 15004
    Settings.StreamType = STREAM_TYPE_H264;
    
    // Apply multicast-aware configuration
    // This will automatically set Ttl = 1 for multicast
    ConfigureNetworkVideoForMulticast(&Settings);
    
    // Encode and send to gimbal
    encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
    OrionCommSend(&PktOut);
}
```

### Example 2: Unicast Configuration

```c
void ConfigureUnicastStream(uint32_t UnicastIp, uint16_t Port)
{
    OrionNetworkVideo_t Settings;
    OrionPkt_t PktOut;
    
    memset(&Settings, 0, sizeof(Settings));
    Settings.DestIp = UnicastIp;  // e.g., 192.168.1.100
    Settings.Port = Port;
    Settings.StreamType = STREAM_TYPE_H264;
    
    // Apply configuration (will leave TTL=-1 for unicast)
    ConfigureNetworkVideoForMulticast(&Settings);
    
    encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
    OrionCommSend(&PktOut);
}
```

### Example 3: IP Address from String

```c
#include <arpa/inet.h>  // For inet_addr()

void ConfigureStreamFromString(const char *IpString, uint16_t Port)
{
    OrionNetworkVideo_t Settings;
    OrionPkt_t PktOut;
    
    memset(&Settings, 0, sizeof(Settings));
    
    // Convert string IP to network byte order
    Settings.DestIp = inet_addr(IpString);  // Handles both "192.168.1.1" and "239.255.1.1"
    Settings.Port = Port;
    Settings.StreamType = STREAM_TYPE_H264;
    
    // Automatically configures TTL based on IP type
    ConfigureNetworkVideoForMulticast(&Settings);
    
    encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
    OrionCommSend(&PktOut);
}
```

## Integration with Skylink

If Skylink uses this SDK, you can integrate it like this:

### Before (Manual TTL):
```c
OrionNetworkVideo_t Settings;
// ... set DestIp, Port, etc. ...
Settings.Ttl = 1;  // Manual - might be wrong for unicast!
encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
```

### After (Automatic):
```c
#include "NetworkVideoConfig.h"

OrionNetworkVideo_t Settings;
// ... set DestIp, Port, etc. ...
ConfigureNetworkVideoForMulticast(&Settings);  // Automatic TTL!
encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
```

## API Reference

### `IsMulticastIp(uint32_t Ip)`

Checks if an IP address is multicast.

**Parameters**:
- `Ip`: IP address in network byte order

**Returns**:
- `1` if multicast (224.0.0.0 - 239.255.255.255)
- `0` if unicast

**Example**:
```c
uint32_t ip = inet_addr("239.255.1.1");
if (IsMulticastIp(ip)) {
    printf("This is a multicast address\n");
}
```

### `ConfigureNetworkVideoForMulticast(OrionNetworkVideo_t *pSettings)`

Configures `OrionNetworkVideo_t` with proper TTL based on destination IP.

**Parameters**:
- `pSettings`: Pointer to `OrionNetworkVideo_t` structure (must have `DestIp` set)

**Side Effects**:
- Sets `pSettings->Ttl = 1` if multicast
- Leaves `pSettings->Ttl = -1` if unicast (or unchanged if already set)

**Example**:
```c
OrionNetworkVideo_t Settings;
Settings.DestIp = inet_addr("239.255.1.1");
ConfigureNetworkVideoForMulticast(&Settings);
// Settings.Ttl is now 1
```

## Notes

1. **Header-only**: The functions are `static inline`, so they're compiled into your code - no separate library needed
2. **Safe to call multiple times**: Can be called before encoding without side effects
3. **Network byte order**: `DestIp` must be in network byte order (as returned by `inet_addr()` or `uint32FromBeBytes()`)
4. **Backward compatible**: Works with existing code - just add the function call before encoding

## Benefits

✅ **Automatic TTL configuration** - No manual TTL setting needed
✅ **Prevents errors** - Can't accidentally set wrong TTL for multicast/unicast
✅ **Reusable** - Works in any application using this SDK
✅ **Header-only** - Easy to integrate, no linking required
✅ **Type-safe** - Compile-time checked
