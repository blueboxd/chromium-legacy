// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "content/browser/compositor/reflector_impl.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    scoped_refptr<viz::ContextProvider> context_provider,
    std::unique_ptr<viz::OverlayCandidateValidator> overlay_candidate_validator)
    : OutputSurface(std::move(context_provider)) {
  overlay_candidate_validator_ = std::move(overlay_candidate_validator);
}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    std::unique_ptr<viz::SoftwareOutputDevice> software_device)
    : OutputSurface(std::move(software_device)) {}

BrowserCompositorOutputSurface::~BrowserCompositorOutputSurface() {
  if (reflector_)
    reflector_->DetachFromOutputSurface();
  DCHECK(!reflector_);
}

void BrowserCompositorOutputSurface::SetReflector(ReflectorImpl* reflector) {
  reflector_ = reflector;

  OnReflectorChanged();
}

void BrowserCompositorOutputSurface::OnReflectorChanged() {
}

std::unique_ptr<viz::OverlayCandidateValidator>
BrowserCompositorOutputSurface::TakeOverlayCandidateValidator() {
  return std::move(overlay_candidate_validator_);
}

bool BrowserCompositorOutputSurface::HasExternalStencilTest() const {
  return false;
}

void BrowserCompositorOutputSurface::ApplyExternalStencil() {}

void BrowserCompositorOutputSurface::SetUpdateVSyncParametersCallback(
    viz::UpdateVSyncParametersCallback callback) {
  update_vsync_parameters_callback_ = std::move(callback);
}

gfx::OverlayTransform BrowserCompositorOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

}  // namespace content
