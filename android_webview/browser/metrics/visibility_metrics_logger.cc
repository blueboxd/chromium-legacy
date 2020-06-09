// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/visibility_metrics_logger.h"

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using base::NoDestructor;
using content::BrowserThread;

namespace android_webview {

base::HistogramBase* VisibilityMetricsLogger::GetGlobalVisibilityHistogram() {
  static NoDestructor<base::HistogramBase*> histogram(
      base::Histogram::FactoryGet(
          "Android.WebView.Visibility.Global", 1,
          static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue) + 1,
          static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue) + 2,
          base::HistogramBase::kUmaTargetedHistogramFlag));
  return *histogram;
}

base::HistogramBase*
VisibilityMetricsLogger::GetPerWebViewVisibilityHistogram() {
  static NoDestructor<base::HistogramBase*> histogram(
      base::Histogram::FactoryGet(
          "Android.WebView.Visibility.PerWebView", 1,
          static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue) + 1,
          static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue) + 2,
          base::HistogramBase::kUmaTargetedHistogramFlag));
  return *histogram;
}

VisibilityMetricsLogger::VisibilityMetricsLogger() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  last_update_time_ = base::TimeTicks::Now();
}

VisibilityMetricsLogger::~VisibilityMetricsLogger() = default;

void VisibilityMetricsLogger::AddClient(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(client_visibility_.find(client) == client_visibility_.end());

  UpdateDurations(base::TimeTicks::Now());

  client_visibility_[client] = VisibilityInfo();
  ProcessClientUpdate(client, client->GetVisibilityInfo());
}

void VisibilityMetricsLogger::RemoveClient(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(client_visibility_.find(client) != client_visibility_.end());

  UpdateDurations(base::TimeTicks::Now());

  ProcessClientUpdate(client, VisibilityInfo());
  client_visibility_.erase(client);
}

void VisibilityMetricsLogger::ClientVisibilityChanged(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(client_visibility_.find(client) != client_visibility_.end());

  UpdateDurations(base::TimeTicks::Now());

  ProcessClientUpdate(client, client->GetVisibilityInfo());
}

void VisibilityMetricsLogger::UpdateDurations(base::TimeTicks update_time) {
  base::TimeDelta delta = update_time - last_update_time_;
  if (visible_client_count_ > 0) {
    any_webview_visible_duration_ += delta;
  } else {
    no_webview_visible_duration_ += delta;
  }
  total_webview_visible_duration_ += delta * visible_client_count_;
  total_webview_hidden_duration_ +=
      delta * (client_visibility_.size() - visible_client_count_);

  last_update_time_ = update_time;
}

bool VisibilityMetricsLogger::IsVisible(const VisibilityInfo& info) {
  return info.view_attached && info.view_visible && info.window_visible;
}

void VisibilityMetricsLogger::ProcessClientUpdate(Client* client,
                                                  const VisibilityInfo& info) {
  VisibilityInfo curr_info = client_visibility_[client];
  bool was_visible = IsVisible(curr_info);
  bool is_visible = IsVisible(info);
  client_visibility_[client] = info;

  DCHECK(!was_visible || visible_client_count_ > 0);
  if (!was_visible && is_visible) {
    ++visible_client_count_;
  } else if (was_visible && !is_visible) {
    --visible_client_count_;
  }
}

void VisibilityMetricsLogger::RecordMetrics() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UpdateDurations(base::TimeTicks::Now());

  int32_t any_webview_visible_seconds;
  int32_t no_webview_visible_seconds;
  int32_t total_webview_visible_seconds;
  int32_t total_webview_hidden_seconds;

  any_webview_visible_seconds = any_webview_visible_duration_.InSeconds();
  any_webview_visible_duration_ -=
      base::TimeDelta::FromSeconds(any_webview_visible_seconds);

  no_webview_visible_seconds = no_webview_visible_duration_.InSeconds();
  no_webview_visible_duration_ -=
      base::TimeDelta::FromSeconds(no_webview_visible_seconds);

  total_webview_visible_seconds = total_webview_visible_duration_.InSeconds();
  total_webview_visible_duration_ -=
      base::TimeDelta::FromSeconds(total_webview_visible_seconds);

  total_webview_hidden_seconds = total_webview_hidden_duration_.InSeconds();
  total_webview_hidden_duration_ -=
      base::TimeDelta::FromSeconds(total_webview_hidden_seconds);

  if (any_webview_visible_seconds) {
    GetGlobalVisibilityHistogram()->AddCount(
        static_cast<int>(Visibility::kVisible), any_webview_visible_seconds);
  }
  if (no_webview_visible_seconds) {
    GetGlobalVisibilityHistogram()->AddCount(
        static_cast<int>(Visibility::kNotVisible), no_webview_visible_seconds);
  }

  if (total_webview_visible_seconds) {
    GetPerWebViewVisibilityHistogram()->AddCount(
        static_cast<int>(Visibility::kVisible), total_webview_visible_seconds);
  }
  if (total_webview_hidden_seconds) {
    GetPerWebViewVisibilityHistogram()->AddCount(
        static_cast<int>(Visibility::kNotVisible),
        total_webview_hidden_seconds);
  }
}

}  // namespace android_webview