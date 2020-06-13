// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <utility>

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

using ::testing::_;
using ::testing::SizeIs;

// Verifies that the argument goes from 0 to the maximum uint16_t times |scale|.
MATCHER_P(MatchesLinearRamp, scale, "") {
  EXPECT_FALSE(arg.empty());

  EXPECT_EQ(arg.front().r, 0);
  EXPECT_EQ(arg.front().g, 0);
  EXPECT_EQ(arg.front().b, 0);

  const uint16_t max_value = std::numeric_limits<uint16_t>::max() * scale;

  const auto middle_element = arg[arg.size() / 2];
  const uint16_t middle_value = max_value * (arg.size() / 2) / (arg.size() - 1);
  EXPECT_NEAR(middle_element.r, middle_value, 1);
  EXPECT_NEAR(middle_element.g, middle_value, 1);
  EXPECT_NEAR(middle_element.b, middle_value, 1);

  const uint16_t last_value = max_value;
  EXPECT_EQ(arg.back().r, last_value);
  EXPECT_EQ(arg.back().g, last_value);
  EXPECT_EQ(arg.back().b, last_value);

  return true;
}

namespace ui {

namespace {

class MockHardwareDisplayPlaneManager : public HardwareDisplayPlaneManager {
 public:
  explicit MockHardwareDisplayPlaneManager(DrmDevice* drm)
      : HardwareDisplayPlaneManager(drm) {}
  ~MockHardwareDisplayPlaneManager() override = default;

  MOCK_METHOD3(SetGammaCorrection,
               bool(uint32_t crtc_id,
                    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
                    const std::vector<display::GammaRampRGBEntry>& gamma_lut));

  bool Modeset(uint32_t crtc_id,
               uint32_t framebuffer_id,
               uint32_t connector_id,
               const drmModeModeInfo& mode,
               const HardwareDisplayPlaneList& plane_list) override {
    return false;
  }
  bool DisableModeset(uint32_t crtc_id, uint32_t connector) override {
    return false;
  }
  bool Commit(HardwareDisplayPlaneList* plane_list,
              scoped_refptr<PageFlipRequest> page_flip_request,
              std::unique_ptr<gfx::GpuFence>* out_fence) override {
    return false;
  }
  bool DisableOverlayPlanes(HardwareDisplayPlaneList* plane_list) override {
    return false;
  }
  bool SetColorCorrectionOnAllCrtcPlanes(
      uint32_t crtc_id,
      ScopedDrmColorCtmPtr ctm_blob_data) override {
    return false;
  }
  bool ValidatePrimarySize(const DrmOverlayPlane& primary,
                           const drmModeModeInfo& mode) override {
    return false;
  }
  void RequestPlanesReadyCallback(
      DrmOverlayPlaneList planes,
      base::OnceCallback<void(DrmOverlayPlaneList planes)> callback) override {
    return;
  }
  bool InitializePlanes() override { return false; }
  bool SetPlaneData(HardwareDisplayPlaneList* plane_list,
                    HardwareDisplayPlane* hw_plane,
                    const DrmOverlayPlane& overlay,
                    uint32_t crtc_id,
                    const gfx::Rect& src_rect) override {
    return false;
  }
  std::unique_ptr<HardwareDisplayPlane> CreatePlane(
      uint32_t plane_id) override {
    return nullptr;
  }
  bool IsCompatible(HardwareDisplayPlane* plane,
                    const DrmOverlayPlane& overlay,
                    uint32_t crtc_index) const override {
    return false;
  }
  bool CommitColorMatrix(const CrtcProperties& crtc_props) override {
    return false;
  }
  bool CommitGammaCorrection(const CrtcProperties& crtc_props) override {
    return false;
  }
};

}  // namespace

class DrmDisplayTest : public testing::Test {
 protected:
  DrmDisplayTest()
      : mock_drm_device_(base::MakeRefCounted<MockDrmDevice>(
            std::make_unique<MockGbmDevice>())),
        drm_display_(&screen_manager_, mock_drm_device_) {}

  MockHardwareDisplayPlaneManager* AddMockHardwareDisplayPlaneManager() {
    auto mock_hardware_display_plane_manager =
        std::make_unique<MockHardwareDisplayPlaneManager>(
            mock_drm_device_.get());
    MockHardwareDisplayPlaneManager* pointer =
        mock_hardware_display_plane_manager.get();
    mock_drm_device_->plane_manager_ =
        std::move(mock_hardware_display_plane_manager);
    return pointer;
  }

  base::test::TaskEnvironment env_;
  scoped_refptr<DrmDevice> mock_drm_device_;
  ScreenManager screen_manager_;
  DrmDisplay drm_display_;
};

TEST_F(DrmDisplayTest, SetColorSpace) {
  drm_display_.set_is_hdr_capable_for_testing(true);
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  ON_CALL(*plane_manager, SetGammaCorrection(_, SizeIs(0), _))
      .WillByDefault(::testing::Return(true));

  const auto kHDRColorSpace = gfx::ColorSpace::CreateHDR10();
  EXPECT_CALL(*plane_manager, SetGammaCorrection(_, SizeIs(0), SizeIs(0)));
  drm_display_.SetColorSpace(kHDRColorSpace);

  const auto kSDRColorSpace = gfx::ColorSpace::CreateREC709();
  constexpr float kHDRLevel = 2.0;
  EXPECT_CALL(
      *plane_manager,
      SetGammaCorrection(_, SizeIs(0), MatchesLinearRamp(1.0 / kHDRLevel)));
  drm_display_.SetColorSpace(kSDRColorSpace);
}

TEST_F(DrmDisplayTest, SetEmptyGammaCorrectionNonHDRDisplay) {
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  ON_CALL(*plane_manager, SetGammaCorrection(_, _, _))
      .WillByDefault(::testing::Return(true));

  EXPECT_CALL(*plane_manager, SetGammaCorrection(_, SizeIs(0), SizeIs(0)));
  drm_display_.SetGammaCorrection(std::vector<display::GammaRampRGBEntry>(),
                                  std::vector<display::GammaRampRGBEntry>());
}

TEST_F(DrmDisplayTest, SetEmptyGammaCorrectionHDRDisplay) {
  drm_display_.set_is_hdr_capable_for_testing(true);
  MockHardwareDisplayPlaneManager* plane_manager =
      AddMockHardwareDisplayPlaneManager();

  ON_CALL(*plane_manager, SetGammaCorrection(_, _, _))
      .WillByDefault(::testing::Return(true));

  constexpr float kHDRLevel = 2.0;
  EXPECT_CALL(
      *plane_manager,
      SetGammaCorrection(_, SizeIs(0), MatchesLinearRamp(1.0 / kHDRLevel)));
  drm_display_.SetGammaCorrection(std::vector<display::GammaRampRGBEntry>(),
                                  std::vector<display::GammaRampRGBEntry>());
}

}  // namespace ui
