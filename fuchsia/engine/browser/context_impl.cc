// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/context_impl.h"

#include <lib/zx/channel.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/common.h"

ContextImpl::ContextImpl(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      cookie_manager_(base::BindRepeating(
          &content::StoragePartition::GetNetworkContext,
          base::Unretained(content::BrowserContext::GetDefaultStoragePartition(
              browser_context_)))) {}

ContextImpl::~ContextImpl() = default;

void ContextImpl::DestroyFrame(FrameImpl* frame) {
  DCHECK(frames_.find(frame) != frames_.end());
  frames_.erase(frames_.find(frame));
}

bool ContextImpl::IsJavaScriptInjectionAllowed() {
  return allow_javascript_injection_;
}

void ContextImpl::OnDebugDevToolsPortReady() {
  web_engine_remote_debugging_.OnDebugDevToolsPortReady();
}

void ContextImpl::CreateFrame(
    fidl::InterfaceRequest<fuchsia::web::Frame> frame) {
  content::WebContents::CreateParams create_params(browser_context_, nullptr);
  create_params.initially_hidden = true;
  auto web_contents = content::WebContents::Create(create_params);

  frames_.insert(std::make_unique<FrameImpl>(std::move(web_contents), this,
                                             std::move(frame)));
}

void ContextImpl::GetCookieManager(
    fidl::InterfaceRequest<fuchsia::web::CookieManager> request) {
  cookie_manager_bindings_.AddBinding(&cookie_manager_, std::move(request));
}

void ContextImpl::GetRemoteDebuggingPort(
    GetRemoteDebuggingPortCallback callback) {
  web_engine_remote_debugging_.GetRemoteDebuggingPort(std::move(callback));
}

FrameImpl* ContextImpl::GetFrameImplForTest(fuchsia::web::FramePtr* frame_ptr) {
  DCHECK(frame_ptr);

  // Find the FrameImpl whose channel is connected to |frame_ptr| by inspecting
  // the related_koids of active FrameImpls.
  zx_info_handle_basic_t handle_info;
  zx_status_t status = frame_ptr->channel().get_info(
      ZX_INFO_HANDLE_BASIC, &handle_info, sizeof(zx_info_handle_basic_t),
      nullptr, nullptr);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_get_info";
  zx_handle_t client_handle_koid = handle_info.koid;

  for (const std::unique_ptr<FrameImpl>& frame : frames_) {
    status = frame->GetBindingChannelForTest()->get_info(
        ZX_INFO_HANDLE_BASIC, &handle_info, sizeof(zx_info_handle_basic_t),
        nullptr, nullptr);
    ZX_CHECK(status == ZX_OK, status) << "zx_object_get_info";

    if (client_handle_koid == handle_info.related_koid)
      return frame.get();
  }

  return nullptr;
}
