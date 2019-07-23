// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ozone_platform.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

namespace {

bool g_platform_initialized_ui = false;
bool g_platform_initialized_gpu = false;

OzonePlatform* g_instance = nullptr;

}  // namespace

OzonePlatform::OzonePlatform() {
  DCHECK(!g_instance) << "There should only be a single OzonePlatform.";
  g_instance = this;
}

OzonePlatform::~OzonePlatform() = default;

// static
void OzonePlatform::InitializeForUI(const InitParams& args) {
  EnsureInstance();
  if (g_platform_initialized_ui)
    return;
  g_platform_initialized_ui = true;
  g_instance->InitializeUI(args);
  // This is deliberately created after initializing so that the platform can
  // create its own version of DDM.
  DeviceDataManager::CreateInstance();
}

// static
void OzonePlatform::InitializeForGPU(const InitParams& args) {
  EnsureInstance();
  if (g_platform_initialized_gpu)
    return;
  g_platform_initialized_gpu = true;
  g_instance->InitializeGPU(args);
}

// static
OzonePlatform* OzonePlatform::GetInstance() {
  DCHECK(g_instance) << "OzonePlatform is not initialized";
  return g_instance;
}

// static
OzonePlatform* OzonePlatform::EnsureInstance() {
  if (!g_instance) {
    TRACE_EVENT1("ozone",
                 "OzonePlatform::Initialize",
                 "platform",
                 GetOzonePlatformName());
    std::unique_ptr<OzonePlatform> platform =
        PlatformObject<OzonePlatform>::Create();

    // TODO(spang): Currently need to leak this object.
    OzonePlatform* pl = platform.release();
    DCHECK_EQ(g_instance, pl);
  }
  return g_instance;
}

IPC::MessageFilter* OzonePlatform::GetGpuMessageFilter() {
  return nullptr;
}

std::unique_ptr<PlatformScreen> OzonePlatform::CreateScreen() {
  return nullptr;
}

PlatformClipboard* OzonePlatform::GetPlatformClipboard() {
  // Platforms that support system clipboard must override this method.
  return nullptr;
}

bool OzonePlatform::IsNativePixmapConfigSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) const {
  // Platform that support NativePixmap must override this method.
  return false;
}

const OzonePlatform::PlatformProperties&
OzonePlatform::GetPlatformProperties() {
  static const base::NoDestructor<OzonePlatform::PlatformProperties> properties;
  return *properties;
}

const OzonePlatform::InitializedHostProperties&
OzonePlatform::GetInitializedHostProperties() {
  DCHECK(g_platform_initialized_ui);

  static InitializedHostProperties host_properties;
  return host_properties;
}

void OzonePlatform::AddInterfaces(service_manager::BinderRegistry* registry) {}

void OzonePlatform::AfterSandboxEntry() {}

// static
bool OzonePlatform::has_initialized_ui() {
  return g_platform_initialized_ui;
}

}  // namespace ui
