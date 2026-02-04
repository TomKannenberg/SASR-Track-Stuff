#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#define ZLIB_WINAPI
#include <zlib.h>

static uint32_t ReadU32LE(const std::vector<uint8_t>& d, size_t o){
    return (uint32_t)d[o] | ((uint32_t)d[o+1]<<8) | ((uint32_t)d[o+2]<<16) | ((uint32_t)d[o+3]<<24);
}
static uint32_t ReadU32BE(const std::vector<uint8_t>& d, size_t o){
    return ((uint32_t)d[o]<<24) | ((uint32_t)d[o+1]<<16) | ((uint32_t)d[o+2]<<8) | (uint32_t)d[o+3];
}
static uint32_t MakeType(char a,char b,char c,char d){
    return (uint32_t)(uint8_t)a | ((uint32_t)(uint8_t)b<<8) | ((uint32_t)(uint8_t)c<<16) | ((uint32_t)(uint8_t)d<<24);
}
static uint32_t MakeTypeBE(char a,char b,char c,char d){
    return ((uint32_t)(uint8_t)a<<24) | ((uint32_t)(uint8_t)b<<16) | ((uint32_t)(uint8_t)c<<8) | (uint32_t)(uint8_t)d;
}
static bool LooksLikeSif(const std::vector<uint8_t>& d){
    if(d.size()<0x10) return false;
    size_t off=0; if(d.size()>=4){ auto len=ReadU32LE(d,0); if(len==d.size()-4||len==d.size()) off=4; }
    if(off+4>d.size()) return false;
    uint32_t t=ReadU32LE(d,off);
    switch(t){
        case 0x4B415254: case 0x45524F46: case 0x494C4F43: case 0x43474F4C: case 0x58455450: case 0x4F464E49:
            return true;
        default: return false;
    }
}
static bool LooksLikeSifBE(const std::vector<uint8_t>& d){
    if(d.size()<0x10) return false;
    size_t off=0; if(d.size()>=4){ auto len=ReadU32BE(d,0); if(len==d.size()-4||len==d.size()) off=4; }
    if(off+4>d.size()) return false;
    uint32_t t=ReadU32BE(d,off);
    switch(t){
        case 0x5452414B: case 0x464F5245: case 0x434F4C49: case 0x4C4F4743: case 0x50544558: case 0x494E464F:
            return true;
        default: return false;
    }
}

static bool IsZlibHeader(uint8_t cmf, uint8_t flg){
    if((cmf & 0x0F) != 8) return false;
    int val = ((int)cmf<<8) | flg;
    return (val % 31)==0;
}

static bool InflateFrom(const std::vector<uint8_t>& src, size_t offset, std::vector<uint8_t>& out){
    z_stream zs{};
    zs.next_in = (Bytef*)(&src[offset]);
    zs.avail_in = (uInt)(src.size()-offset);
    if(inflateInit(&zs)!=Z_OK) return false;
    out.clear();
    std::vector<uint8_t> buf(1<<15);
    int ret=Z_OK;
    while(ret==Z_OK){
        zs.next_out = buf.data();
        zs.avail_out = (uInt)buf.size();
        ret = inflate(&zs, Z_FINISH);
        size_t have = buf.size() - zs.avail_out;
        if(have) out.insert(out.end(), buf.begin(), buf.begin()+have);
    }
    inflateEnd(&zs);
    return (ret==Z_STREAM_END);
}

int main(int argc,char**argv){
    if(argc<3){ std::cerr<<"Usage: zlib_carve <file> <outdir>\n"; return 1; }
    std::filesystem::path in=argv[1];
    std::filesystem::path outdir=argv[2];
    std::ifstream f(in, std::ios::binary); if(!f){ std::cerr<<"open fail\n"; return 1; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),{});
    std::filesystem::create_directories(outdir);
    int found=0, ok=0;
    for(size_t i=0;i+1<data.size();++i){
        if(!IsZlibHeader(data[i], data[i+1])) continue;
        ++found;
        std::vector<uint8_t> out;
        if(!InflateFrom(data,i,out)) continue;
        ++ok;
        bool sif = LooksLikeSif(out) || LooksLikeSifBE(out);
        std::string name = "offset_" + std::to_string(i) + (sif?"_sif":"") + ".bin";
        std::ofstream of(outdir / name, std::ios::binary); of.write((char*)out.data(), out.size());
    }
    std::cout<<"zlib headers: "<<found<<"\n";
    std::cout<<"inflated ok: "<<ok<<"\n";
    return 0;
}
