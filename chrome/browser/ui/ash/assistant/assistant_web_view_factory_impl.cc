// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_web_view_factory_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/assistant/assistant_web_view_impl.h"

AssistantWebViewFactoryImpl::AssistantWebViewFactoryImpl(Profile* profile)
    : profile_(profile) {}

AssistantWebViewFactoryImpl::~AssistantWebViewFactoryImpl() = default;

std::unique_ptr<ash::AssistantWebView> AssistantWebViewFactoryImpl::Create(
    const ash::AssistantWebView::InitParams& params) {
  return std::make_unique<AssistantWebViewImpl>(profile_, params);
}
