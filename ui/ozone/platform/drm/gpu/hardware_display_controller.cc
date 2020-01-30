// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

#include <drm.h>
#include <string.h>
#include <xf86drm.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/libdrm/src/include/drm/drm_fourcc.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

namespace {

void CompletePageFlip(
    base::WeakPtr<HardwareDisplayController> hardware_display_controller_,
    PresentationOnceCallback callback,
    DrmOverlayPlaneList plane_list,
    const gfx::PresentationFeedback& presentation_feedback) {
  if (hardware_display_controller_) {
    hardware_display_controller_->OnPageFlipComplete(std::move(plane_list),
                                                     presentation_feedback);
  }
  std::move(callback).Run(presentation_feedback);
}

void DrawCursor(DrmDumbBuffer* cursor, const SkBitmap& image) {
  SkRect damage;
  image.getBounds(&damage);

  // Clear to transparent in case |image| is smaller than the canvas.
  SkCanvas* canvas = cursor->GetCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->drawBitmapRect(image, damage, NULL);
}

}  // namespace

HardwareDisplayController::HardwareDisplayController(
    std::unique_ptr<CrtcController> controller,
    const gfx::Point& origin)
    : origin_(origin), is_disabled_(controller->is_disabled()) {
  AddCrtc(std::move(controller));
  AllocateCursorBuffers();
}

HardwareDisplayController::~HardwareDisplayController() = default;

bool HardwareDisplayController::Modeset(const DrmOverlayPlane& primary,
                                        const drmModeModeInfo& mode) {
  TRACE_EVENT0("drm", "HDC::Modeset");
  return ModesetCrtc(primary, /*use_current_crtc_mode=*/false, mode);
}

bool HardwareDisplayController::Enable(const DrmOverlayPlane& primary) {
  TRACE_EVENT0("drm", "HDC::Enable");
  drmModeModeInfo empty_mode = {};
  return ModesetCrtc(primary, /*use_current_crtc_mode=*/true, empty_mode);
}

bool HardwareDisplayController::ModesetCrtc(const DrmOverlayPlane& primary,
                                            bool use_current_crtc_mode,
                                            const drmModeModeInfo& mode) {
  DCHECK(primary.buffer.get());
  bool status = true;

  HardwareDisplayPlaneList hardware_planes = {};
  hardware_planes.old_plane_list = GetAllOwnedPlanes();
  GetDrmDevice()->plane_manager()->BeginFrame(hardware_planes);

  DrmOverlayPlaneList plane_list = {};
  plane_list.push_back(primary.Clone());
  for (const auto& controller : crtc_controllers_) {
    status &= controller->AssignOverlayPlanes(&hardware_planes, plane_list,
                                              /*is_modesetting=*/true);
    drmModeModeInfo modeset_mode =
        use_current_crtc_mode ? controller->mode() : mode;
    status &= controller->Modeset(primary, modeset_mode, hardware_planes);
  }

  is_disabled_ = false;
  ResetCursor();
  OnModesetComplete(primary);
  return status;
}

void HardwareDisplayController::Disable() {
  TRACE_EVENT0("drm", "HDC::Disable");

  for (const auto& controller : crtc_controllers_)
    // TODO(crbug.com/1015104): Modeset and Disable operations should go
    // together. The current split is due to how the legacy/atomic split
    // evolved. It should be cleaned up under the more generic
    // HardwareDisplayPlaneManager{Legacy,Atomic} calls.
    controller->Disable();

  bool ret = GetDrmDevice()->plane_manager()->DisableOverlayPlanes(
      GetAllOwnedPlanes());
  LOG_IF(ERROR, !ret) << "Can't disable overlays when disabling HDC.";

  is_disabled_ = true;
}

void HardwareDisplayController::SchedulePageFlip(
    DrmOverlayPlaneList plane_list,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback) {
  DCHECK(!page_flip_request_);
  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(GetRefreshInterval());
  std::unique_ptr<gfx::GpuFence> out_fence;

  bool status =
      ScheduleOrTestPageFlip(plane_list, page_flip_request, &out_fence);
  CHECK(status) << "SchedulePageFlip failed";

  if (page_flip_request->page_flip_count() == 0) {
    // Apparently, there was nothing to do. This probably should not be
    // able to happen but both CrtcController::AssignOverlayPlanes and
    // HardwareDisplayPlaneManagerLegacy::Commit appear to have cases
    // where we ACK without actually scheduling a page flip.
    std::move(submission_callback).Run(gfx::SwapResult::SWAP_ACK, nullptr);
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  std::move(submission_callback)
      .Run(gfx::SwapResult::SWAP_ACK, std::move(out_fence));

  // Everything was submitted successfully, wait for asynchronous completion.
  page_flip_request->TakeCallback(
      base::BindOnce(&CompletePageFlip, weak_ptr_factory_.GetWeakPtr(),
                     std::move(presentation_callback), std::move(plane_list)));
  page_flip_request_ = std::move(page_flip_request);
}

bool HardwareDisplayController::TestPageFlip(
    const DrmOverlayPlaneList& plane_list) {
  return ScheduleOrTestPageFlip(plane_list, nullptr, nullptr);
}

bool HardwareDisplayController::ScheduleOrTestPageFlip(
    const DrmOverlayPlaneList& plane_list,
    scoped_refptr<PageFlipRequest> page_flip_request,
    std::unique_ptr<gfx::GpuFence>* out_fence) {
  TRACE_EVENT0("drm", "HDC::SchedulePageFlip");
  DCHECK(!is_disabled_);

  // Ignore requests with no planes to schedule.
  if (plane_list.empty())
    return true;

  DrmOverlayPlaneList pending_planes = DrmOverlayPlane::Clone(plane_list);
  std::sort(pending_planes.begin(), pending_planes.end(),
            [](const DrmOverlayPlane& l, const DrmOverlayPlane& r) {
              return l.z_order < r.z_order;
            });

  HardwareDisplayPlaneList hardware_planes;
  hardware_planes.old_plane_list = GetAllOwnedPlanes();

  GetDrmDevice()->plane_manager()->BeginFrame(hardware_planes);

  bool status = true;
  for (const auto& controller : crtc_controllers_) {
    status &= controller->AssignOverlayPlanes(&hardware_planes, pending_planes,
                                              /*is_modesetting=*/false);
  }

  status &= GetDrmDevice()->plane_manager()->Commit(
      hardware_planes, /*modeset=*/false, page_flip_request, out_fence);

  return status;
}

std::vector<uint64_t> HardwareDisplayController::GetFormatModifiers(
    uint32_t format) const {
  std::vector<uint64_t> modifiers;

  if (crtc_controllers_.empty())
    return modifiers;

  modifiers = crtc_controllers_[0]->GetFormatModifiers(format);

  for (size_t i = 1; i < crtc_controllers_.size(); ++i) {
    std::vector<uint64_t> other =
        crtc_controllers_[i]->GetFormatModifiers(format);
    std::vector<uint64_t> intersection;

    std::set_intersection(modifiers.begin(), modifiers.end(), other.begin(),
                          other.end(), std::back_inserter(intersection));
    modifiers = std::move(intersection);
  }

  return modifiers;
}

std::vector<uint64_t>
HardwareDisplayController::GetFormatModifiersForModesetting(
    uint32_t fourcc_format) const {
  const auto& modifiers = GetFormatModifiers(fourcc_format);
  std::vector<uint64_t> filtered_modifiers;
  for (auto modifier : modifiers) {
    // AFBC for modeset buffers doesn't work correctly, as we can't fill it with
    // a valid AFBC buffer. For now, don't use AFBC for modeset buffers.
    // TODO: Use AFBC for modeset buffers if it is available.
    // See https://crbug.com/852675.
    if (modifier != DRM_FORMAT_MOD_CHROMEOS_ROCKCHIP_AFBC) {
      filtered_modifiers.push_back(modifier);
    }
  }
  return filtered_modifiers;
}

void HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  cursor_location_ = location;
  UpdateCursorLocation();
}

void HardwareDisplayController::SetCursor(SkBitmap bitmap) {
  if (bitmap.drawsNothing()) {
    current_cursor_ = nullptr;
  } else {
    current_cursor_ = NextCursorBuffer();
    DrawCursor(current_cursor_, bitmap);
  }

  UpdateCursorImage();
}

void HardwareDisplayController::AddCrtc(
    std::unique_ptr<CrtcController> controller) {
  DCHECK(crtc_controllers_.empty() || controller->drm() == GetDrmDevice());
  crtc_controllers_.push_back(std::move(controller));
}

std::unique_ptr<CrtcController> HardwareDisplayController::RemoveCrtc(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc) {
  auto controller_it = std::find_if(
      crtc_controllers_.begin(), crtc_controllers_.end(),
      [drm, crtc](const std::unique_ptr<CrtcController>& crtc_controller) {
        return crtc_controller->drm() == drm && crtc_controller->crtc() == crtc;
      });
  if (controller_it == crtc_controllers_.end())
    return nullptr;

  std::unique_ptr<CrtcController> controller(std::move(*controller_it));
  crtc_controllers_.erase(controller_it);

  std::vector<HardwareDisplayPlane*> planes =
      drm->plane_manager()->GetOwnedPlanesForCrtcs({crtc});
  drm->plane_manager()->DisableOverlayPlanes(planes);

  return controller;
}

bool HardwareDisplayController::HasCrtc(const scoped_refptr<DrmDevice>& drm,
                                        uint32_t crtc) const {
  for (const auto& controller : crtc_controllers_) {
    if (controller->drm() == drm && controller->crtc() == crtc)
      return true;
  }

  return false;
}

bool HardwareDisplayController::IsMirrored() const {
  return crtc_controllers_.size() > 1;
}

bool HardwareDisplayController::IsDisabled() const {
  return is_disabled_;
}

gfx::Size HardwareDisplayController::GetModeSize() const {
  // If there are multiple CRTCs they should all have the same size.
  return gfx::Size(crtc_controllers_[0]->mode().hdisplay,
                   crtc_controllers_[0]->mode().vdisplay);
}

base::TimeDelta HardwareDisplayController::GetRefreshInterval() const {
  // If there are multiple CRTCs they should all have the same refresh rate.
  float vrefresh = ModeRefreshRate(crtc_controllers_[0]->mode());
  return vrefresh ? base::TimeDelta::FromSeconds(1) / vrefresh
                  : base::TimeDelta();
}

base::TimeTicks HardwareDisplayController::GetTimeOfLastFlip() const {
  return time_of_last_flip_;
}

scoped_refptr<DrmDevice> HardwareDisplayController::GetDrmDevice() const {
  DCHECK(!crtc_controllers_.empty());
  // TODO(dnicoara) When we support mirroring across DRM devices, figure out
  // which device should be used for allocations.
  return crtc_controllers_[0]->drm();
}

void HardwareDisplayController::OnPageFlipComplete(
    DrmOverlayPlaneList pending_planes,
    const gfx::PresentationFeedback& presentation_feedback) {
  if (!page_flip_request_)
    return;  // Modeset occured during this page flip.
  time_of_last_flip_ = presentation_feedback.timestamp;
  current_planes_ = std::move(pending_planes);
  for (const auto& controller : crtc_controllers_)
    controller->OnPageFlipComplete();
  page_flip_request_ = nullptr;
}

void HardwareDisplayController::OnModesetComplete(
    const DrmOverlayPlane& primary) {
  // drmModeSetCrtc has an immediate effect, so we can assume that the current
  // planes have been updated. However if a page flip is still pending, set the
  // pending planes to the same values so that the callback keeps the correct
  // state.
  page_flip_request_ = nullptr;
  current_planes_.clear();
  current_planes_.push_back(primary.Clone());
  time_of_last_flip_ = base::TimeTicks::Now();
}

void HardwareDisplayController::AllocateCursorBuffers() {
  TRACE_EVENT0("drm", "HDC::AllocateCursorBuffers");
  gfx::Size max_cursor_size = GetMaximumCursorSize(GetDrmDevice()->get_fd());
  SkImageInfo info = SkImageInfo::MakeN32Premul(max_cursor_size.width(),
                                                max_cursor_size.height());
  for (size_t i = 0; i < base::size(cursor_buffers_); ++i) {
    cursor_buffers_[i] = std::make_unique<DrmDumbBuffer>(GetDrmDevice());
    // Don't register a framebuffer for cursors since they are special (they
    // aren't modesetting buffers and drivers may fail to register them due to
    // their small sizes).
    if (!cursor_buffers_[i]->Initialize(info)) {
      LOG(FATAL) << "Failed to initialize cursor buffer";
      return;
    }
  }
}

DrmDumbBuffer* HardwareDisplayController::NextCursorBuffer() {
  ++cursor_frontbuffer_;
  cursor_frontbuffer_ %= base::size(cursor_buffers_);
  return cursor_buffers_[cursor_frontbuffer_].get();
}

void HardwareDisplayController::UpdateCursorImage() {
  uint32_t handle = 0;
  gfx::Size size;

  if (current_cursor_) {
    handle = current_cursor_->GetHandle();
    size = current_cursor_->GetSize();
  }

  for (const auto& controller : crtc_controllers_)
    controller->SetCursor(handle, size);
}

void HardwareDisplayController::UpdateCursorLocation() {
  for (const auto& controller : crtc_controllers_)
    controller->MoveCursor(cursor_location_);
}

void HardwareDisplayController::ResetCursor() {
  UpdateCursorLocation();
  UpdateCursorImage();
}

std::vector<HardwareDisplayPlane*>
HardwareDisplayController::GetAllOwnedPlanes() {
  std::vector<uint32_t> crtcs;
  for (const auto& controller : crtc_controllers_)
    crtcs.push_back(controller->crtc());
  return GetDrmDevice()->plane_manager()->GetOwnedPlanesForCrtcs(crtcs);
}

}  // namespace ui
