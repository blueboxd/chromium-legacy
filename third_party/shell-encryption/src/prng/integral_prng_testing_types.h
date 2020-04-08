#ifndef RLWE_PRNG_INTEGRAL_PRNG_TYPE_H_
#define RLWE_PRNG_INTEGRAL_PRNG_TYPE_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "prng/chacha_prng.h"
#include "prng/single_thread_chacha_prng.h"

namespace rlwe {

typedef ::testing::Types<ChaChaPrng, SingleThreadChaChaPrng> TestingPrngTypes;

}  // namespace rlwe

#endif  // RLWE_PRNG_INTEGRAL_PRNG_TYPE_H_
