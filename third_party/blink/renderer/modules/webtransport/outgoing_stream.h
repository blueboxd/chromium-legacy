// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_OUTGOING_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_OUTGOING_STREAM_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/util/type_safety/strong_alias.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace v8 {
class Isolate;
}

namespace blink {

class ScriptState;
class StreamAbortInfo;
class WebTransportCloseProxy;
class WritableStream;
class WritableStreamDefaultController;

// Implementation of the OutgoingStream mixin from the standard. SendStream and
// BidirectionalStream inherit from this.
class MODULES_EXPORT OutgoingStream
    : public ScriptWrappable,
      public ActiveScriptWrappable<OutgoingStream>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(OutgoingStream, Dispose);
  USING_GARBAGE_COLLECTED_MIXIN(OutgoingStream);

 public:
  OutgoingStream(ScriptState*,
                 WebTransportCloseProxy*,
                 mojo::ScopedDataPipeProducerHandle);
  ~OutgoingStream() override;

  // Init() must be called before the stream is used.
  virtual void Init();

  WebTransportCloseProxy* GetWebTransportCloseProxy() { return close_proxy_; }

  // Implementation of outgoing_stream.idl.
  WritableStream* writable() const {
    DVLOG(1) << "OutgoingStream::writable() called";

    return writable_;
  }

  ScriptPromise writingAborted() const { return writing_aborted_; }

  void abortWriting();
  void abortWriting(StreamAbortInfo*);

  // Called via WebTransportCloseProxy. Expects a JavaScript scope to be
  // entered.
  void Reset();

  // OutgoingStream cannot be collected until it is explicitly closed, either
  // remotely or locally.
  bool HasPendingActivity() const final { return writing_aborted_resolver_; }

  // Implementation of ExecutionContextLifecycleObserver.
  void ContextDestroyed() override;

  void Trace(Visitor*) override;

 private:
  class UnderlyingSink;

  using IsLocalAbort = util::StrongAlias<class IsLocalAbortTag, bool>;

  // Called when |data_pipe_| becomes writable or errored.
  void OnHandleReady(MojoResult, const mojo::HandleSignalsState&);

  // Called when |data_pipe_| is closed.
  void OnPeerClosed(MojoResult, const mojo::HandleSignalsState&);

  // Rejects any unfinished write() calls and resets |data_pipe_|.
  void HandlePipeClosed();

  // Implements UnderlyingSink::write().
  ScriptPromise SinkWrite(ScriptState*, ScriptValue chunk, ExceptionState&);

  // Writes |data| to |data_pipe_|, possible saving unwritten data to
  // |cached_data_|.
  ScriptPromise WriteOrCacheData(ScriptState*, base::span<const uint8_t> data);

  // Attempts to write some more of |cached_data_| to |data_pipe_|.
  void WriteCachedData();

  // Writes zero or more bytes of |data| synchronously to |data_pipe_|,
  // returning the number of bytes that were written.
  size_t WriteDataSynchronously(base::span<const uint8_t> data);

  // Creates a DOMException indicating that the stream has been aborted.
  // If IsLocalAbort it true it will indicate a locally-initiated abort,
  // otherwise it will indicate a remote-initiated abort.
  ScriptValue CreateAbortException(IsLocalAbort);

  // Errors |writable_|, resolves |writing_aborted_| and resets |data_pipe_|.
  // The error message used to error |writable_| depends on whether IsLocalAbort
  // is true or not.
  void ErrorStreamAbortAndReset(IsLocalAbort);

  // Resolve the |writing_aborted_| promise and reset the |data_pipe_|.
  void AbortAndReset();

  // Resets |data_pipe_| and clears the watchers. Also discards |cached_data_|.
  // If the pipe is open it will be closed as a side-effect.
  void ResetPipe();

  // Prepares the object for destruction.
  void Dispose();

  class CachedDataBuffer {
   public:
    CachedDataBuffer(v8::Isolate* isolate, const uint8_t* data, size_t length);

    ~CachedDataBuffer();

    size_t length() const { return length_; }

    uint8_t* data() { return buffer_; }

   private:
    // We need the isolate to call |AdjustAmountOfExternalAllocatedMemory| for
    // the memory stored in |buffer_|.
    v8::Isolate* isolate_;
    size_t length_ = 0u;
    uint8_t* buffer_ = nullptr;
  };

  const Member<ScriptState> script_state_;
  const Member<WebTransportCloseProxy> close_proxy_;
  mojo::ScopedDataPipeProducerHandle data_pipe_;

  // Only armed when we need to write something.
  mojo::SimpleWatcher write_watcher_;

  // Always armed to detect close.
  mojo::SimpleWatcher close_watcher_;

  // Data which has been passed to write() but still needs to be written
  // asynchronously.
  // Uses a custom CachedDataBuffer rather than a Vector because
  // WTF::Vector is currently limited to 2GB.
  // TODO(ricea): Change this to a Vector when it becomes 64-bit safe.
  std::unique_ptr<CachedDataBuffer> cached_data_;

  // The offset into |cached_data_| of the first byte that still needs to be
  // written.
  size_t offset_ = 0;

  Member<WritableStream> writable_;
  Member<WritableStreamDefaultController> controller_;

  // Promise returned by the |writingAborted| attribute.
  ScriptPromise writing_aborted_;
  Member<ScriptPromiseResolver> writing_aborted_resolver_;

  // If an asynchronous write() on the underlying sink object is pending, this
  // will be non-null.
  Member<ScriptPromiseResolver> write_promise_resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_OUTGOING_STREAM_H_
