#pragma once

struct z_stream {
    unsigned char* next_in;
    unsigned long avail_in;
    unsigned char* next_out;
    unsigned long avail_out;
};

constexpr int Z_OK = 0;
constexpr int Z_STREAM_END = 1;
constexpr int Z_FINISH = 2;
using uInt = unsigned int;

inline int inflateInit(z_stream* stream) { return Z_OK; }
inline int inflate(z_stream* stream, int flush) { return Z_STREAM_END; }
inline int inflateEnd(z_stream* stream) { return Z_OK; }
