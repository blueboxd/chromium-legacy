// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_shm_image_pool.h"

#include "base/bind.h"
#include "base/location.h"
#include "build/build_config.h"
#include "ui/events/platform/platform_event_source.h"

namespace ui {

X11ShmImagePool::X11ShmImagePool(
    scoped_refptr<base::SequencedTaskRunner> host_task_runner,
    scoped_refptr<base::SequencedTaskRunner> event_task_runner,
    XDisplay* display,
    XID drawable,
    Visual* visual,
    int depth,
    std::size_t frames_pending)
    : XShmImagePoolBase(std::move(host_task_runner),
                        std::move(event_task_runner),
                        display,
                        drawable,
                        visual,
                        depth,
                        frames_pending) {}

X11ShmImagePool::~X11ShmImagePool() = default;

void X11ShmImagePool::AddEventDispatcher() {
  ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);

#ifndef NDEBUG
  dispatcher_registered_ = true;
#endif
}

void X11ShmImagePool::RemoveEventDispatcher() {
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

#ifndef NDEBUG
  dispatcher_registered_ = false;
#endif
}

bool X11ShmImagePool::CanDispatchEvent(const ui::PlatformEvent& event) {
  DCHECK(event_task_runner_->RunsTasksInCurrentSequence());

  XEvent* xevent = event;
  return CanDispatchXEvent(xevent);
}

uint32_t X11ShmImagePool::DispatchEvent(const ui::PlatformEvent& event) {
  DCHECK(event_task_runner_->RunsTasksInCurrentSequence());

  XShmCompletionEvent* shm_event =
      reinterpret_cast<XShmCompletionEvent*>(event);

  host_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&X11ShmImagePool::DispatchShmCompletionEvent,
                                this, *shm_event));

  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
