// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/tools/certificate_tag.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <utility>

#include "base/logging.h"
#include "base/time/time.h"

namespace updater {
namespace tools {
namespace {

// The number of bits in the RSA modulus of the key.
constexpr int kRsaKeyBits = 2048;

// kNotBeforeTime and kNotAfterTime are the validity period of the
// certificate. They are deliberately set so that they are already expired.
constexpr char kNotBeforeTime[] = "Mon Jan 1 10:00:00 UTC 2013";
constexpr char kNotAfterTime[] = "Mon Apr 1 10:00:00 UTC 2013";

// The structures here were taken from "Microsoft Portable Executable and
// Common Object File Format Specification".
constexpr int fileHeaderSize = 20;

// FileHeader represents the IMAGE_FILE_HEADER structure (the COFF header
// format) from
// https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_file_header.
struct FileHeader {
  uint16_t machine;
  uint16_t number_of_sections;
  uint32_t time_date_stamp;
  uint32_t pointer_for_symbol_table;
  uint32_t number_of_symbols;
  uint16_t size_of_optional_header;
  uint16_t characteristics;
};

// OptionalHeader represents the IMAGE_OPTIONAL_HEADER structure from
// https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_optional_header32.
struct OptionalHeader {
  uint16_t magic;
  uint8_t major_linker_version;
  uint8_t minor_linker_version;
  uint32_t size_of_code;
  uint32_t size_of_initialized_data;
  uint32_t size_of_uninitialized_data;
  uint32_t address_of_entry_point;
  uint32_t base_of_code;
};

// DataDirectory represents the IMAGE_DATA_DIRECTORY structure from
// https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_data_directory.
struct dataDirectory {
  uint32_t virtual_address;
  uint32_t size;
};

// A subset of the known COFF "characteristic" flags found in
// fileHeader.Characteristics.
constexpr int kCoffCharacteristicExecutableImage = 2;
constexpr int kCoffCharacteristicDLL = 0x2000;
constexpr int kPE32Magic = 0x10b;
constexpr int kPE32PlusMagic = 0x20b;

// Certificate constants. See
// https://docs.microsoft.com/en-us/windows/win32/api/wintrust/ns-wintrust-win_certificate.
// Despite MSDN claiming that 0x100 is the only, current revision - in practice
// it is 0x200.
constexpr int kAttributeCertificateRevision = 0x200;
constexpr int kAttributeCertificateTypePKCS7SignedData = 2;
constexpr int kCertificateTableIndex = 4;

}  // namespace

Binary::Binary() = default;
Binary::~Binary() = default;

bool ParseUnixTime(const char* time_string, base::Time* parsed_time) {
  return base::Time::FromUTCString(time_string, parsed_time);
}

std::vector<uint8_t> AppendedTag(const Binary& bin) {
  const bool is_all_zero =
      std::all_of(std::begin(bin.appended_tag), std::end(bin.appended_tag),
                  [](uint8_t i) { return i == 0; });
  if (is_all_zero && bin.appended_tag.size() < 8)
    return {};
  return bin.appended_tag;
}

std::vector<uint8_t> RemoveAppendedTag(const Binary& bin) {
  const auto appended_tag = AppendedTag(bin);
  if (appended_tag.empty())
    std::cerr << "authenticodetag: no appended tag found";
  return BuildBinary(bin, bin.asn1_data, {});
}

std::vector<uint8_t> SetAppendedTag(const Binary& bin,
                                    const std::vector<uint8_t>& tag_contents) {
  return BuildBinary(bin, bin.asn1_data, tag_contents);
}

int GetAttributeCertificates(const std::vector<uint8_t>& contents,
                             size_t* offset,
                             size_t* size,
                             size_t* cert_size_offset) {
  // TODO(agl): implement this. crbug.com/1031734.
  NOTREACHED();
  return -1;
}

int ProcessAttributeCertificates(
    const std::vector<uint8_t>& attribute_certificates,
    std::vector<uint8_t>* asn1_data,
    std::vector<uint8_t>* appended_tag) {
  // TODO(agl): implement this. crbug.com/1031734.
  NOTREACHED();
  return -1;
}

std::vector<uint8_t> SetSuperfluousCertTag(const Binary& bin,
                                           const std::vector<uint8_t>& tag) {
  // TODO(agl): implement this. crbug.com/1031734.
  NOTREACHED();
  return {};
}

std::vector<uint8_t> BuildBinary(const Binary& bin,
                                 const std::vector<uint8_t>& asn1_data,
                                 const std::vector<uint8_t>& tag) {
  // TODO(agl): implement this. crbug.com/1031734.
  NOTREACHED();
  return {};
}

std::tuple<std::unique_ptr<Binary>, int /*err*/> NewBinary(
    const std::vector<uint8_t>& contents) {
  size_t offset = 0;
  size_t size = 0;
  size_t cert_size_offset;
  int err =
      GetAttributeCertificates(contents, &offset, &size, &cert_size_offset);
  if (err) {
    std::cerr << "authenticodetag: error parsing headers: " << err;
    std::make_tuple(nullptr, err);
  }

  const std::vector<uint8_t> attribute_certificates{&contents[offset],
                                                    &contents[offset + size]};
  std::vector<uint8_t> asn1_data;
  std::vector<uint8_t> appended_tag;
  err = ProcessAttributeCertificates(attribute_certificates, &asn1_data,
                                     &appended_tag);
  if (err) {
    std::cerr
        << "authenticodetag: error parsing attribute certificate section: "
        << err;
    std::make_tuple(nullptr, err);
  }

  // TODO(agl): ASN1 unmarshaling magic here. crbug.com/1031734.

  auto binary = std::make_unique<Binary>();
  binary->contents = contents;
  binary->attr_cert_offset = offset;
  binary->cert_size_offset = cert_size_offset;
  binary->asn1_data = std::move(asn1_data);
  binary->appended_tag = std::move(appended_tag);

  return std::make_tuple(std::move(binary), 0);
}

}  // namespace tools
}  // namespace updater
