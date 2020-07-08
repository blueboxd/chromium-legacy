// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/qr_code_generator/qr_code_generator.h"

#include <string.h>

#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"

using GeneratedCode = QRCodeGenerator::GeneratedCode;
using QRVersionInfo = QRCodeGenerator::QRVersionInfo;

namespace {
// Default version five QR Code.
constexpr int kVersionDefault = 5;
// Extended-length QR code version used by service.
constexpr int kVersionExtended = 7;
// Threshold for switching between the two supported versions.
constexpr int kLargeVersionThresholdLength = 84;
}  // namespace

// TODO(skare): tracking some items to resolve before submit in this block.
//  - In the QRVersionInfo comment, "Error correction group" may not be a formal
//  term in the spec. OK?
//    if so, change naming: Group 0/1 -> Group 1/2 (1-based indexing).
constexpr QRCodeGenerator::QRVersionInfo version_infos[] = {
    // Version data is specified as:
    //   version, size, total_bytes.
    // Error correction Group 0 [see Table 9]
    //   group_bytes, num_segments, segment_data_bytes
    // Error correction Group 1
    // [may not apply for all versions, in which case num_segments is 0]
    //   group_bytes, num_segments, segment_data_bytes
    // total_bytes for the overall code, and {num_segments, segment_data_bytes}
    // or each group are available on table 9, page 38 of the spec.
    // group_bytes may be calculated as num_segments*c from the table.

    // 5-M
    // 134 bytes, as 2 segments of 67.
    {5, 37, 134, 134, 2, 43, 0, 0, 0},

    // 7-M
    // 196 bytes, as 4 segments of 49.
    {7, 45, 196, 196, 4, 31, 0, 0, 0},
};

// static
const QRVersionInfo* QRCodeGenerator::GetVersionInfo(int version) {
  for (unsigned int i = 0; i < base::size(version_infos); i++) {
    if (version_infos[i].version == version)
      return &version_infos[i];
  }
  NOTREACHED() << "No version info found for v" << version;
  return nullptr;
}

// Static assertions for constraints for commonly-used versions.
static_assert(version_infos[0].num_segments != 0 &&
                  version_infos[0].total_bytes %
                          version_infos[0].num_segments ==
                      0,
              "Invalid configuration, version_infos[0]");

static_assert(
    version_infos[1].total_bytes ==
        version_infos[1].group_bytes + version_infos[1].group_bytes_1,
    "Invalid configuration, version_infos[1]. Groups don't sum to total.");
static_assert(version_infos[1].group_bytes == version_infos[1].segment_bytes() *
                                                  version_infos[1].num_segments,
              "Invalid configuration, version_infos[1], group 0.");
static_assert(version_infos[1].group_bytes_1 ==
                  version_infos[1].segment_bytes_1() *
                      version_infos[1].num_segments_1,
              "Invalid configuration, version_infos[1], group 1.");

QRCodeGenerator::QRCodeGenerator() = default;

QRCodeGenerator::~QRCodeGenerator() = default;

QRCodeGenerator::GeneratedCode::GeneratedCode() = default;
QRCodeGenerator::GeneratedCode::GeneratedCode(
    QRCodeGenerator::GeneratedCode&&) = default;
QRCodeGenerator::GeneratedCode::~GeneratedCode() = default;

base::Optional<GeneratedCode> QRCodeGenerator::Generate(
    base::span<const uint8_t> in) {
  // We're currently using a minimal set of versions to shrink test surface.
  // When expanding, take care to validate across different platforms and
  // a selection of QR Scanner apps.
  const QRVersionInfo* version_info =
      (in.size() <= kLargeVersionThresholdLength)
          ? GetVersionInfo(kVersionDefault)
          : GetVersionInfo(kVersionExtended);
  if (version_info != version_info_) {
    version_info_ = version_info;
    d_ = std::make_unique<uint8_t[]>(version_info_->total_size());
  }
  // Previous data and "set" bits must be cleared.
  memset(d_.get(), 0, version_info_->total_size());

  // Input data is too long for any supported code.
  if (in.size() > version_info->input_bytes()) {
    return base::nullopt;
  }

  PutVerticalTiming(6);
  PutHorizontalTiming(6);
  PutFinder(3, 3);
  PutFinder(3, version_info_->size - 4);
  PutFinder(version_info_->size - 4, 3);

  // See table E.1 for the location of alignment symbols.
  if (version_info_->version == kVersionDefault) {
    PutAlignment(30, 30);
  } else if (version_info_->version == kVersionExtended) {
    constexpr int kLocatorIndicesV7[] = {6, 22, 38};
    constexpr int kLocatorIndicesV13[] = {6, 34, 62};
    // Constant for now; may differ in higher versions.
    constexpr int num_locator_coefficients = 3;
    const int* locator_indices = nullptr;
    switch (version_info_->version) {
      case 7:
        locator_indices = kLocatorIndicesV7;
        break;
      case 13:
        locator_indices = kLocatorIndicesV13;
        break;
      default:
        NOTREACHED() << "No Locator Indices found for v"
                     << version_info_->version;
        break;
    }

    int first_index = locator_indices[0];
    int last_index = locator_indices[num_locator_coefficients - 1];

    for (int i = 0; i < num_locator_coefficients; i++) {
      for (int j = 0; j < num_locator_coefficients; j++) {
        int row = locator_indices[i];
        int col = locator_indices[j];
        // Aligntment symbols must not overwrite locators.
        if ((row == first_index && (col == first_index || col == last_index)) ||
            (row == last_index && col == first_index)) {
          continue;
        }
        PutAlignment(row, col);
      }
    }
  }

  // kFormatInformation is the encoded formatting word for the QR code that
  // this code generates. See tables 10 and 12.
  //                  00 011
  //                  --|---
  // error correction M | Mask pattern 3
  //
  // It's translated into the following, 15-bit value using the table on page
  // 80.
  constexpr uint16_t kFormatInformation = 0x5b4b;
  PutFormatBits(kFormatInformation);

  // Add the mode and character count.

  // QR codes require some framing of the data. This requires:
  // Version 1-9:   4 bits for mode + 8 bits for char count = 12 bits
  // Version 10-26: 4 bits for mode + 16 bits for char count = 20 bits
  // Details are in Table 3.
  // Since 12 and 20 are not a multiple of eight, a frame-shift of all
  // subsequent bytes is required.
  size_t data_bytes = version_info_->data_bytes();
  uint8_t prefixed_data[data_bytes];
  int framing_offset_bytes = 0;
  if (version_info->version <= 9) {
    DCHECK_LT(in.size(), 256u) << "in.size() too large for 8-bit length";
    const uint8_t len8 = static_cast<uint8_t>(in.size());
    prefixed_data[0] = 0x40 | (len8 >> 4);
    prefixed_data[1] = (len8 << 4);
    if (!in.empty()) {
      prefixed_data[1] |= (in[0] >> 4);
    }
    framing_offset_bytes = 2;
  } else if (version_info->version <= 26) {
    DCHECK_LT(in.size(), 0x10000u) << "in.size() too large for 16-bit length";
    const uint16_t len16 = static_cast<uint16_t>(in.size());
    prefixed_data[0] = 0x40 | (len16 >> 12);
    prefixed_data[1] = (len16 >> 4);
    prefixed_data[2] = (len16 << 4);
    if (!in.empty()) {
      prefixed_data[2] |= (in[0] >> 4);
    }
    framing_offset_bytes = 3;
  } else {
    NOTREACHED() << "Unsupported version in Generate(): "
                 << version_info->version;
  }

  for (size_t i = 0; i < in.size() - 1; i++) {
    prefixed_data[i + framing_offset_bytes] = (in[i] << 4) | (in[i + 1] >> 4);
  }
  if (!in.empty()) {
    prefixed_data[in.size() + 1] = in[in.size() + 1 - framing_offset_bytes]
                                   << 4;
  }

  // The QR code looks a little odd with fixed padding. Thus replicate the
  // message to fill the input.
  for (size_t i = in.size() + framing_offset_bytes;
       i < version_info_->input_bytes(); i++) {
    prefixed_data[i] = prefixed_data[i % (in.size() + framing_offset_bytes)];
  }

  // Each segment of input data is expanded with error correcting
  // information and then interleaved.

  // Error Correction for Group 0, present for all versions.
  size_t num_segments = version_info_->num_segments;
  size_t segment_bytes = version_info_->segment_bytes();
  size_t segment_ec_bytes = version_info_->segment_ec_bytes();
  uint8_t expanded_segments[num_segments][segment_bytes];
  for (size_t i = 0; i < num_segments; i++) {
    AddErrorCorrection(&expanded_segments[i][0],
                       &prefixed_data[version_info_->segment_data_bytes * i],
                       segment_bytes, segment_ec_bytes);
  }

  // Error Correction for Group 1, present for some versions.
  // Factor out the number of bytes written by the prior group.
  size_t num_segments_1 = version_info_->num_segments_1;
  size_t segment_bytes_1 = version_info_->segment_bytes_1();
  // TODO(skare): Reenable when extendiong to v13.
  // Additionally do not use a zero-length array; nonstandard.
  /*
  int group_data_offset = version_info_->segment_data_bytes * num_segments;
  size_t segment_ec_bytes_1 = version_info_->segment_ec_bytes_1();
  uint8_t expanded_segments_1[num_segments_1][segment_bytes_1];
  if (version_info_->num_segments_1 > 0) {
    for (size_t i = 0; i < num_segments_1; i++) {
      AddErrorCorrection(
          &expanded_segments_1[i][0],
          &prefixed_data[group_data_offset +
                         version_info_->segment_data_bytes_1 * i],
          segment_bytes_1, segment_ec_bytes_1);
    }
  }
  */

  size_t total_bytes = version_info_->total_bytes;
  uint8_t interleaved_data[total_bytes];
  CHECK(total_bytes ==
        segment_bytes * num_segments + segment_bytes_1 * num_segments_1)
      << "internal error";

  size_t k = 0;
  // Interleave data from all segments.
  // If we have multiple groups, the later groups may have more bytes in their
  // segments after we exhaust data in the first group.
  // TODO(skare): Extend when enabling v13.
  for (size_t j = 0; j < segment_bytes; j++) {
    for (size_t i = 0; i < num_segments; i++) {
      interleaved_data[k++] = expanded_segments[i][j];
    }
  }

  // The mask pattern is fixed for this implementation. A full implementation
  // would generate QR codes with every mask pattern and evaluate a quality
  // score, ultimately picking the optimal pattern. Here it's assumed that a
  // different QR code will soon be generated so any random issues will be
  // transient.
  PutBits(interleaved_data, sizeof(interleaved_data), MaskFunction3);

  GeneratedCode code;
  code.data = base::span<uint8_t>(d_.get(), version_info_->total_size());
  code.qr_size = version_info_->size;
  return code;
}

// MaskFunction3 implements one of the data-masking functions. See figure 21.
// static
uint8_t QRCodeGenerator::MaskFunction3(int x, int y) {
  return (x + y) % 3 == 0;
}

// PutFinder paints a finder symbol at the given coordinates.
void QRCodeGenerator::PutFinder(int x, int y) {
  DCHECK_GE(x, 3);
  DCHECK_GE(y, 3);
  fillAt(x - 3, y - 3, 7, 0b11);
  fillAt(x - 2, y - 2, 5, 0b10);
  fillAt(x - 2, y + 2, 5, 0b10);
  fillAt(x - 3, y + 3, 7, 0b11);

  static constexpr uint8_t kLine[7] = {0b11, 0b10, 0b11, 0b11,
                                       0b11, 0b10, 0b11};
  copyTo(x - 3, y - 1, kLine, sizeof(kLine));
  copyTo(x - 3, y, kLine, sizeof(kLine));
  copyTo(x - 3, y + 1, kLine, sizeof(kLine));

  at(x - 3, y - 2) = 0b11;
  at(x + 3, y - 2) = 0b11;
  at(x - 3, y + 2) = 0b11;
  at(x + 3, y + 2) = 0b11;

  for (int xx = x - 4; xx <= x + 4; xx++) {
    clipped(xx, y - 4) = 0b10;
    clipped(xx, y + 4) = 0b10;
  }
  for (int yy = y - 3; yy <= y + 3; yy++) {
    clipped(x - 4, yy) = 0b10;
    clipped(x + 4, yy) = 0b10;
  }
}

// PutAlignment paints an alignment symbol centered at the given coordinates.
void QRCodeGenerator::PutAlignment(int x, int y) {
  fillAt(x - 2, y - 2, 5, 0b11);
  fillAt(x - 2, y + 2, 5, 0b11);
  static constexpr uint8_t kLine[5] = {0b11, 0b10, 0b10, 0b10, 0b11};
  copyTo(x - 2, y - 1, kLine, sizeof(kLine));
  copyTo(x - 2, y, kLine, sizeof(kLine));
  copyTo(x - 2, y + 1, kLine, sizeof(kLine));
  at(x, y) = 0b11;
}

// PutVerticalTiming paints the vertical timing signal.
void QRCodeGenerator::PutVerticalTiming(int x) {
  for (int y = 0; y < version_info_->size; y++) {
    at(x, y) = 0b10 | (1 ^ (y & 1));
  }
}

// PutVerticalTiming paints the horizontal timing signal.
void QRCodeGenerator::PutHorizontalTiming(int y) {
  for (int x = 0; x < version_info_->size; x++) {
    at(x, y) = 0b10 | (1 ^ (x & 1));
  }
}

// PutFormatBits paints the 15-bit, pre-encoded format metadata. See page 56
// for the location of the format bits.
void QRCodeGenerator::PutFormatBits(const uint16_t format) {
  // kRun1 is the location of the initial format bits, where the upper nibble
  // is the x coordinate and the lower the y.
  static constexpr uint8_t kRun1[15] = {
      0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x87, 0x88,
      0x78, 0x58, 0x48, 0x38, 0x28, 0x18, 0x08,
  };

  uint16_t v = format;
  for (size_t i = 0; i < sizeof(kRun1); i++) {
    const uint8_t location = kRun1[i];
    at(location >> 4, location & 15) = 0b10 | (v & 1);
    v >>= 1;
  }

  v = format;
  for (int x = version_info_->size - 1; x >= version_info_->size - 1 - 7; x--) {
    at(x, 8) = 0b10 | (v & 1);
    v >>= 1;
  }

  at(8, version_info_->size - 1 - 7) = 0b11;
  for (int y = version_info_->size - 1 - 6; y <= version_info_->size - 1; y++) {
    at(8, y) = 0b10 | (v & 1);
    v >>= 1;
  }

  // Version 7 and larger require 18-bit version information taking the form
  // of 6x3 rectangles above the bottom-left locator and to the left of the
  // top-right locator.
  int size = version_info_->size;
  int version = version_info_->version;
  int vi_string = 0;
  switch (version) {
    case 5:
      break;
    case 7:
      vi_string = 0b000111110010010100;
      break;
    case 13:
      vi_string = 0b001101100001000111;
      break;
    default:
      NOTREACHED() << "No version information string provided for QR v"
                   << version;
      break;
  }
  if (vi_string) {
    for (int i = 0; i < 6; i++) {
      for (int j = 0; j < 3; j++) {
        // Bottom-left rectangle is top-to-bottom, left-to-right
        at(i, size - 8 - 3 + j) = 0b10 | (vi_string & 1);
        // Top-right rectangle is left-to-right, top-to-bottom
        at(size - 8 - 3 + j, i) = 0b10 | (vi_string & 1);
        // Shift to consider the next bit.
        vi_string >>= 1;
      }
    }
  }
}

// PutBits writes the given data into the QR code in correct order, avoiding
// structural elements that must have already been painted. See section 7.7.3
// about the placement algorithm.
void QRCodeGenerator::PutBits(const uint8_t* data,
                              size_t data_len,
                              uint8_t (*mask_func)(int, int)) {
  // BitStream vends bits from |data| on demand, in the order that QR codes
  // expect them.
  class BitStream {
   public:
    BitStream(const uint8_t* data, size_t data_len)
        : data_(data), data_len_(data_len), i_(0), bits_in_current_byte_(0) {}

    uint8_t Next() {
      if (bits_in_current_byte_ == 0) {
        if (i_ >= data_len_) {
          byte_ = 0;
        } else {
          byte_ = data_[i_++];
        }
        bits_in_current_byte_ = 8;
      }

      const uint8_t ret = byte_ >> 7;
      byte_ <<= 1;
      bits_in_current_byte_--;
      return ret;
    }

   private:
    const uint8_t* const data_;
    const size_t data_len_;
    size_t i_;
    unsigned bits_in_current_byte_;
    uint8_t byte_;
  };

  BitStream stream(data, data_len);

  bool going_up = true;
  int x = version_info_->size - 1;
  int y = version_info_->size - 1;

  for (;;) {
    uint8_t& right = at(x, y);
    // Test the current value in the QR code to avoid painting over any
    // existing structural elements.
    if (right == 0) {
      right = stream.Next() ^ mask_func(x, y);
    }

    uint8_t& left = at(x - 1, y);
    if (left == 0) {
      left = stream.Next() ^ mask_func(x - 1, y);
    }

    if ((going_up && y == 0) || (!going_up && y == version_info_->size - 1)) {
      if (x == 1) {
        break;
      }
      x -= 2;
      // The vertical timing column is skipped over.
      if (x == 6) {
        x--;
      }
      going_up = !going_up;
    } else {
      if (going_up) {
        y--;
      } else {
        y++;
      }
    }
  }
}

// at returns a reference to the given element of |d_|.
uint8_t& QRCodeGenerator::at(int x, int y) {
  DCHECK_LE(0, x);
  DCHECK_LT(x, version_info_->size);
  DCHECK_LE(0, y);
  DCHECK_LT(y, version_info_->size);
  return d_[version_info_->size * y + x];
}

// fillAt sets the |len| elements at (x, y) to |value|.
void QRCodeGenerator::fillAt(int x, int y, size_t len, uint8_t value) {
  DCHECK_LE(0, x);
  DCHECK_LE(static_cast<int>(x + len), version_info_->size);
  DCHECK_LE(0, y);
  DCHECK_LT(y, version_info_->size);
  memset(&d_[version_info_->size * y + x], value, len);
}

// copyTo copies |len| elements from |data| to the elements at (x, y).
void QRCodeGenerator::copyTo(int x, int y, const uint8_t* data, size_t len) {
  DCHECK_LE(0, x);
  DCHECK_LE(static_cast<int>(x + len), version_info_->size);
  DCHECK_LE(0, y);
  DCHECK_LT(y, version_info_->size);
  memcpy(&d_[version_info_->size * y + x], data, len);
}

// clipped returns a reference to the given element of |d_|, or to
// |clip_dump_| if the coordinates are out of bounds.
uint8_t& QRCodeGenerator::clipped(int x, int y) {
  if (0 <= x && x < version_info_->size && 0 <= y && y < version_info_->size) {
    return d_[version_info_->size * y + x];
  }
  return clip_dump_;
}

// GF28Mul returns the product of |a| and |b| (which must be field elements,
// i.e. < 256) in the field GF(2^8) mod x^8 + x^4 + x^3 + x^2 + 1.
// static
uint8_t QRCodeGenerator::GF28Mul(uint16_t a, uint16_t b) {
  uint16_t acc = 0;

  // Perform 8-bit, carry-less multiplication of |a| and |b|.
  for (int i = 0; i < 8; i++) {
    const uint16_t mask = ~((b & 1) - 1);
    acc ^= a & mask;
    b >>= 1;
    a <<= 1;
  }

  // Add multiples of the modulus to eliminate all bits past a byte. Note that
  // the bits in |modulus| have a one where there's a non-zero power of |x| in
  // the field modulus.
  uint16_t modulus = 0b100011101 << 7;
  for (int i = 15; i >= 8; i--) {
    const uint16_t mask = ~((acc >> i) - 1);
    acc ^= modulus & mask;
    modulus >>= 1;
  }

  return acc;
}

// AddErrorCorrection writes the Reed-Solomon expanded version of |in| to
// |out|.
// |out| should have length segment_bytes for the code's version.
// |in| should have length segment_data_bytes for the code's version.
void QRCodeGenerator::AddErrorCorrection(uint8_t out[],
                                         const uint8_t in[],
                                         size_t segment_bytes,
                                         size_t segment_ec_bytes) {
  // kGenerator is the product of (z - x^i) for 0 <= i < |segment_ec_bytes|,
  // where x is the term of GF(2^8) and z is the term of a polynomial ring
  // over GF(2^8). It's generated with the following Sage script:
  //
  // F.<x> = GF(2^8, modulus = x^8 + x^4 + x^3 + x^2 + 1)
  // R.<z> = PolynomialRing(F, 'z')
  //
  // def toByte(p):
  //     return sum([(1<<i) * int(term) for (i, term) in
  //     enumerate(p.polynomial())])
  //
  // def generatorPoly(n):
  //    acc = (z - F(1))
  //    for i in range(1,n):
  //        acc *= (z - x^i)
  //    return acc
  //
  // gen = generatorPoly(24)
  // coeffs = list(gen)
  // gen = [toByte(x) for x in coeffs]
  // print 'uint8_t kGenerator[' + str(len(gen)) + '] = {' + str(gen) + '}'

  // Used for 7-M: 18 error correction codewords per block.
  static std::vector<uint8_t> kGenerator18 = {
      146, 217, 67,  32,  75,  173, 82,  73,  220, 240,
      215, 199, 175, 149, 113, 183, 251, 239, 1,
  };

  // Used for 13-M; 22 error correction codewords per block.
  static std::vector<uint8_t> kGenerator22 = {
      245, 145, 26,  230, 218, 86,  253, 67,  123, 29, 137, 28,
      40,  69,  189, 19,  244, 182, 176, 131, 179, 89, 1,
  };

  // Used for 5-M, 24 error correction codewords per block.
  static std::vector<uint8_t> kGenerator24 = {
      117, 144, 217, 127, 247, 237, 1,   206, 43,  61,  72,  130, 73,
      229, 150, 115, 102, 216, 237, 178, 70,  169, 118, 122, 1,
  };

  const std::vector<uint8_t>* generator = nullptr;
  switch (segment_ec_bytes) {
    case 18:
      generator = &kGenerator18;
      break;
    case 22:
      generator = &kGenerator22;
      break;
    case 24:
      generator = &kGenerator24;
      break;
    default: {
      NOTREACHED() << "Unsupported Generator Polynomial for segment_ec_bytes: "
                   << segment_ec_bytes;
      return;
    }
  }

  // The error-correction bytes are the remainder of dividing |in| * x^k by
  // |kGenerator|, where |k| is the number of EC codewords. Polynomials here
  // are represented in little-endian order, i.e. the value at index |i| is
  // the coefficient of z^i.

  // Multiplication of |in| by x^k thus just involves moving it up.
  std::unique_ptr<uint8_t[]> remainder(new uint8_t[segment_bytes]);
  memset(remainder.get(), 0, segment_ec_bytes);
  size_t segment_data_bytes = segment_bytes - segment_ec_bytes;
  // Reed-Solomon input is backwards. See section 7.5.2.
  for (size_t i = 0; i < segment_data_bytes; i++) {
    remainder[segment_ec_bytes + i] = in[segment_data_bytes - 1 - i];
  }

  // Progressively eliminate the leading coefficient by subtracting some
  // multiple of |generator| until we have a value smaller than |generator|.
  for (size_t i = segment_bytes - 1; i >= segment_ec_bytes; i--) {
    // The leading coefficient of |generator| is 1, so the multiple to
    // subtract to eliminate the leading term of |remainder| is the value of
    // that leading term. The polynomial ring is characteristic two, so
    // subtraction is the same as addition, which is XOR.
    for (size_t j = 0; j < generator->size() - 1; j++) {
      remainder[i - segment_ec_bytes + j] ^=
          GF28Mul(generator->at(j), remainder[i]);
    }
  }

  memmove(out, in, segment_data_bytes);
  // Remove the Reed-Solomon remainder again to match QR's convention.
  for (size_t i = 0; i < segment_ec_bytes; i++) {
    out[segment_data_bytes + i] = remainder[segment_ec_bytes - 1 - i];
  }
}
