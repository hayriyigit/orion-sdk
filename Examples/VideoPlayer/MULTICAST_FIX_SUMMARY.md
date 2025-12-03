# Multicast Stream Freezing - Investigation Summary

## Problem
When using multicast IP addresses as the destination for video streams, the stream freezes when:
- Skylink application receives the stream via this SDK
- Another ffmpeg script also receives the multicast stream
- Both processes compete for the same multicast packets

## Root Cause Analysis

The VideoPlayer example was not configured for multicast reception. Key issues:

1. **Missing Multicast Socket Options**: FFmpeg needs specific options for multicast:
   - `reuse=1` to allow multiple receivers on the same address/port
   - Larger buffer sizes to handle multicast packet bursts
   - Graceful handling of buffer overruns

2. **Insufficient Buffer Sizes**: Default UDP socket buffers are too small for multicast traffic, causing packet drops

3. **No Error Recovery**: Packet loss wasn't handled gracefully, causing decoder to freeze

## Solution Implemented

### Code Changes

1. **Added Multicast Detection** (`StreamDecoder.c`)
   - New `IsMulticastAddress()` function detects multicast IPs (224.0.0.0 - 239.255.255.255)
   - Automatically configures FFmpeg with multicast options when detected

2. **Enhanced FFmpeg Options** (`StreamDecoder.c` - `StreamOpen()`)
   - Multicast streams: `reuse=1`, `fifo_size=5000000`, `buffer_size=2097152`, `overrun_nonfatal=1`
   - Unicast streams: Increased buffer size to 1MB

3. **Improved Error Handling** (`StreamDecoder.c` - `StreamProcess()`)
   - Captures `av_read_frame()` return value
   - Handles `AVERROR(EAGAIN)` gracefully (temporary unavailability)
   - Allows decoder to recover from transient errors

4. **Added Required Headers** (`FFmpeg.h`)
   - Included `libavutil/error.h` for `AVERROR` macro

## Files Modified

- `Examples/VideoPlayer/StreamDecoder.c` - Main fixes
- `Examples/VideoPlayer/FFmpeg.h` - Added error header
- `Examples/VideoPlayer/MULTICAST_ANALYSIS.md` - Detailed analysis

## Testing Recommendations

1. **Single Receiver Test**: Verify stream works with one receiver
2. **Dual Receiver Test**: Add second receiver and verify no freezing
3. **Monitor Packet Loss**: Use `netstat -s | grep -i udp` to check for drops
4. **Check Buffers**: Use `ss -u -m` to verify buffer sizes

## Usage

No changes needed - fixes are automatic. When you pass a multicast address:

```bash
./VideoPlayer gimbal_ip 239.255.1.1 15004
```

The code automatically detects multicast and applies the appropriate configuration.

## Additional Notes

- Ensure your multicast sender uses proper packet sizes (`pkt_size=1316`)
- Verify network switches support IGMP for multicast
- Consider network topology when setting TTL values
