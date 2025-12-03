# Video Streaming Architecture - VideoPlayer and Gimbal Configuration

## Overview

The VideoPlayer example is **one way** to configure video streaming from the gimbal, but it's not necessarily the only instrument. Here's how the architecture works:

## How Video Streaming Works

### 1. Gimbal Configuration (Sender Side)
The gimbal needs to be configured to send video to a specific destination. This is done via the `OrionNetworkVideo` packet:

```c
OrionNetworkVideo_t Settings;
Settings.DestIp = <destination_ip>;
Settings.Port = <destination_port>;
Settings.StreamType = STREAM_TYPE_H264;
Settings.Ttl = <ttl_value>;  // -1 for auto, 1 for multicast, 64 for unicast
// ... other settings

encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
OrionCommSend(&PktOut);
```

**Key Point**: Any application using this SDK can send this packet to configure the gimbal's video streaming destination.

### 2. Video Reception (Receiver Side)
Once configured, the gimbal sends UDP video streams to the specified IP:port. Multiple applications can receive:
- **VideoPlayer**: Uses FFmpeg to receive and decode the stream
- **Skylink**: Likely uses its own receiver (possibly also FFmpeg-based)
- **Other FFmpeg scripts**: Can receive the stream directly

## Is VideoPlayer the Only Instrument?

**Answer: No.** VideoPlayer is an **example** that demonstrates:
1. How to configure the gimbal to stream video (via `OrionNetworkVideo` packet)
2. How to receive and decode the video stream (via FFmpeg)

### Other Applications Can:
- **Skylink**: Likely uses the same SDK to send `OrionNetworkVideo` packets and configure the gimbal
- **Custom applications**: Can use `encodeOrionNetworkVideoPacketStructure()` to configure video streaming
- **Multiple receivers**: Once the gimbal is configured, multiple applications can receive the stream (especially with multicast)

## Typical Workflow

### Scenario 1: VideoPlayer Only
```
VideoPlayer → sends OrionNetworkVideo packet → Gimbal streams to VideoPlayer's IP
VideoPlayer → receives stream via FFmpeg
```

### Scenario 2: Skylink + VideoPlayer (Your Case)
```
Skylink → sends OrionNetworkVideo packet → Gimbal streams to multicast IP
Skylink → receives multicast stream
VideoPlayer → receives multicast stream (same IP:port)
Another FFmpeg script → receives multicast stream
```

**Important**: In your scenario, **Skylink is configuring the gimbal** (sending the `OrionNetworkVideo` packet), and then multiple receivers (Skylink, VideoPlayer, ffmpeg script) are all trying to receive from the same multicast address.

## The `OrionNetworkVideo_t` Structure

From `OrionPublicProtocol.xml`:

```xml
<Packet name="OrionNetworkVideo" ID="ORION_PKT_NETWORK_VIDEO">
    <Data name="DestIp" ... comment="destination IPv4 address"/>
    <Data name="Port" ... comment="destination port"/>
    <Data name="Ttl" default="-1" comment="TTL value for UDP video packets. 
          Values less than or equal to zero will use default values of 
          64 for unicast and 1 for multicast." />
    <Data name="StreamType" ... />
    <!-- ... other fields ... -->
</Packet>
```

### Key Fields:
- **DestIp**: Where the gimbal sends video (can be unicast or multicast)
- **Port**: UDP port for video stream
- **Ttl**: 
  - `-1` (default): Gimbal auto-detects multicast vs unicast
  - `1`: Explicit multicast TTL
  - `64`: Explicit unicast TTL
- **StreamType**: H.264, MJPEG, etc.

## Why This Matters for Multicast Freezing

### The Problem:
1. **Skylink configures gimbal** → sends `OrionNetworkVideo` with multicast IP
2. **Gimbal starts streaming** → sends UDP packets to multicast address
3. **Multiple receivers** → Skylink, VideoPlayer, ffmpeg script all try to receive
4. **Freezing occurs** → because receivers aren't properly configured for multicast

### The Solution (Two Parts):

#### Part 1: Gimbal Configuration (Sender)
- Ensure `OrionNetworkVideo_t.Ttl` is set to `1` for multicast
- VideoPlayer now does this automatically when multicast IP is detected

#### Part 2: Receiver Configuration
- FFmpeg needs multicast socket options (`reuse=1`, larger buffers, etc.)
- VideoPlayer's StreamDecoder now handles this automatically

## Who Configures What?

| Component | Responsibility |
|-----------|---------------|
| **Skylink** | Configures gimbal (sends `OrionNetworkVideo` packet) |
| **Gimbal** | Sends UDP video stream to configured IP:port |
| **VideoPlayer** | Receives stream (can also configure gimbal if used standalone) |
| **FFmpeg script** | Receives stream (passive receiver) |

## Recommendations

1. **If Skylink configures the gimbal**: 
   - Ensure Skylink sets `Ttl=1` when using multicast
   - VideoPlayer fixes handle the receiver side

2. **If VideoPlayer configures the gimbal**:
   - VideoPlayer now automatically sets `Ttl=1` for multicast
   - Receiver side is also handled

3. **For multiple receivers**:
   - All receivers must use multicast socket options (`reuse=1`)
   - All receivers need adequate buffer sizes
   - Network must support IGMP for multicast

## Summary

- **VideoPlayer is NOT the only instrument** - any application using this SDK can configure video streaming
- **Skylink likely configures the gimbal** in your scenario
- **Multiple receivers can coexist** if properly configured for multicast
- **Both sender (gimbal) and receivers need proper configuration** for multicast to work reliably
