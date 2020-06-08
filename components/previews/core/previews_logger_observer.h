
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_LOGGER_OBSERVER_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_LOGGER_OBSERVER_H_

#include "base/macros.h"
#include "components/previews/core/previews_logger.h"

namespace previews {

// An interface for PreviewsLogger observer. This interface is for responding to
// events occur in PreviewsLogger (e.g. PreviewsLogger::LogMessage events).
class PreviewsLoggerObserver {
 public:
  // Notifies this observer that |message| has been added in PreviewsLogger.
  virtual void OnNewMessageLogAdded(
      const PreviewsLogger::MessageLog& message) = 0;

  // Notifies this observer that |host| is blocklisted at |time|.
  virtual void OnNewBlocklistedHost(const std::string& host,
                                    base::Time time) = 0;

  // Notifies this observer that the user blocklisted state has changed to
  // |blocklisted|.
  virtual void OnUserBlocklistedStatusChange(bool blocklisted) = 0;

  // Notifies this observer that the blocklist is cleared at |time|.
  virtual void OnBlocklistCleared(base::Time time) = 0;

  // Notify this observer that PreviewsBlockList decisions is ignored or not.
  virtual void OnIgnoreBlocklistDecisionStatusChanged(bool ignored) = 0;

  // Notify this observer that |this| is the last observer to be removed from
  // the Logger's observers list.
  virtual void OnLastObserverRemove() = 0;

 protected:
  PreviewsLoggerObserver() {}
  virtual ~PreviewsLoggerObserver() {}
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_LOG_OBSERVER_H_
