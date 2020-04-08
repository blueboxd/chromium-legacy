#ifndef RLWE_BITS_UTIL_H_
#define RLWE_BITS_UTIL_H_

#include <stdint.h>

#include "absl/numeric/int128.h"
#include "integral_types.h"

namespace rlwe {
namespace internal {

inline unsigned int CountOnesInByte(Uint8 x) {
  Uint8 x0 = x & 0x55;
  Uint8 x1 = (x >> 1) & 0x55;
  x = x0 + x1;

  x0 = x & 0x33;
  x1 = (x >> 2) & 0x33;
  x = x0 + x1;

  x0 = x & 0x0F;
  x1 = (x >> 4) & 0x0F;
  return x0 + x1;
}

inline unsigned int CountOnes64(Uint64 x) {
  Uint64 x0 = x & 0x5555555555555555;
  Uint64 x1 = (x >> 1) & 0x5555555555555555;
  x = x0 + x1;

  x0 = x & 0x3333333333333333;
  x1 = (x >> 2) & 0x3333333333333333;
  x = x0 + x1;

  x0 = x & 0x0F0F0F0F0F0F0F0F;
  x1 = (x >> 4) & 0x0F0F0F0F0F0F0F0F;
  x = x0 + x1;

  x0 = x & 0x00FF00FF00FF00FF;
  x1 = (x >> 8) & 0x00FF00FF00FF00FF;
  x = x0 + x1;

  x0 = x & 0x0000FFFF0000FFFF;
  x1 = (x >> 16) & 0x0000FFFF0000FFFF;
  x = x0 + x1;

  x0 = x & 0x00000000FFFFFFFF;
  x1 = (x >> 32) & 0x00000000FFFFFFFF;
  return x0 + x1;
}

inline unsigned int CountLeadingZeros64(Uint64 x) {
  unsigned int zeros = 64;
  if (x >> 32) {
    zeros -= 32;
    x >>= 32;
  }
  if (x >> 16) {
    zeros -= 16;
    x >>= 16;
  }
  if (x >> 8) {
    zeros -= 8;
    x >>= 8;
  }
  if (x >> 4) {
    zeros -= 4;
    x >>= 4;
  }
  if (x >> 2) {
    zeros -= 2;
    x >>= 2;
  }
  if (x >> 1) {
    zeros -= 1;
    x >>= 1;
  }
  return zeros - x;
}

inline unsigned int CountLeadingZeros128(absl::uint128 x) {
  if (Uint64 hi = absl::Uint128High64(x)) return CountLeadingZeros64(hi);
  return CountLeadingZeros64(absl::Uint128Low64(x)) + 64;
}

}  // namespace internal
}  // namespace rlwe

#endif  // RLWE_BITS_UTIL_H_
