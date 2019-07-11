// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_CONTEXT_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_CONTEXT_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "fuchsia/engine/browser/web_engine_remote_debugging.h"
#include "fuchsia/engine/web_engine_export.h"

namespace content {
class BrowserContext;
}  // namespace content

class FrameImpl;

// Implementation of Context from fuchsia.web.
// Owns a BrowserContext instance and uses it to create new WebContents/Frames.
// All created Frames are owned by this object.
class WEB_ENGINE_EXPORT ContextImpl : public fuchsia::web::Context {
 public:
  // |browser_context| must outlive ContextImpl.
  explicit ContextImpl(content::BrowserContext* browser_context);

  // Tears down the Context, destroying any active Frames in the process.
  ~ContextImpl() override;

  content::BrowserContext* browser_context_for_test() {
    return browser_context_;
  }

  // Removes and destroys the specified |frame|.
  void DestroyFrame(FrameImpl* frame);

  // Returns |true| if JS injection was enabled for this Context.
  bool IsJavaScriptInjectionAllowed();

  // Called by Frames to signal a document has been loaded and signal to the
  // debug listeners in |web_engine_remote_debugging_| that they can now
  // successfully connect ChromeDriver.
  void OnDebugDevToolsPortReady();

  // fuchsia::web::Context implementation.
  void CreateFrame(fidl::InterfaceRequest<fuchsia::web::Frame> frame) override;
  void GetRemoteDebuggingPort(GetRemoteDebuggingPortCallback callback) override;

  // Gets the underlying FrameImpl service object associated with a connected
  // |frame_ptr| client.
  FrameImpl* GetFrameImplForTest(fuchsia::web::FramePtr* frame_ptr);

 private:
  content::BrowserContext* const browser_context_;

  // TODO(crbug.com/893236): Make this false by default, and allow it to be
  // initialized at Context creation time.
  bool allow_javascript_injection_ = true;

  // Tracks all active FrameImpl instances, so that we can request their
  // destruction when this ContextImpl is destroyed.
  std::set<std::unique_ptr<FrameImpl>, base::UniquePtrComparator> frames_;

  WebEngineRemoteDebugging web_engine_remote_debugging_;

  DISALLOW_COPY_AND_ASSIGN(ContextImpl);
};

#endif  // FUCHSIA_ENGINE_BROWSER_CONTEXT_IMPL_H_
