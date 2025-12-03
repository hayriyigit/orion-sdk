# SDK Core Multicast Handling - Complete Analysis (Non-Examples)

## Overview

This document analyzes how the **core SDK** (excluding Examples) handles multicast video streaming configuration. The SDK itself **does not stream video** - it **configures the gimbal** to stream video via UDP.

## Key Finding: SDK Architecture

The SDK is a **configuration/control layer**, not a video streaming layer:

```
SDK Layer (This Repository)
    ↓
Sends OrionNetworkVideo packet via TCP
    ↓
Gimbal Firmware (Not in this repo)
    ↓
Gimbal streams UDP video packets
```

## Core SDK Components

### 1. Protocol Definition

**File**: `Communications/OrionPublicProtocol.xml`

This is the **source of truth** for how multicast is handled:

```xml
<Packet name="OrionNetworkVideo" ID="ORION_PKT_NETWORK_VIDEO" 
        comment="Change the IP settings for video delivery.">
    
    <Data name="DestIp" inMemoryType="unsigned32" 
          comment="destination IPv4 address"/>
    
    <Data name="Port" inMemoryType="unsigned16" 
          comment="desitnation port"/>
    
    <Data name="Ttl" inMemoryType="signed8" default="-1" 
          comment="TTL value for UDP video packets. 
                   Values less than or equal to zero will use default values 
                   of 64 for unicast and 1 for multicast." />
    
    <Data name="StreamType" enum="StreamType_t" encodedType="unsigned8" 
          default="STREAM_TYPE_H264" />
    
    <!-- ... other fields ... -->
</Packet>
```

**Key Points**:
- `Ttl` field defaults to `-1` (auto-detection)
- Protocol comment explicitly states: "default values of 64 for unicast and 1 for multicast"
- This means **gimbal firmware** auto-detects multicast based on `DestIp`

### 2. Communication Layer

**File**: `Communications/OrionComm.h` / `OrionCommLinux.c`

The SDK uses **TCP** to communicate with the gimbal:

```c
// OrionComm.h
#define UDP_OUT_PORT        8745  // UDP for discovery only
#define UDP_IN_PORT         8746  // UDP for discovery only
#define TCP_PORT            8747  // TCP for actual communication

BOOL OrionCommSend(const OrionPkt_t *pPkt);
BOOL OrionCommReceive(OrionPkt_t *pPkt);
```

**How It Works**:
1. **Discovery Phase** (UDP):
   - Uses UDP broadcast on port 8745 to discover gimbal
   - Receives response on port 8746
   - Gets gimbal's IP address

2. **Communication Phase** (TCP):
   - Opens TCP connection to gimbal on port 8747
   - Sends `OrionNetworkVideo` packet via TCP
   - Gimbal receives packet and configures UDP video streaming

**Code Flow** (`OrionCommLinux.c`):

```c
BOOL OrionCommOpenNetworkIp(const char *pAddress)
{
    // 1. Create UDP socket for discovery
    int UdpHandle = socket(AF_INET, SOCK_DGRAM, 0);
    
    // 2. Bind to receive port
    bind(UdpHandle, GetSockAddr(INADDR_ANY, UDP_IN_PORT), ...);
    
    // 3. Enable broadcast
    setsockopt(UdpHandle, SOL_SOCKET, SO_BROADCAST, ...);
    
    // 4. Send discovery packet
    sendto(UdpHandle, &Pkt, ..., GetSockAddr(BroadcastAddr, UDP_OUT_PORT), ...);
    
    // 5. Receive gimbal response
    recvfrom(UdpHandle, Buffer, ..., ...);
    
    // 6. Open TCP connection to gimbal
    Handle = socket(AF_INET, SOCK_STREAM, 0);
    connect(Handle, GetSockAddr(Address, TCP_PORT), ...);
    
    return Handle != -1;
}

BOOL OrionCommSend(const OrionPkt_t *pPkt)
{
    // Send packet via TCP socket
    return write(Handle, (char *)pPkt, pPkt->Length + ORION_PKT_OVERHEAD) > 0;
}
```

**Important**: The SDK sends `OrionNetworkVideo` packet via **TCP**, not UDP. The gimbal then uses the configuration to stream video via **UDP**.

### 3. Packet Encoding/Decoding

**File**: `Utils/TrilliumPacket.c` / `TrilliumPacket.h`

The SDK uses a generic packet format called "Trillium":

```c
// TrilliumPacket.h
typedef struct
{
    UInt8 Sync0;      // Sync byte 0
    UInt8 Sync1;      // Sync byte 1
    UInt8 ID;         // Packet ID (e.g., ORION_PKT_NETWORK_VIDEO)
    UInt8 Length;     // Data length
    UInt8 Data[140];  // Payload data
    TrilliumPktInfo_t Info;  // Parsing state
} TrilliumPkt_t;

#define TRILLIUM_PKT_MAX_SIZE       140
#define TRILLIUM_PKT_HEADER_SIZE    4
#define TRILLIUM_PKT_OVERHEAD       (TRILLIUM_PKT_HEADER_SIZE + 2)
```

**Packet Structure**:
```
[Sync0][Sync1][ID][Length][Data...][Checksum0][Checksum1]
```

**Encoding Process** (`TrilliumPacket.c`):

```c
BOOL MakeTrilliumPacket(TrilliumPkt_t *pPkt, UInt16 Sync, UInt8 ID, UInt16 Length)
{
    // Set sync bytes
    pPkt->Sync0 = (UInt8)(Sync >> 8);
    pPkt->Sync1 = (UInt8)(Sync & 0xFF);
    pPkt->ID = ID;
    pPkt->Length = (UInt8)Length;
    
    // Calculate checksum over header + data
    // ... checksum calculation ...
    
    // Append checksum bytes
    pPkt->Data[Length++] = (UInt8)(Check0 & 0xFF);
    pPkt->Data[Length++] = (UInt8)(Check1 & 0xFF);
    
    return TRUE;
}
```

**For Orion Packets** (`Utils/OrionPublicPacketShim.h`):

```c
// Orion uses Trillium packet format with sync = 0xD00D
#define ORION_SYNC         0xD00D
typedef TrilliumPkt_t OrionPkt_t;
```

### 4. Protocol Code Generation

**Tool**: `Protogen/ProtoGen` (generates C code from XML)

The actual `encodeOrionNetworkVideoPacketStructure()` function is **generated** by ProtoGen from `OrionPublicProtocol.xml`. The generated code:

1. Takes `OrionNetworkVideo_t` structure
2. Encodes each field according to XML definition:
   - `DestIp` → `uint32_t` (network byte order)
   - `Port` → `uint16_t`
   - `Ttl` → `int8_t` ← **This is where TTL is encoded**
   - `StreamType` → `uint8_t`
   - etc.
3. Packs into `TrilliumPkt_t` structure
4. Calculates checksum

**Pseudo-code** (what ProtoGen generates):

```c
void encodeOrionNetworkVideoPacketStructure(OrionPkt_t *pkt, 
                                             const OrionNetworkVideo_t *settings)
{
    UInt8 *data = pkt->Data;
    int index = 0;
    
    // Encode DestIp (uint32_t, network byte order)
    uint32ToBeBytes(data, &index, settings->DestIp);
    
    // Encode Port (uint16_t)
    uint16ToBeBytes(data, &index, settings->Port);
    
    // Encode Bitrate (uint32_t)
    uint32ToBeBytes(data, &index, settings->Bitrate);
    
    // Encode Ttl (int8_t) - THIS IS WHERE TTL IS ENCODED
    int8ToBytes(data, &index, settings->Ttl);
    
    // Encode StreamType (uint8_t)
    uint8ToBytes(data, &index, settings->StreamType);
    
    // ... encode other fields ...
    
    // Create packet with encoded data
    MakeTrilliumPacket(pkt, ORION_SYNC, ORION_PKT_NETWORK_VIDEO, index);
}
```

## How Multicast Works in the SDK

### Step-by-Step Flow

1. **Application Code** (e.g., Skylink):
   ```c
   OrionNetworkVideo_t settings;
   settings.DestIp = inet_addr("239.255.1.1");  // Multicast IP
   settings.Port = 15004;
   settings.Ttl = 1;  // Or -1 for auto-detection
   
   encodeOrionNetworkVideoPacketStructure(&pkt, &settings);
   OrionCommSend(&pkt);
   ```

2. **SDK Encodes Packet**:
   - `encodeOrionNetworkVideoPacketStructure()` encodes `Ttl` field into packet
   - `MakeTrilliumPacket()` creates packet with checksum
   - Packet structure: `[Sync][ID][Length][DestIp][Port][Bitrate][Ttl][...][Checksum]`

3. **SDK Sends via TCP**:
   - `OrionCommSend()` writes packet to TCP socket
   - Packet transmitted to gimbal on port 8747

4. **Gimbal Receives Packet** (firmware, not in SDK):
   - Decodes `OrionNetworkVideo` packet
   - Reads `DestIp`, `Port`, `Ttl` fields
   - If `Ttl = -1`, checks if `DestIp` is multicast (224.x.x.x - 239.x.x.x)
   - Configures UDP socket with proper TTL
   - Starts streaming UDP video packets to `DestIp:Port`

5. **Video Streaming** (gimbal firmware, not SDK):
   - Gimbal sends UDP packets to multicast address
   - Multiple receivers can receive the stream
   - SDK is **not involved** in video streaming

## What the SDK Does NOT Do

The SDK **does not**:
- ❌ Stream video packets (gimbal does this)
- ❌ Receive video streams (applications use FFmpeg directly)
- ❌ Handle UDP video packets (gimbal sends, applications receive)
- ❌ Detect multicast at the SDK level (gimbal firmware does this)

The SDK **only**:
- ✅ Defines protocol (XML)
- ✅ Encodes configuration packets
- ✅ Sends packets to gimbal via TCP
- ✅ Receives status/response packets from gimbal

## Multicast Detection in SDK vs Gimbal

### SDK Level (What We Added)
- **Location**: `Examples/VideoPlayer/NetworkVideoConfig.h`
- **Purpose**: Help applications set correct `Ttl` value before encoding
- **Function**: `ConfigureNetworkVideoForMulticast()` detects multicast IPs and sets `Ttl = 1`

### Gimbal Firmware Level (Not in SDK)
- **Location**: Gimbal firmware (not in this repository)
- **Purpose**: Auto-detect multicast when `Ttl = -1`
- **Behavior**: Checks if `DestIp` is in range 224.0.0.0 - 239.255.255.255

## Protocol Fields Related to Multicast

### OrionNetworkVideo Packet

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `DestIp` | `uint32_t` | - | Destination IP (can be multicast) |
| `Port` | `uint16_t` | - | UDP port for video stream |
| `Ttl` | `int8_t` | `-1` | TTL: -1=auto, 1=multicast, 64=unicast |
| `StreamType` | `uint8_t` | `STREAM_TYPE_H264` | Video format |

### Other Related Packets

**VideoRecordStatus** (`OrionPublicProtocol.xml` line 1123):
- Contains `UdpDestIp`, `UdpPort` fields
- Shows current streaming status
- Used to query gimbal's current video configuration

**VideoRecordCmd** (`OrionPublicProtocol.xml` line 1149):
- Contains `EnableUdpStream` field
- Comment: "Enable a UDP stream, either uni-cast or multi-cast"
- Also has `UdpDestIp`, `UdpPort` fields

## Summary

### SDK's Role in Multicast

1. **Protocol Definition**: Defines `Ttl` field with auto-detection
2. **Packet Encoding**: Encodes `Ttl` value into packet sent to gimbal
3. **Communication**: Sends packet via TCP to gimbal
4. **No Video Handling**: SDK does not handle UDP video streaming

### Gimbal's Role (Not in SDK)

1. **Receives Configuration**: Gets `OrionNetworkVideo` packet via TCP
2. **Auto-Detection**: If `Ttl = -1`, detects multicast from `DestIp`
3. **UDP Streaming**: Configures UDP socket and streams video packets
4. **Multicast Support**: Handles multicast group membership, TTL, etc.

### Application's Role

1. **Configuration**: Sets `DestIp`, `Port`, `Ttl` in `OrionNetworkVideo_t`
2. **Encoding**: Calls `encodeOrionNetworkVideoPacketStructure()`
3. **Sending**: Calls `OrionCommSend()` to send to gimbal
4. **Receiving**: Uses FFmpeg or other tools to receive UDP video stream

## Key Code Locations (Non-Examples)

| Component | File | Purpose |
|-----------|------|---------|
| **Protocol Definition** | `Communications/OrionPublicProtocol.xml` | Defines `Ttl` field with auto-detection |
| **Communication** | `Communications/OrionComm.h` | TCP communication with gimbal |
| **TCP Implementation** | `Communications/OrionCommLinux.c` | Linux TCP socket implementation |
| **Packet Format** | `Utils/TrilliumPacket.h` | Generic packet structure |
| **Packet Encoding** | `Utils/TrilliumPacket.c` | Packet creation and checksum |
| **Orion Shim** | `Utils/OrionPublicPacketShim.h` | Orion-specific packet definitions |
| **Code Generator** | `Protogen/ProtoGen` | Generates encode/decode functions |

## Conclusion

The SDK is a **thin configuration layer** that:
- Defines the protocol for configuring video streaming
- Encodes and sends configuration packets to the gimbal
- Does **not** handle actual video streaming

Multicast support comes from:
1. **Protocol definition** allowing `Ttl` field with auto-detection
2. **Gimbal firmware** auto-detecting multicast IPs
3. **Applications** (like our VideoPlayer example) detecting multicast and setting `Ttl = 1`

The SDK itself has **no multicast-specific code** - it just encodes whatever `Ttl` value is set in the structure and sends it to the gimbal.
