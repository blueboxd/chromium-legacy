// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_lifecycle_notifier.h"

#include <utility>

#include "android_webview/browser_jni_headers/AwContentsLifecycleNotifier_jni.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;
using content::BrowserThread;

namespace android_webview {

// static
AwContentsLifecycleNotifier& AwContentsLifecycleNotifier::GetInstance() {
  static base::NoDestructor<AwContentsLifecycleNotifier> instance;
  return *instance;
}

void AwContentsLifecycleNotifier::OnWebViewCreated(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  has_aw_contents_ever_created_ = true;
  uint64_t id = reinterpret_cast<uint64_t>(aw_contents);
  bool first_created = !HasAwContentsInstance();
  DCHECK(aw_contents_id_to_state_.find(id) == aw_contents_id_to_state_.end());

  aw_contents_id_to_state_.insert(
      std::make_pair(id, AwContentsState::kDetached));
  state_count_[ToIndex(AwContentsState::kDetached)]++;
  UpdateAppState();

  if (first_created) {
    Java_AwContentsLifecycleNotifier_onFirstWebViewCreated(
        AttachCurrentThread());
  }
}

void AwContentsLifecycleNotifier::OnWebViewDestroyed(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  uint64_t id = reinterpret_cast<uint64_t>(aw_contents);
  const auto it = aw_contents_id_to_state_.find(id);
  DCHECK(it != aw_contents_id_to_state_.end());

  state_count_[ToIndex(it->second)]--;
  DCHECK(state_count_[ToIndex(it->second)] >= 0);
  aw_contents_id_to_state_.erase(it);
  UpdateAppState();

  if (!HasAwContentsInstance()) {
    Java_AwContentsLifecycleNotifier_onLastWebViewDestroyed(
        AttachCurrentThread());
  }
}

void AwContentsLifecycleNotifier::OnWebViewAttachedToWindow(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnAwContentsStateChanged(aw_contents, AwContentsState::kBackground);
}

void AwContentsLifecycleNotifier::OnWebViewDetachedFromWindow(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnAwContentsStateChanged(aw_contents, AwContentsState::kDetached);
}

void AwContentsLifecycleNotifier::OnWebViewWindowBeVisible(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnAwContentsStateChanged(aw_contents, AwContentsState::kForeground);
}

void AwContentsLifecycleNotifier::OnWebViewWindowBeInvisible(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnAwContentsStateChanged(aw_contents, AwContentsState::kBackground);
}

void AwContentsLifecycleNotifier::AddObserver(
    WebViewAppStateObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
  observer->OnAppStateChanged(app_state_);
}

void AwContentsLifecycleNotifier::RemoveObserver(
    WebViewAppStateObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

AwContentsLifecycleNotifier::AwContentsLifecycleNotifier() = default;

AwContentsLifecycleNotifier::~AwContentsLifecycleNotifier() = default;

size_t AwContentsLifecycleNotifier::ToIndex(AwContentsState state) const {
  size_t index = static_cast<size_t>(state);
  DCHECK(index < base::size(state_count_));
  return index;
}

void AwContentsLifecycleNotifier::OnAwContentsStateChanged(
    const AwContents* aw_contents,
    AwContentsState state) {
  uint64_t id = reinterpret_cast<uint64_t>(aw_contents);
  const auto it = aw_contents_id_to_state_.find(id);
  DCHECK(it != aw_contents_id_to_state_.end());
  DCHECK(it->second != state);
  state_count_[ToIndex(it->second)]--;
  DCHECK(state_count_[ToIndex(it->second)] >= 0);
  state_count_[ToIndex(state)]++;
  aw_contents_id_to_state_[it->first] = state;
  UpdateAppState();
}

void AwContentsLifecycleNotifier::UpdateAppState() {
  WebViewAppStateObserver::State state;
  if (state_count_[ToIndex(AwContentsState::kForeground)] > 0)
    state = WebViewAppStateObserver::State::kForeground;
  else if (state_count_[ToIndex(AwContentsState::kBackground)] > 0)
    state = WebViewAppStateObserver::State::kBackground;
  else if (state_count_[ToIndex(AwContentsState::kDetached)] > 0)
    state = WebViewAppStateObserver::State::kUnknown;
  else
    state = WebViewAppStateObserver::State::kDestroyed;
  if (state != app_state_) {
    app_state_ = state;
    for (auto& observer : observers_) {
      observer.OnAppStateChanged(app_state_);
    }
  }
}

bool AwContentsLifecycleNotifier::HasAwContentsInstance() const {
  for (size_t i = 0; i < base::size(state_count_); i++) {
    if (state_count_[i] > 0)
      return true;
  }
  return false;
}

}  // namespace android_webview
