#pragma once
#include <endian.h>
#include <stdint.h>

//主机字节序和网络字节序的转化 //以下函数不是posix标准
namespace sockets
{   
    inline uint64_t hostToNetwork64(uint64_t host64) 
    {
        return htobe64(host64);
    }

    inline uint32_t hostToNetwork32(uint32_t host32)
    {
        return htobe32(host32);
    }

    inline uint16_t hostToNetwork16(uint16_t host16)
    {
        return htobe16(host16);
    }

    inline uint64_t networkToHost64(uint64_t host64)
    {
        return be64toh(host64);
    }

    inline uint32_t networkToHost32(uint32_t host32)
    {
        return be32toh(host32);
    }

    inline uint16_t networkToHost16(uint16_t host16)
    {
        return be16toh(host16);
    }

} // namespace sockets
