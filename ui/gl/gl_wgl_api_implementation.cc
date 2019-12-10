// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_wgl_api_implementation.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_implementation_wrapper.h"
#include "ui/gl/gl_surface_wgl.h"

namespace gl {

RealWGLApi* g_real_wgl = nullptr;

GL_IMPL_WRAPPER_TYPE(WGL) * g_wgl_wrapper = nullptr;

void InitializeStaticGLBindingsWGL() {
  g_driver_wgl.InitializeStaticBindings();
  if (!g_wgl_wrapper) {
    auto real_api = std::make_unique<RealWGLApi>();
    real_api->Initialize(&g_driver_wgl);
    g_wgl_wrapper = new GL_IMPL_WRAPPER_TYPE(WGL)(std::move(real_api));
  }

  g_current_wgl_context = g_wgl_wrapper->api();
}

void ClearBindingsWGL() {
  delete g_wgl_wrapper;
  g_wgl_wrapper = nullptr;

  g_current_wgl_context = nullptr;
  g_driver_wgl.ClearBindings();
}

WGLApi::WGLApi() {
}

WGLApi::~WGLApi() {
}

WGLApiBase::WGLApiBase() : driver_(nullptr) {}

WGLApiBase::~WGLApiBase() {
}

void WGLApiBase::InitializeBase(DriverWGL* driver) {
  driver_ = driver;
}

RealWGLApi::RealWGLApi() {
}

RealWGLApi::~RealWGLApi() {
}

void RealWGLApi::Initialize(DriverWGL* driver) {
  InitializeBase(driver);
}

void RealWGLApi::SetDisabledExtensions(const std::string& disabled_extensions) {
  disabled_exts_.clear();
  filtered_ext_exts_ = "";
  filtered_arb_exts_ = "";
  if (!disabled_extensions.empty()) {
    disabled_exts_ =
        base::SplitString(disabled_extensions, ", ;",
                          base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
  }
}

const char* RealWGLApi::wglGetExtensionsStringARBFn(HDC hDC) {
  if (filtered_arb_exts_.size())
    return filtered_arb_exts_.c_str();

  if (!driver_->fn.wglGetExtensionsStringARBFn)
    return nullptr;

  const char* str = WGLApiBase::wglGetExtensionsStringARBFn(hDC);
  if (!str)
    return nullptr;

  filtered_arb_exts_ = FilterGLExtensionList(str, disabled_exts_);
  return filtered_arb_exts_.c_str();
}

const char* RealWGLApi::wglGetExtensionsStringEXTFn() {
  if (filtered_ext_exts_.size())
    return filtered_ext_exts_.c_str();

  if (!driver_->fn.wglGetExtensionsStringEXTFn)
    return nullptr;

  const char* str = WGLApiBase::wglGetExtensionsStringEXTFn();
  if (!str)
    return nullptr;

  filtered_ext_exts_ = FilterGLExtensionList(str, disabled_exts_);
  return filtered_ext_exts_.c_str();
}

LogWGLApi::LogWGLApi(WGLApi* wgl_api) : wgl_api_(wgl_api) {}

LogWGLApi::~LogWGLApi() {}

void LogWGLApi::SetDisabledExtensions(const std::string& disabled_extensions) {
  if (wgl_api_) {
    wgl_api_->SetDisabledExtensions(disabled_extensions);
  }
}

TraceWGLApi::~TraceWGLApi() {
}

void TraceWGLApi::SetDisabledExtensions(
    const std::string& disabled_extensions) {
  if (wgl_api_) {
    wgl_api_->SetDisabledExtensions(disabled_extensions);
  }
}

bool GetGLWindowSystemBindingInfoWGL(GLWindowSystemBindingInfo* info) {
  const char* extensions = wglGetExtensionsStringEXT();
  *info = GLWindowSystemBindingInfo();
  if (extensions)
    info->extensions = extensions;
  return true;
}

void SetDisabledExtensionsWGL(const std::string& disabled_extensions) {
  DCHECK(g_current_wgl_context);
  DCHECK(GLContext::TotalGLContexts() == 0);
  g_current_wgl_context->SetDisabledExtensions(disabled_extensions);
}

bool InitializeExtensionSettingsOneOffWGL() {
  return GLSurfaceWGL::InitializeExtensionSettingsOneOff();
}

}  // namespace gl
