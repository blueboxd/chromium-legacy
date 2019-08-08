// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"

#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace password_manager {

std::string ScryptHashUsernameAndPassword(const std::string& username,
                                          const std::string& password) {
  // Constant salt added to the password hash on top of username.
  static constexpr char kPasswordHashSalt[] = {
      48,   118, 42,  -46,  63,  123, -95, -101, -8,  -29, 66,
      -4,   -95, -89, -115, 6,   -26, 107, -28,  -37, -72, 79,
      -127, 83,  -59, 3,    -56, -37, -67, -34,  -91, 32};
  static constexpr size_t kHashKeyLength = 32;
  static constexpr uint64_t kScryptCost = 1 << 12;  // It must be a power of 2.
  static constexpr uint64_t kScryptBlockSize = 8;
  static constexpr uint64_t kScryptParallelization = 1;
  static constexpr size_t kScryptMaxMemory = 1024 * 1024 * 32;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  std::string username_password = username + password;
  std::string salt = base::StrCat(
      {username,
       base::StringPiece(kPasswordHashSalt, base::size(kPasswordHashSalt))});

  std::string result;
  uint8_t* key_data =
      reinterpret_cast<uint8_t*>(base::WriteInto(&result, kHashKeyLength + 1));

  int scrypt_ok =
      EVP_PBE_scrypt(username_password.data(), username_password.size(),
                     reinterpret_cast<const uint8_t*>(salt.data()), salt.size(),
                     kScryptCost, kScryptBlockSize, kScryptParallelization,
                     kScryptMaxMemory, key_data, kHashKeyLength);
  return scrypt_ok == 1 ? std::move(result) : std::string();
}

}  // namespace password_manager
