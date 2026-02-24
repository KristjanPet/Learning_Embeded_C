#pragma once
#include <cstddef>
#include <cstdint>

struct SdFlushPolicy{
    uint32_t flush_period_ms;
    size_t watermark_bytes;
};

struct SdFlushState{
    uint32_t last_flush_ms;
};

inline bool should_flush(const SdFlushPolicy& p, const SdFlushState& st, size_t buf_len, uint32_t now_ms){
    if (buf_len == 0) return false;
    if (buf_len >= p.watermark_bytes) return true;
    if ((now_ms - st.last_flush_ms) >= p.flush_period_ms) return true;
    return false;
}