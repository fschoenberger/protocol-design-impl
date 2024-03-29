#pragma once

#include "pch.hpp"

namespace rft {

enum class MessageType: U8 {
    kClientHello = 0x1,
    kServerHello = 0x2,
    kAck = 0x3,
    kFin = 0x4,
    kError = 0xFF,
    kChunk = 0x00
};

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

struct PACKED MessageBase {
    U16 streamId;
    MessageType messageType;
    U64 sequenceNumber;
};

constexpr static size_t MAX_FILENAME_SIZE = 1024 - 8 /* UDP Frame*/ - 1 - 1 - 1 - 2 - 4 - sizeof(MessageBase);
struct PACKED ClientHello final : MessageBase {
    U8 version;
    U8 nextHeaderType;
    U8 nextHeaderOffset;
    U16 windowInMessages;
    U32 startChunk;
    char fileName[MAX_FILENAME_SIZE];
};
static_assert(sizeof(ClientHello) + 8 == 1024);

struct PACKED ServerHello final : MessageBase {
    U8 version;
    U8 nextHeaderType;
    U8 nextHeaderOffset;
    U16 windowInMessages;
    std::array<U64, 4> checksum;
    I64 lastModified;
    U64 fileSizeInBytes;
};

struct PACKED AckMessage final : MessageBase {
    U16 windowInMessages;
    U64 ackNumber;
};

struct PACKED FinMessage final : MessageBase {
};

struct PACKED ErrorMessage final : MessageBase {
    U8 errorCategory;
    U8 errorCode;
    std::string message;
};

struct PACKED ChunkMessage final : MessageBase {
    std::array<U8, 8> checksum;
    std::array<U8, 997> payload;
};

static_assert(sizeof(ChunkMessage) + 8 == 1024);

}

#ifdef _MSC_VER
#pragma pack(pop)
#endif
