// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_

#include <map>

#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace base {

class HistogramBase;

}  // namespace base

namespace android_webview {

class VisibilityMetricsLogger {
 public:
  struct VisibilityInfo {
    bool view_attached = false;
    bool view_visible = false;
    bool window_visible = false;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Visibility {
    kVisible = 0,
    kNotVisible = 1,
    kMaxValue = kNotVisible
  };

  class Client {
   public:
    virtual VisibilityInfo GetVisibilityInfo() = 0;
  };

  VisibilityMetricsLogger();
  virtual ~VisibilityMetricsLogger();

  VisibilityMetricsLogger(const VisibilityMetricsLogger&) = delete;
  VisibilityMetricsLogger& operator=(const VisibilityMetricsLogger&) = delete;

  void AddClient(Client* client);
  void RemoveClient(Client* client);
  void ClientVisibilityChanged(Client* client);

  void RecordMetrics();

 private:
  static base::HistogramBase* GetGlobalVisibilityHistogram();
  static base::HistogramBase* GetPerWebViewVisibilityHistogram();

  void UpdateDurations(base::TimeTicks update_time);
  void ProcessClientUpdate(Client* client, const VisibilityInfo& info);
  bool IsVisible(const VisibilityInfo& info);

  // Current number of visible webviews.
  size_t visible_client_count_ = 0;

  // Duration for which any webview was visible.
  base::TimeDelta any_webview_visible_duration_ =
      base::TimeDelta::FromSeconds(0);
  // Duration for which no webviews were visible.
  base::TimeDelta no_webview_visible_duration_ =
      base::TimeDelta::FromSeconds(0);
  // Total duration for which all webviews were visible.
  base::TimeDelta total_webview_visible_duration_ =
      base::TimeDelta::FromSeconds(0);
  // Total duration for which all webviews were not visible.
  base::TimeDelta total_webview_hidden_duration_ =
      base::TimeDelta::FromSeconds(0);

  base::TimeTicks last_update_time_;
  std::map<Client*, VisibilityInfo> client_visibility_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_
