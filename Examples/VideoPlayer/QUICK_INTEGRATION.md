# Quick Integration Guide

## How to Use the Improvements in Your Code

Yes! You can force `encodeOrionNetworkVideoPacketStructure()` to use the improvements by calling `ConfigureNetworkVideoForMulticast()` **before** encoding.

## The Pattern

```c
// 1. Include the header
#include "NetworkVideoConfig.h"

// 2. Set up your settings
OrionNetworkVideo_t Settings;
memset(&Settings, 0, sizeof(Settings));
Settings.DestIp = <your_ip>;
Settings.Port = 15004;
Settings.StreamType = STREAM_TYPE_H264;

// 3. Apply the improvements (THIS IS THE KEY STEP!)
ConfigureNetworkVideoForMulticast(&Settings);

// 4. Encode and send (as before)
encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
OrionCommSend(&PktOut);
```

## What Changes?

**Before**:
```c
Settings.Ttl = 1;  // Manual - might be wrong!
encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
```

**After**:
```c
ConfigureNetworkVideoForMulticast(&Settings);  // Automatic!
encodeOrionNetworkVideoPacketStructure(&PktOut, &Settings);
```

## Key Benefits

✅ **Automatic TTL configuration** - Sets TTL=1 for multicast, leaves default for unicast
✅ **No manual TTL needed** - Function detects multicast automatically
✅ **Prevents errors** - Can't set wrong TTL value
✅ **Works everywhere** - Use in Skylink, VideoPlayer, or any app

## File Location

Copy `NetworkVideoConfig.h` to your project and include it:
- Location: `Examples/VideoPlayer/NetworkVideoConfig.h`
- Header-only (no .c file needed)
- Functions are `static inline` (compiled into your code)

## Example: Integration in Skylink

If Skylink configures video streaming, add this before encoding:

```c
// In your video configuration function
OrionNetworkVideo_t videoSettings;
// ... populate videoSettings ...

// Add this line before encodeOrionNetworkVideoPacketStructure()
ConfigureNetworkVideoForMulticast(&videoSettings);

// Then encode as usual
encodeOrionNetworkVideoPacketStructure(&pkt, &videoSettings);
```

That's it! The improvements are now applied automatically.
