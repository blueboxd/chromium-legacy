// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port_underlying_source.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"

namespace blink {

SerialPortUnderlyingSource::SerialPortUnderlyingSource(
    ScriptState* script_state,
    SerialPort* serial_port,
    mojo::ScopedDataPipeConsumerHandle handle)
    : UnderlyingSourceBase(script_state),
      data_pipe_(std::move(handle)),
      watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      serial_port_(serial_port) {
  watcher_.Watch(data_pipe_.get(),
                 MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                 MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                 WTF::BindRepeating(&SerialPortUnderlyingSource::OnHandleReady,
                                    WrapWeakPersistent(this)));
}

ScriptPromise SerialPortUnderlyingSource::pull(ScriptState* script_state) {
  // Only one pending call to pull() is allowed by the spec.
  DCHECK(!pending_pull_);
  // pull() shouldn't be called if an error has been signaled to the controller.
  DCHECK(data_pipe_);

  if (ReadData())
    return ScriptPromise::CastUndefined(script_state);

  return ArmWatcher(script_state);
}

ScriptPromise SerialPortUnderlyingSource::Cancel(ScriptState* script_state,
                                                 ScriptValue reason) {
  Close();
  return ScriptPromise::CastUndefined(script_state);
}

void SerialPortUnderlyingSource::ContextDestroyed(
    ExecutionContext* execution_context) {
  Close();
  UnderlyingSourceBase::ContextDestroyed(execution_context);
}

void SerialPortUnderlyingSource::SignalErrorImmediately(
    DOMException* exception) {
  SignalErrorOnClose(exception);
  PipeClosed();
}

void SerialPortUnderlyingSource::SignalErrorOnClose(DOMException* exception) {
  if (data_pipe_) {
    // Pipe is still open. Wait for PipeClosed() to be called.
    pending_exception_ = exception;
    return;
  }

  Controller()->Error(exception);
  serial_port_->UnderlyingSourceClosed();
}

void SerialPortUnderlyingSource::ExpectClose() {
  if (data_pipe_) {
    // Pipe is still open. Wait for PipeClosed() to be called.
    expect_close_ = true;
    return;
  }

  Controller()->Close();
  serial_port_->UnderlyingSourceClosed();
}

void SerialPortUnderlyingSource::Trace(Visitor* visitor) {
  visitor->Trace(pending_pull_);
  visitor->Trace(pending_exception_);
  visitor->Trace(serial_port_);
  UnderlyingSourceBase::Trace(visitor);
}

bool SerialPortUnderlyingSource::ReadData() {
  const void* buffer = nullptr;
  uint32_t available = 0;
  MojoResult result =
      data_pipe_->BeginReadData(&buffer, &available, MOJO_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK: {
      auto* array = DOMUint8Array::Create(
          static_cast<const unsigned char*>(buffer), available);
      result = data_pipe_->EndReadData(available);
      DCHECK_EQ(result, MOJO_RESULT_OK);
      Controller()->Enqueue(array);
      return true;
    }
    case MOJO_RESULT_FAILED_PRECONDITION:
      PipeClosed();
      return true;
    case MOJO_RESULT_SHOULD_WAIT:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

ScriptPromise SerialPortUnderlyingSource::ArmWatcher(
    ScriptState* script_state) {
  MojoResult ready_result;
  mojo::HandleSignalsState ready_state;
  MojoResult result = watcher_.Arm(&ready_result, &ready_state);
  if (result == MOJO_RESULT_OK) {
    pending_pull_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    return pending_pull_->Promise();
  }

  DCHECK_EQ(ready_result, MOJO_RESULT_OK);
  if (ready_state.readable()) {
    bool read_result = ReadData();
    DCHECK(read_result);
  } else if (ready_state.peer_closed()) {
    PipeClosed();
  }

  return ScriptPromise::CastUndefined(script_state);
}

void SerialPortUnderlyingSource::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK(pending_pull_);

  switch (result) {
    case MOJO_RESULT_OK: {
      bool read_result = ReadData();
      DCHECK(read_result);
      // If the pipe was closed |pending_pull_| will have been resolved.
      if (pending_pull_) {
        pending_pull_->Resolve();
        pending_pull_ = nullptr;
      }
      break;
    }
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_.ArmOrNotify();
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      PipeClosed();
      break;
  }
}

void SerialPortUnderlyingSource::PipeClosed() {
  if (pending_exception_) {
    Controller()->Error(pending_exception_);
    serial_port_->UnderlyingSourceClosed();
  }
  if (expect_close_) {
    Controller()->Close();
    serial_port_->UnderlyingSourceClosed();
  }
  Close();
}

void SerialPortUnderlyingSource::Close() {
  watcher_.Cancel();
  data_pipe_.reset();
  if (pending_pull_) {
    pending_pull_->Resolve();
    pending_pull_ = nullptr;
  }
}

}  // namespace blink
