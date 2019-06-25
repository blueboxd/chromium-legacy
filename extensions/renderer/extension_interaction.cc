// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_interaction.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/service_worker_data.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "extensions/renderer/worker_thread_util.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_scoped_user_gesture.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"

namespace extensions {

// static
std::unique_ptr<ExtensionInteraction>
ExtensionInteraction::CreateScopeForWorker() {
  // Note: ExtensionInteraction ctor is private.
  return base::WrapUnique(new ExtensionInteraction(nullptr));
}

// static
std::unique_ptr<ExtensionInteraction>
ExtensionInteraction::CreateScopeForMainThread(
    blink::WebLocalFrame* web_frame) {
  // Note: ExtensionInteraction ctor is private.
  return base::WrapUnique(new ExtensionInteraction(web_frame));
}

// static
bool ExtensionInteraction::HasActiveInteraction(ScriptContext* context) {
  if (worker_thread_util::IsWorkerThread()) {
    DCHECK(!context->web_frame());
    ServiceWorkerData* worker_data =
        WorkerThreadDispatcher::GetServiceWorkerData();
    return worker_data->interaction_count() > 0 ||
           worker_data->is_service_worker_window_interaction_allowed();
  }
  return blink::WebUserGestureIndicator::IsProcessingUserGesture(
      context->web_frame());
}

ExtensionInteraction::ExtensionInteraction(blink::WebLocalFrame* context)
    : is_worker_thread_(worker_thread_util::IsWorkerThread()) {
  if (is_worker_thread_) {
    DCHECK(!context);
    WorkerThreadDispatcher::GetServiceWorkerData()->IncrementInteraction();
  } else {
    main_thread_gesture_ =
        std::make_unique<blink::WebScopedUserGesture>(context);
  }
}

ExtensionInteraction::~ExtensionInteraction() {
  if (is_worker_thread_)
    WorkerThreadDispatcher::GetServiceWorkerData()->DecrementInteraction();
}

}  // namespace extensions
