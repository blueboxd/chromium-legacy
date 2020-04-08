#ifndef RLWE_PRNG_INTEGRAL_PRNG_TYPE_H_
#define RLWE_PRNG_INTEGRAL_PRNG_TYPE_H_

#include "prng/chacha_prng.h"
#include "prng/single_thread_chacha_prng.h"

namespace rlwe {

typedef SingleThreadChaChaPrng SingleThreadPrng;
typedef ChaChaPrng Prng;

}  // namespace rlwe

#endif  // RLWE_PRNG_INTEGRAL_PRNG_TYPE_H_
