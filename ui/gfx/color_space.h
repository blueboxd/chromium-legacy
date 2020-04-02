// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_H_
#define UI_GFX_COLOR_SPACE_H_

#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/color_space_export.h"

// These forward declarations are used to give IPC code friend access to private
// fields of gfx::ColorSpace for the purpose of serialization and
// deserialization.
namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace mojo {
template <class T, class U>
struct StructTraits;
}  // namespace mojo

// Used to serialize a gfx::ColorSpace through the GPU command buffer.
struct _GLcolorSpace;

namespace gfx {

enum class ContentColorUsage : uint8_t;

namespace mojom {
class ColorSpaceDataView;
}  // namespace mojom

// Used to represet a color space for the purpose of color conversion.
// This is designed to be safe and compact enough to send over IPC
// between any processes.
class COLOR_SPACE_EXPORT ColorSpace {
 public:
  enum class PrimaryID : uint8_t {
    INVALID,
    BT709,
    BT470M,
    BT470BG,
    SMPTE170M,
    SMPTE240M,
    FILM,
    BT2020,
    SMPTEST428_1,
    SMPTEST431_2,
    SMPTEST432_1,
    XYZ_D50,
    ADOBE_RGB,
    // Corresponds the the primaries of the "Generic RGB" profile used in the
    // Apple ColorSync application, used by layout tests on Mac.
    APPLE_GENERIC_RGB,
    // A very wide gamut space with rotated primaries. Used by layout tests.
    WIDE_GAMUT_COLOR_SPIN,
    // Primaries defined by the primary matrix |custom_primary_matrix_|.
    CUSTOM,
    kMaxValue = CUSTOM,
  };

  enum class TransferID : uint8_t {
    INVALID,
    BT709,
    // On macOS, BT709 hardware decoded video frames, when displayed as
    // overlays, will have a transfer function of gamma=1.961.
    BT709_APPLE,
    GAMMA18,
    GAMMA22,
    GAMMA24,
    GAMMA28,
    SMPTE170M,
    SMPTE240M,
    LINEAR,
    LOG,
    LOG_SQRT,
    IEC61966_2_4,
    BT1361_ECG,
    IEC61966_2_1,
    BT2020_10,
    BT2020_12,
    SMPTEST2084,
    SMPTEST428_1,
    ARIB_STD_B67,  // AKA hybrid-log gamma, HLG.
    // The same as IEC61966_2_1 on the interval [0, 1], with the nonlinear
    // segment continuing beyond 1 and point symmetry defining values below 0.
    IEC61966_2_1_HDR,
    // The same as LINEAR but is defined for all real values.
    LINEAR_HDR,
    // A parametric transfer function defined by |transfer_params_|.
    CUSTOM,
    // An HDR parametric transfer function defined by |transfer_params_|.
    CUSTOM_HDR,
    // An HDR transfer function that is piecewise sRGB, and piecewise linear.
    PIECEWISE_HDR,
    kMaxValue = PIECEWISE_HDR,
  };

  enum class MatrixID : uint8_t {
    INVALID,
    RGB,
    BT709,
    FCC,
    BT470BG,
    SMPTE170M,
    SMPTE240M,
    YCOCG,
    BT2020_NCL,
    BT2020_CL,
    YDZDX,
    GBR,
    kMaxValue = GBR,
  };

  enum class RangeID : uint8_t {
    INVALID,
    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    LIMITED,
    // Full RGB color range with RGB valees from 0 to 255.
    FULL,
    // Range is defined by TransferID/MatrixID.
    DERIVED,
    kMaxValue = DERIVED,
  };

  constexpr ColorSpace() {}
  constexpr ColorSpace(PrimaryID primaries, TransferID transfer)
      : ColorSpace(primaries, transfer, MatrixID::RGB, RangeID::FULL) {}
  constexpr ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range)
      : primaries_(primaries),
        transfer_(transfer),
        matrix_(matrix),
        range_(range) {}
  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID range,
             const skcms_Matrix3x3* custom_primary_matrix,
             const skcms_TransferFunction* cunstom_transfer_fn);

  explicit ColorSpace(const SkColorSpace& sk_color_space);

  // Returns true if this is not the default-constructor object.
  bool IsValid() const;

  static constexpr ColorSpace CreateSRGB() {
    return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1, MatrixID::RGB,
                      RangeID::FULL);
  }

  static constexpr ColorSpace CreateDisplayP3D65() {
    return ColorSpace(PrimaryID::SMPTEST432_1, TransferID::IEC61966_2_1,
                      MatrixID::RGB, RangeID::FULL);
  }
  static ColorSpace CreateCustom(const skcms_Matrix3x3& to_XYZD50,
                                 const skcms_TransferFunction& fn);
  static ColorSpace CreateCustom(const skcms_Matrix3x3& to_XYZD50,
                                 TransferID transfer);
  static constexpr ColorSpace CreateXYZD50() {
    return ColorSpace(PrimaryID::XYZ_D50, TransferID::LINEAR, MatrixID::RGB,
                      RangeID::FULL);
  }

  // Extended sRGB matches sRGB for values in [0, 1], and extends the transfer
  // function to all real values.
  static constexpr ColorSpace CreateExtendedSRGB() {
    return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1_HDR,
                      MatrixID::RGB, RangeID::FULL);
  }

  // scRGB uses the same primaries as sRGB but has a linear transfer function
  // for all real values. The slope of the transfer function may be specified
  // by |slope|.
  static ColorSpace CreateSCRGBLinear(float slope = 1.f);

  // HDR10 uses BT.2020 primaries with SMPTE ST 2084 PQ transfer function.
  static ColorSpace CreateHDR10(float sdr_white_point = 0.f);

  // Create a piecewise-HDR color space.
  // - If |primaries| is CUSTOM, then |custom_primary_matrix| must be
  //   non-nullptr.
  // - The SDR joint is the encoded pixel value where the SDR portion reaches 1,
  //   usually 0.25 or 0.5, corresponding to giving 8 or 9 of 10 bits to SDR.
  //   This must be in the open interval (0, 1).
  // - The HDR level the value that the transfer function will evaluate to at 1,
  //   and represents the maximum HDR brightness relative to the maximum SDR
  //   brightness. This must be strictly greater than 1.
  static ColorSpace CreatePiecewiseHDR(
      PrimaryID primaries,
      float sdr_joint,
      float hdr_level,
      const skcms_Matrix3x3* custom_primary_matrix = nullptr);

  // TODO(ccameron): Remove these, and replace with more generic constructors.
  static constexpr ColorSpace CreateJpeg() {
    // TODO(ccameron): Determine which primaries and transfer function were
    // intended here.
    return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1,
                      MatrixID::SMPTE170M, RangeID::FULL);
  }
  static constexpr ColorSpace CreateREC601() {
    return ColorSpace(PrimaryID::SMPTE170M, TransferID::SMPTE170M,
                      MatrixID::SMPTE170M, RangeID::LIMITED);
  }
  static constexpr ColorSpace CreateREC709() {
    return ColorSpace(PrimaryID::BT709, TransferID::BT709, MatrixID::BT709,
                      RangeID::LIMITED);
  }

  // On macOS and on ChromeOS, sRGB's (1,1,1) always coincides with PQ's 100
  // nits (which may not be 100 physical nits). Life is more complicated on
  // Windows.
  static constexpr float kDefaultSDRWhiteLevel = 100.f;

  bool operator==(const ColorSpace& other) const;
  bool operator!=(const ColorSpace& other) const;
  bool operator<(const ColorSpace& other) const;
  size_t GetHash() const;
  std::string ToString() const;

  bool IsWide() const;

  // Returns true if the transfer function is an HDR one (SMPTE 2084, HLG, etc).
  bool IsHDR() const;

  // Returns true if the encoded values can be outside of the 0.0-1.0 range.
  bool FullRangeEncodedValues() const;

  // Returns the color space's content color usage category (sRGB, WCG, or HDR).
  ContentColorUsage GetContentColorUsage() const;

  // Return this color space with any YUV to RGB conversion stripped off.
  ColorSpace GetAsRGB() const;

  // Return this color space with any range adjust or YUV to RGB conversion
  // stripped off.
  ColorSpace GetAsFullRangeRGB() const;

  // Return a color space where all values are bigger/smaller by the given
  // factor. If you convert colors from SRGB to SRGB.GetScaledColorSpace(2.0)
  // everything will be half as bright in linear lumens.
  ColorSpace GetScaledColorSpace(float factor) const;

  // If |this| is the final output color space, return the color space that
  // would be appropriate for rasterization.
  ColorSpace GetRasterColorSpace() const;

  // Return true if blending in |this| is close enough to blending in sRGB to
  // be considered acceptable (only PQ and nearly-linear transfer functions
  // return false).
  bool IsSuitableForBlending() const;

  // Return a combined color space with has the same primary and transfer than
  // the caller but replacing the matrix and range with the given values.
  ColorSpace GetWithMatrixAndRange(MatrixID matrix, RangeID range) const;

  // If this color space has a PQ transfer function that did not specify an
  // SDR white level, then return |this| with its SDR white level set to
  // |sdr_white_level|. Otherwise return |this| unmodified.
  ColorSpace GetWithPQSDRWhiteLevel(float sdr_white_level) const;

  // This will return nullptr for non-RGB spaces, spaces with non-FULL
  // range, and unspecified spaces.
  sk_sp<SkColorSpace> ToSkColorSpace() const;

  // Return a GLcolorSpace value that is valid for the lifetime of |this|. This
  // function is used to serialize ColorSpace objects across the GPU command
  // buffer.
  const _GLcolorSpace* AsGLColorSpace() const;

  // For YUV color spaces, return the closest SkYUVColorSpace.
  // Returns true if a close match is found.
  bool ToSkYUVColorSpace(SkYUVColorSpace* out) const;

  void GetPrimaryMatrix(skcms_Matrix3x3* to_XYZD50) const;
  void GetPrimaryMatrix(SkMatrix44* to_XYZD50) const;
  bool GetTransferFunction(skcms_TransferFunction* fn) const;
  bool GetInverseTransferFunction(skcms_TransferFunction* fn) const;

  // Returns the SDR white level specified for the PQ transfer function. If
  // no value was specified, then use kDefaultSDRWhiteLevel. If the transfer
  // function is not PQ then return false.
  bool GetPQSDRWhiteLevel(float* sdr_white_level) const;

  // Returns the parameters for a PIECEWISE_HDR transfer function. See
  // CreatePiecewiseHDR for parameter meanings.
  bool GetPiecewiseHDRParams(float* sdr_point, float* hdr_level) const;

  // For most formats, this is the RGB to YUV matrix.
  void GetTransferMatrix(SkMatrix44* matrix) const;
  void GetRangeAdjustMatrix(SkMatrix44* matrix) const;

  // Returns the current primary ID.
  // Note: if SetCustomPrimaries() has been used, the primary ID returned
  // may have been set to PrimaryID::CUSTOM, or been coerced to another
  // PrimaryID if it was very close.
  PrimaryID GetPrimaryID() const;

  // Returns the current transfer ID.
  TransferID GetTransferID() const;

  // Returns the current matrix ID.
  MatrixID GetMatrixID() const;

  // Returns the current range ID.
  RangeID GetRangeID() const;

  // Returns true if the transfer function is defined by an
  // skcms_TransferFunction which is extended to all real values.
  bool HasExtendedSkTransferFn() const;

 private:
  static void GetPrimaryMatrix(PrimaryID, skcms_Matrix3x3* to_XYZD50);
  static bool GetTransferFunction(TransferID, skcms_TransferFunction* fn);
  static size_t TransferParamCount(TransferID);

  void GetPQTransferFunction(skcms_TransferFunction* fn) const;

  void SetCustomTransferFunction(const skcms_TransferFunction& fn);
  void SetCustomPrimaries(const skcms_Matrix3x3& to_XYZD50);

  PrimaryID primaries_ = PrimaryID::INVALID;
  TransferID transfer_ = TransferID::INVALID;
  MatrixID matrix_ = MatrixID::INVALID;
  RangeID range_ = RangeID::INVALID;

  // Only used if primaries_ is PrimaryID::CUSTOM.
  float custom_primary_matrix_[9] = {0, 0, 0, 0, 0, 0, 0, 0};

  // Parameters for the transfer function. The interpretation depends on
  // |transfer_|. Only TransferParamCount() of these parameters are used, all
  // others must be zero.
  // - CUSTOM and CUSTOM_HDR: Entries A through G of the skcms_TransferFunction
  //   structure in alphabetical order.
  // - SMPTEST2084: SDR white point.
  float transfer_params_[7] = {0, 0, 0, 0, 0, 0, 0};

  friend struct IPC::ParamTraits<gfx::ColorSpace>;
  friend struct mojo::StructTraits<gfx::mojom::ColorSpaceDataView,
                                   gfx::ColorSpace>;
};

// Stream operator so ColorSpace can be used in assertion statements.
COLOR_SPACE_EXPORT std::ostream& operator<<(std::ostream& out,
                                            const ColorSpace& color_space);

}  // namespace gfx

#endif  // UI_GFX_COLOR_SPACE_H_
