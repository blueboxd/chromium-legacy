// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/gvr_install_helper.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/android/vr/android_vr_utils.h"

using base::android::AttachCurrentThread;

namespace vr {

GvrInstallHelper::GvrInstallHelper() : XrInstallHelper() {}

GvrInstallHelper::~GvrInstallHelper() {
  RunInstallFinishedCallback(false);
}

void GvrInstallHelper::EnsureInstalled(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> install_callback) {
  DVLOG(1) << __func__;
  // Callers should ensure they only prompt for install once.
  DCHECK(!install_finished_callback_);

  install_finished_callback_ = std::move(install_callback);

  // By ensuring that there is no outstanding callback, we also ensure that we
  // don't care about any value that may currently be in module_delegate_.
  // Unfortunately, the only place we have to null this value out (so that we
  // could also DCHECK on this being null), is OnModuleInstalled. Turns out,
  // it's a pretty bad idea to delete the object that's currently processing
  // your callback, as it may still have outstanding work it wants to do.
  module_delegate_ = VrModuleProviderFactory::CreateModuleProvider(
      render_process_id, render_frame_id);

  // If we failed to get the module delegate, then report failure to install.
  if (!module_delegate_) {
    RunInstallFinishedCallback(false);
    return;
  }

  // If the module is already installed, then skip to the next step, asserting
  // that install succeeded.
  if (module_delegate_->ModuleInstalled()) {
    OnModuleInstalled(true);
    return;
  }

  // Prompt for module installation.
  module_delegate_->InstallModule(base::BindOnce(
      &GvrInstallHelper::OnModuleInstalled, weak_ptr_.GetWeakPtr()));
}

void GvrInstallHelper::OnModuleInstalled(bool success) {
  if (!success) {
    // If we failed to install the DFM, then abort the flow.
    RunInstallFinishedCallback(false);
    return;
  }

  // TODO(crbug.com/1037935): Currently when spinning up VrShellDelegate will
  // detect that VrCore is not installed and prompt for it to be installed.
  // However, since we don't know about it here, this causes the session to get
  // rejected when the prompt comes up. It would be better if we could do this
  // installation here.
  RunInstallFinishedCallback(success);
}

void GvrInstallHelper::RunInstallFinishedCallback(bool success) {
  if (install_finished_callback_)
    std::move(install_finished_callback_).Run(success);
}

}  // namespace vr
