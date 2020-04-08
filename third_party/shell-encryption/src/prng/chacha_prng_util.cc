#include "prng/chacha_prng_util.h"

#include <cstdint>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include <openssl/chacha.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include "status_macros.h"

namespace rlwe {
namespace internal {

absl::Status ChaChaPrngResalt(absl::string_view key, int buffer_size,
                              int* salt_counter, int* position_in_buffer,
                              std::vector<Uint8>* buffer) {
  buffer->assign(buffer_size, 0);
  std::string salt = "salt";
  if (salt.size() > kChaChaNonceSize) {
    return absl::InternalError("The salt length is too large.");
  }
  salt.resize(kChaChaNonceSize, 0);

  CRYPTO_chacha_20(buffer->data(), buffer->data(), buffer->size(),
                   reinterpret_cast<const Uint8*>(key.data()),
                   reinterpret_cast<const Uint8*>(salt.data()),
                   static_cast<uint32_t>(*salt_counter));

  ++(*salt_counter);
  *position_in_buffer = 0;
  return absl::OkStatus();
}

rlwe::StatusOr<std::string> ChaChaPrngGenerateKey() {
  std::unique_ptr<Uint8[]> buf(new Uint8[kChaChaKeyBytesSize]);
  // BoringSSL documentation says that it always returns 1; while
  // OpenSSL documentation says that it returns 1 on success, 0 otherwise. Check
  // for an error just in case.
  if (RAND_bytes(buf.get(), kChaChaKeyBytesSize) == 0) {
    return absl::InternalError("Internal error generating random PRNG key.");
  }
  return std::string(reinterpret_cast<const char*>(buf.get()),
                     kChaChaKeyBytesSize);
}

rlwe::StatusOr<Uint8> ChaChaPrngRand8(absl::string_view key,
                                      int* position_in_buffer,
                                      int* salt_counter,
                                      std::vector<Uint8>* buffer) {
  Uint8 rand;
  if (*position_in_buffer >= buffer->size()) {
    RLWE_RETURN_IF_ERROR(ChaChaPrngResalt(key, kChaChaOutputBytes, salt_counter,
                                          position_in_buffer, buffer));
  }
  rand = buffer->at(*position_in_buffer);
  ++(*position_in_buffer);
  return rand;
}

rlwe::StatusOr<Uint64> ChaChaPrngRand64(absl::string_view key,
                                        int* position_in_buffer,
                                        int* salt_counter,
                                        std::vector<Uint8>* buffer) {
  Uint64 rand64 = 0;
  for (int i = 0; i < 8; ++i) {
    RLWE_ASSIGN_OR_RETURN(Uint8 rand8, ChaChaPrngRand8(key, position_in_buffer,
                                                       salt_counter, buffer));
    rand64 += Uint64{rand8} << (8 * i);
  }
  return rand64;
}

}  // namespace internal
}  // namespace rlwe
