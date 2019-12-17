// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TOOLS_CERTIFICATE_TAG_H_
#define CHROME_UPDATER_TOOLS_CERTIFICATE_TAG_H_

#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

namespace base {
class Time;
}

namespace updater {
namespace tools {

struct SignedData;

// Binary represents a PE binary.
struct Binary {
  Binary();
  ~Binary();
  Binary(const Binary&) = delete;
  Binary& operator=(const Binary&) = delete;

  // The full file.
  std::vector<uint8_t> contents;

  // The offset to the attribute certificates table.
  size_t attr_cert_offset = 0;

  // The offset to the size of the attribute certificates table.
  size_t cert_size_offset = 0;

  // The PKCS#7, SignedData in DER form.
  std::vector<uint8_t> asn1_data;

  // The appended tag, if any.
  std::vector<uint8_t> appended_tag;

  // The parsed, SignedData structure.
  SignedData* signed_data = nullptr;
};

bool ParseUnixTime(const char* time_string, base::Time* parsed_time);

std::tuple<std::unique_ptr<Binary>, int /*err*/> NewBinary(
    std::vector<uint8_t> contents);
std::vector<uint8_t> BuildBinary(const Binary& bin,
                                 const std::vector<uint8_t>& asn1_data,
                                 const std::vector<uint8_t>& tag);

int ProcessAttributeCertificates(
    const std::vector<uint8_t>& attribute_certificates,
    std::vector<uint8_t>* asn1_data,
    std::vector<uint8_t>* appended_tag);

std::vector<uint8_t> AppendedTag(const Binary& bin);
std::vector<uint8_t> RemoveAppendedTag(const Binary& bin);
std::vector<uint8_t> SetAppendedTag(const Binary& bin,
                                    const std::vector<uint8_t>& tag_contents);

// SetSuperfluousCertTag returns a PE binary based on bin, but where the
// superfluous certificate contains the given tag data.
std::vector<uint8_t> SetSuperfluousCertTag(const Binary& bin,
                                           const std::vector<uint8_t>& tag);
}  // namespace tools
}  // namespace updater

#endif  // CHROME_UPDATER_TOOLS_CERTIFICATE_TAG_H_
