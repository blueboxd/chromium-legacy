// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_VAAPI_DEVICE_H_
#define MEDIA_GPU_VAAPI_TEST_VAAPI_DEVICE_H_

#include "base/files/file.h"

namespace media {
namespace vaapi_test {

// This class holds shared VA handles used by the various test decoders.
// The decoders themselves may still make direct libva calls.
class VaapiDevice {
 public:
  // Initializes a VaapiDevice for |profile|. Success is ASSERTed.
  explicit VaapiDevice(VAProfile profile);

  VaapiDevice(const VaapiDevice&) = delete;
  VaapiDevice& operator=(const VaapiDevice&) = delete;
  ~VaapiDevice();

  VADisplay display() const { return display_; }
  VAConfigID config_id() const { return config_id_; }
  VAProfile profile() const { return profile_; }
  unsigned int internal_va_format() const { return internal_va_format_; }

 private:
  // Initializes VA handles and display descriptors, checking that HW decode
  // with the expected profile is supported. Success is ASSERTed.
  void Initialize();

  base::File display_file_;

  // VA info and handles
  // Populated on Initialize().
  VADisplay display_;
  VAConfigID config_id_;
  const VAProfile profile_;
  const unsigned int internal_va_format_;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_VAAPI_DEVICE_H_
