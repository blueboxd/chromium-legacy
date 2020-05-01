// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <map>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class DrmUtilTest : public testing::Test {};

TEST_F(DrmUtilTest, TestDisplayModesExtraction) {
  // Initialize a list of display modes.
  constexpr size_t kNumModes = 5;
  drmModeModeInfo modes[kNumModes] = {
      {0,
       640 /* hdisplay */,
       0,
       0,
       0,
       0,
       400 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
      {0,
       640 /* hdisplay */,
       0,
       0,
       0,
       0,
       480 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
      {0,
       800 /* hdisplay */,
       0,
       0,
       0,
       0,
       600 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
      {0,
       1024 /* hdisplay */,
       0,
       0,
       0,
       0,
       768 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
      {0,
       1280 /* hdisplay */,
       0,
       0,
       0,
       0,
       768 /* vdisplay */,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       {}},
  };
  drmModeModeInfoPtr modes_ptr = static_cast<drmModeModeInfoPtr>(
      drmMalloc(kNumModes * sizeof(drmModeModeInfo)));
  std::memcpy(modes_ptr, &modes[0], kNumModes * sizeof(drmModeModeInfo));

  // Initialize a connector.
  drmModeConnector connector = {
      0,
      0,
      0,
      0,
      DRM_MODE_CONNECTED,
      0,
      0,
      DRM_MODE_SUBPIXEL_UNKNOWN,
      5 /* count_modes */,
      modes_ptr,
      0,
      nullptr,
      nullptr,
      0,
      nullptr,
  };
  drmModeConnector* connector_ptr =
      static_cast<drmModeConnector*>(drmMalloc(sizeof(drmModeConnector)));
  *connector_ptr = connector;

  // Initialize a CRTC.
  drmModeCrtc crtc = {
      0, 0, 0, 0, 0, 0, 1 /* mode_valid */, modes[0], 0,
  };
  drmModeCrtcPtr crtc_ptr =
      static_cast<drmModeCrtcPtr>(drmMalloc(sizeof(drmModeCrtc)));
  *crtc_ptr = crtc;

  HardwareDisplayControllerInfo info(ScopedDrmConnectorPtr(connector_ptr),
                                     ScopedDrmCrtcPtr(crtc_ptr), 0);

  const display::DisplayMode* current_mode;
  const display::DisplayMode* native_mode;
  auto extracted_modes =
      ExtractDisplayModes(&info, gfx::Size(), &current_mode, &native_mode);

  // With no preferred mode and no active pixel size, the native mode will be
  // selected as the first mode.
  ASSERT_EQ(5u, extracted_modes.size());
  EXPECT_EQ(extracted_modes[0].get(), current_mode);
  EXPECT_EQ(extracted_modes[0].get(), native_mode);
  EXPECT_EQ(gfx::Size(640, 400), native_mode->size());

  // With no preferred mode, but with an active pixel size, the native mode will
  // be the mode that has the same size as the active pixel size.
  const gfx::Size active_pixel_size(1280, 768);
  extracted_modes = ExtractDisplayModes(&info, active_pixel_size, &current_mode,
                                        &native_mode);
  ASSERT_EQ(5u, extracted_modes.size());
  EXPECT_EQ(extracted_modes[0].get(), current_mode);
  EXPECT_EQ(extracted_modes[4].get(), native_mode);
  EXPECT_EQ(active_pixel_size, native_mode->size());

  // The preferred mode is always returned as the native mode, even when a valid
  // active pixel size supplied.
  modes_ptr[2].type |= DRM_MODE_TYPE_PREFERRED;
  extracted_modes = ExtractDisplayModes(&info, active_pixel_size, &current_mode,
                                        &native_mode);
  ASSERT_EQ(5u, extracted_modes.size());
  EXPECT_EQ(extracted_modes[0].get(), current_mode);
  EXPECT_EQ(extracted_modes[2].get(), native_mode);
  EXPECT_EQ(gfx::Size(800, 600), native_mode->size());

  // While KMS specification says there should be at most one preferred mode per
  // connector, we found monitors with more than one preferred mode. With this
  // test we make sure the first one is the one used for native_mode.
  modes_ptr[1].type |= DRM_MODE_TYPE_PREFERRED;
  extracted_modes = ExtractDisplayModes(&info, active_pixel_size, &current_mode,
                                        &native_mode);
  ASSERT_EQ(5u, extracted_modes.size());
  EXPECT_EQ(extracted_modes[1].get(), native_mode);
}

}  // namespace ui
