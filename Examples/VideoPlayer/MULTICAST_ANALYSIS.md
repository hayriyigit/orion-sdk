# Multicast Stream Freezing Analysis

## Problem Description
When the destination IP:port is set to multicast, the stream freezes. This occurs when:
- Skylink application uses the stream (via this SDK)
- Another ffmpeg script is used to make it multicast
- Both are trying to receive from the same multicast address

## Root Causes

### 1. Missing Multicast Socket Options
The current implementation in `StreamDecoder.c` doesn't configure FFmpeg with multicast-specific options. When receiving multicast UDP streams, FFmpeg needs:

- **SO_REUSEADDR/SO_REUSEPORT**: Allows multiple processes to bind to the same multicast address/port
- **IP_ADD_MEMBERSHIP**: Joins the multicast group (required for receiving multicast traffic)
- **Larger receive buffers**: Multicast can have higher packet rates and needs larger socket buffers

### 2. FFmpeg UDP Options Not Configured
In `StreamDecoder.c:42`, only a timeout is set:
```c
av_dict_set(&pOptions, "timeout", "5000000", 0);
```

Missing critical multicast options:
- `reuse=1` - Reuse socket address (allows multiple receivers)
- `fifo_size` - Buffer size (default may be too small for multicast)
- `overrun_nonfatal=1` - Handle buffer overruns gracefully instead of failing
- `buffer_size` - UDP receive buffer size

### 3. Socket Buffer Size Issues
Multicast streams can have:
- Higher packet rates
- Burst traffic patterns
- Multiple simultaneous receivers competing for packets

The default UDP socket buffer size (typically 200KB-1MB) may be insufficient, causing packet drops and stream freezing.

### 4. Multiple Receiver Competition
When both Skylink and another ffmpeg process receive multicast:
- Both processes compete for the same packets
- If one process has a smaller buffer or slower processing, it may miss packets
- Packet loss causes decoder to freeze waiting for keyframes

## Code Locations

### StreamDecoder.c
- **Line 42**: Only timeout option is set - missing multicast options
- **Line 45**: `avformat_open_input()` called without multicast socket configuration
- **Line 164**: `av_read_frame()` loop may fail silently on packet loss

### VideoPlayer.c
- **Line 295**: Creates UDP URL but doesn't detect multicast addresses
- No differentiation between unicast and multicast handling

## Implemented Fixes

### Fix 1: Added Multicast Detection and Options ✅
**Location**: `StreamDecoder.c` - Added `IsMulticastAddress()` helper function and multicast detection

**Changes**:
- Added helper function to detect multicast IP addresses (224.0.0.0 - 239.255.255.255)
- Automatically detects multicast addresses from UDP URLs (supports both `udp://ip:port` and `udp://@ip:port` formats)
- When multicast is detected, automatically sets:
  - `reuse=1` - Allows multiple processes to bind to the same multicast address/port
  - `fifo_size=5000000` - 5MB FIFO buffer to handle packet bursts
  - `overrun_nonfatal=1` - Handles buffer overruns gracefully instead of failing
  - `buffer_size=2097152` - 2MB UDP socket receive buffer
  - `ttl=1` - Multicast TTL setting
- For unicast streams, still increases buffer size to 1MB to prevent packet loss

### Fix 2: Enhanced Error Handling ✅
**Location**: `StreamDecoder.c` - Updated `StreamProcess()` function

**Changes**:
- Changed `av_read_frame()` to capture return value
- Added error handling for `AVERROR(EAGAIN)` - temporary unavailability (not a fatal error)
- Other errors are logged but don't cause immediate failure, allowing decoder to recover

### Fix 3: Added Required Headers ✅
**Location**: `FFmpeg.h`

**Changes**:
- Added `#include "libavutil/error.h"` to ensure `AVERROR` macro is available

## How to Use

The fixes are automatic - no code changes needed in your application. When you pass a multicast IP address to VideoPlayer, it will:

1. Automatically detect that it's multicast
2. Configure FFmpeg with appropriate multicast options
3. Handle packet loss more gracefully

**Example usage**:
```bash
./VideoPlayer gimbal_ip 239.255.1.1 15004
```

The multicast address `239.255.1.1` will be automatically detected and configured with the multicast options.

## Additional Recommendations

### For the Sender (FFmpeg Script)
Ensure your multicast sender script uses proper options:
```bash
ffmpeg -i input -c copy -f mpegts "udp://239.255.1.1:15004?pkt_size=1316&ttl=1"
```

Key sender options:
- `pkt_size=1316` - Optimal UDP packet size (avoids fragmentation)
- `ttl=1` - Multicast TTL (adjust based on network topology)

### Network Configuration
- Ensure IGMP is enabled on network switches/routers
- Check that multicast traffic is not being filtered
- Verify network interface supports multicast (most do by default)

### Monitoring
To verify the fixes are working:
```bash
# Check UDP statistics for packet drops
netstat -s | grep -i udp

# Monitor socket buffer usage
ss -u -m

# Test with tcpdump to verify multicast packets are arriving
tcpdump -i any -n 'udp port 15004'
```

## Testing Recommendations

1. **Monitor packet loss**: Use `netstat -s` or `ss -u -a` to check UDP packet statistics
2. **Check socket buffers**: Use `ss -m` to verify buffer sizes
3. **Test with single receiver**: Verify stream works with only one receiver
4. **Gradually add receivers**: Add second receiver and monitor for freezing
5. **Check network interface**: Ensure multicast is properly configured on the network interface

## Additional Notes

- The issue may also be in the **sender** (Skylink/ffmpeg script) - ensure it's properly configured for multicast
- Network switches/routers must support IGMP for multicast to work properly
- Consider using `udp://@ip:port` format (with @) which tells FFmpeg to bind to the multicast address
- For production, consider using a dedicated multicast streaming library instead of raw FFmpeg UDP
