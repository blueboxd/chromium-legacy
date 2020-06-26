// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/mojom/decrypt_config_mojom_traits.h"

#include "media/base/encryption_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(DecryptConfigStructTraitsTest, ConvertEncryptionPattern) {
  auto input = media::EncryptionPattern(22, 42);
  std::vector<uint8_t> data =
      chromeos::cdm::mojom::EncryptionPattern::Serialize(&input);

  media::EncryptionPattern output;
  EXPECT_TRUE(chromeos::cdm::mojom::EncryptionPattern::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(input.crypt_byte_block(), output.crypt_byte_block());
  EXPECT_EQ(input.skip_byte_block(), output.skip_byte_block());
}

}  // namespace chromeos