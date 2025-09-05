#pragma once

#include <cassert>
#include <cstdint>
#include <enklume/block_data.h>
#include "chunk_access.h"

struct n_idx { int x, y, z; };

inline bool isOccluded(const ChunkData* chunk, ChunkNeighborsUnsafe& neighbors, const int x, const int y, const int z) {
    const std::array<n_idx, 6> n_idxs{{
        {x, y+ 1, z},
        {x, y- 1, z},
        {x + 1, y, z},
        {x - 1, y, z},
        {x, y, z + 1},
        {x, y, z - 1}
    }};

    for (const auto &[x, y, z] : n_idxs) {
        if (access_safe(chunk, neighbors, x, y, z) == BlockAir) return false;
    }

    return true;
}

// returns position of first non-zero from LSB to MSB
inline int trailingZeros(const uint16_t mask) {
    if (mask == 0) return 0; // may be undefined if mask[x] == 0
    #if defined(__GNUC__) || defined(__clang__)
        return __builtin_ctz(mask);
    #else
        assert(false, "Unsupported compiler :P");
    #endif
    // TODO: Add fallback alg or something
}

// returns position of first 0 from LSB to MSB
inline int trailingOnes(const uint16_t mask) {
    return (mask == 0xFFFF) ? 16 : trailingZeros(~mask);
}

class BitMask {
public:
    uint16_t mask[CUNK_CHUNK_SIZE] = {};
    BlockId type = BlockAir;
    static_assert(CUNK_CHUNK_SIZE == 16, "BitMask relies on uint16_t, so chunk size must also be 16.");

    BitMask() = default;
    BitMask(const ChunkData* chunk, ChunkNeighborsUnsafe& neighbors, const int y, const BlockId t) : type(t) {
        for (int x = 0; x < CUNK_CHUNK_SIZE; x++) {
            for (int z = 0; z < CUNK_CHUNK_SIZE; z++) {
                if (!isOccluded(chunk, neighbors, x, y, z) && access_safe(chunk, neighbors, x, y, z) == t)
                    setBit(x, z);
            }
        }
    }

    bool isSet(const int x, const int z) const { return mask[x] & (1u << z); }

    void setBit(const int x, const int z) {
        // z == 0 => LSB, z == 15 => MSB
        assert(!isSet(x, z));
        mask[x] |= (1u << z);
    }
};