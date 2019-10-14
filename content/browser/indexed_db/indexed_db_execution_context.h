// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXECUTION_CONTEXT_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXECUTION_CONTEXT_H_

namespace content {

// Identifies the frame or worker holding an IndexedDB object.
struct IndexedDBExecutionContext {
  constexpr IndexedDBExecutionContext(int render_process_id,
                                      int render_frame_id)
      : render_process_id(render_process_id),
        render_frame_id(render_frame_id) {}

  // The process hosting the frame or worker.
  const int render_process_id;

  // The frame id, or MSG_ROUTING_NONE if this identifies a worker.
  const int render_frame_id;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_EXECUTION_CONTEXT_H_
