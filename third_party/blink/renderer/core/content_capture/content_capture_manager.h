// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_task.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"

namespace blink {

class LocalFrame;
class Node;
class SentNodes;

// This class is used to create the NodeHolder, and start the ContentCaptureTask
// when necessary. The ContentCaptureManager is owned by main frame.
class CORE_EXPORT ContentCaptureManager
    : public GarbageCollected<ContentCaptureManager> {
 public:
  explicit ContentCaptureManager(LocalFrame& local_frame_root);
  virtual ~ContentCaptureManager();

  // Schedules ContentCaptureTask if it isn't already scheduled.
  void ScheduleTaskIfNeeded();

  // Invokes when the |node_holder| asscociated LayoutText will be destroyed.
  void OnLayoutTextWillBeDestroyed(const Node& node);

  // Invokes when scroll position was changed.
  void OnScrollPositionChanged();

  // Invokes when text node content was changed.
  void OnNodeTextChanged(Node& node);

  // Invokes when the local_frame_root shutdown.
  void Shutdown();

  virtual void Trace(Visitor*);

  ContentCaptureTask* GetContentCaptureTaskForTesting() const {
    return content_capture_idle_task_;
  }

 protected:
  virtual ContentCaptureTask* CreateContentCaptureTask();
  TaskSession& GetTaskSessionForTesting() const { return *task_session_; }

 private:
  void NotifyNodeDetached(const Node& node);
  void ScheduleTask(ContentCaptureTask::ScheduleReason reason);

  Member<ContentCaptureTask> content_capture_idle_task_;

  Member<LocalFrame> local_frame_root_;

  // Indicates if the first NodeHolder is created.
  bool first_node_holder_created_ = false;

  Member<TaskSession> task_session_;

  // A set of weak reference of the node that has been sent.
  Member<SentNodes> sent_nodes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_MANAGER_H_
