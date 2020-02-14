// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/lifecycle/aw_contents_lifecycle_notifier.h"

#include <utility>

#include "android_webview/browser_jni_headers/AwContentsLifecycleNotifier_jni.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;
using content::BrowserThread;

namespace android_webview {

namespace {

AwContentsLifecycleNotifier::AwContentsState CalcuateState(
    bool is_atached_to_window,
    bool is_window_visible) {
  // Can't assume the sequence of Attached, Detached, Visible, Invisible event
  // because the app could changed it; Calculate the state here.
  if (is_atached_to_window) {
    return is_window_visible
               ? AwContentsLifecycleNotifier::AwContentsState::kForeground
               : AwContentsLifecycleNotifier::AwContentsState::kBackground;
  }
  return AwContentsLifecycleNotifier::AwContentsState::kDetached;
}

}  // namespace

AwContentsLifecycleNotifier::AwContentsData::AwContentsData() = default;

AwContentsLifecycleNotifier::AwContentsData::AwContentsData(
    AwContentsData&& data) = default;

AwContentsLifecycleNotifier::AwContentsData::~AwContentsData() = default;

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
  DCHECK(aw_contents_id_to_data_.find(id) == aw_contents_id_to_data_.end());

  aw_contents_id_to_data_.emplace(id, AwContentsData());
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
  const auto it = aw_contents_id_to_data_.find(id);
  DCHECK(it != aw_contents_id_to_data_.end());

  state_count_[ToIndex(it->second.aw_content_state)]--;
  DCHECK(state_count_[ToIndex(it->second.aw_content_state)] >= 0);
  aw_contents_id_to_data_.erase(it);
  UpdateAppState();

  if (!HasAwContentsInstance()) {
    Java_AwContentsLifecycleNotifier_onLastWebViewDestroyed(
        AttachCurrentThread());
  }
}

void AwContentsLifecycleNotifier::OnWebViewAttachedToWindow(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* data = GetAwContentsData(aw_contents);
  data->attached_to_window = true;
  OnAwContentsStateChanged(data);
}

void AwContentsLifecycleNotifier::OnWebViewDetachedFromWindow(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* data = GetAwContentsData(aw_contents);
  data->attached_to_window = false;
  DCHECK(data->aw_content_state != AwContentsState::kDetached);
  OnAwContentsStateChanged(data);
}

void AwContentsLifecycleNotifier::OnWebViewWindowBeVisible(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* data = GetAwContentsData(aw_contents);
  data->window_visible = true;
  OnAwContentsStateChanged(data);
}

void AwContentsLifecycleNotifier::OnWebViewWindowBeInvisible(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* data = GetAwContentsData(aw_contents);
  data->window_visible = false;
  OnAwContentsStateChanged(data);
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
    AwContentsLifecycleNotifier::AwContentsData* data) {
  AwContentsLifecycleNotifier::AwContentsState state =
      CalcuateState(data->attached_to_window, data->window_visible);
  if (data->aw_content_state == state)
    return;
  state_count_[ToIndex(data->aw_content_state)]--;
  DCHECK(state_count_[ToIndex(data->aw_content_state)] >= 0);
  state_count_[ToIndex(state)]++;
  data->aw_content_state = state;
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

AwContentsLifecycleNotifier::AwContentsData*
AwContentsLifecycleNotifier::GetAwContentsData(const AwContents* aw_contents) {
  uint64_t id = reinterpret_cast<uint64_t>(aw_contents);
  DCHECK(aw_contents_id_to_data_.find(id) != aw_contents_id_to_data_.end());
  return &aw_contents_id_to_data_.at(id);
}

}  // namespace android_webview
