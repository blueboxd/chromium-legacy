// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_AW_ATTACHING_TO_WINDOW_RECORDER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_AW_ATTACHING_TO_WINDOW_RECORDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace android_webview {

class AwAttachingToWindowRecorder
    : public base::RefCountedThreadSafe<AwAttachingToWindowRecorder> {
 public:
  AwAttachingToWindowRecorder();

  // Start recording.
  void Start();
  void OnAttachedToWindow();
  void OnDestroyed();

 private:
  friend class base::RefCountedThreadSafe<AwAttachingToWindowRecorder>;

  ~AwAttachingToWindowRecorder();

  void RecordAttachedToWindow(base::TimeDelta time);
  void RecordEverAttachedToWindow();
  void Ping(size_t interval_index);

  base::TimeTicks created_time_;
  bool was_attached_ = false;
  scoped_refptr<base::SequencedTaskRunner> thread_pool_runner_;

  SEQUENCE_CHECKER(thread_pool_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AwAttachingToWindowRecorder);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_AW_ATTACHING_TO_WINDOW_RECORDER_H_
