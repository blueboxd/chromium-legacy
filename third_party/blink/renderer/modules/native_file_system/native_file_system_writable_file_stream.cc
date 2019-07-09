// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/native_file_system_writable_file_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view_or_blob_or_usv_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

NativeFileSystemWritableFileStream::NativeFileSystemWritableFileStream(
    RevocableInterfacePtr<mojom::blink::NativeFileSystemFileWriter> mojo_ptr)
    : mojo_ptr_(std::move(mojo_ptr)) {
  DCHECK(mojo_ptr_);
}

ScriptPromise NativeFileSystemWritableFileStream::write(
    ScriptState* script_state,
    uint64_t position,
    const ArrayBufferOrArrayBufferViewOrBlobOrUSVString& data,
    ExceptionState& exception_state) {
  DCHECK(!data.IsNull());

  auto blob_data = std::make_unique<BlobData>();
  Blob* blob = nullptr;
  if (data.IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer = data.GetAsArrayBuffer();
    blob_data->AppendBytes(array_buffer->Data(), array_buffer->ByteLength());
  } else if (data.IsArrayBufferView()) {
    DOMArrayBufferView* array_buffer_view = data.GetAsArrayBufferView().View();
    blob_data->AppendBytes(array_buffer_view->BaseAddress(),
                           array_buffer_view->byteLength());
  } else if (data.IsBlob()) {
    blob = data.GetAsBlob();
  } else if (data.IsUSVString()) {
    // Let the developer be explicit about line endings.
    blob_data->AppendText(data.GetAsUSVString(),
                          /*normalize_line_endings_to_native=*/false);
  }

  if (!blob) {
    uint64_t size = blob_data->length();
    blob = Blob::Create(BlobDataHandle::Create(std::move(blob_data), size));
  }

  return WriteBlob(script_state, position, blob);
}

ScriptPromise NativeFileSystemWritableFileStream::truncate(
    ScriptState* script_state,
    uint64_t size) {
  if (!mojo_ptr_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError));
  }
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = pending_operation_->Promise();
  mojo_ptr_->Truncate(
      size, WTF::Bind(&NativeFileSystemWritableFileStream::TruncateComplete,
                      WrapPersistent(this)));
  return result;
}

void NativeFileSystemWritableFileStream::WriteComplete(
    mojom::blink::NativeFileSystemErrorPtr result,
    uint64_t bytes_written) {
  DCHECK(pending_operation_);
  if (result->error_code == base::File::FILE_OK) {
    pending_operation_->Resolve();
  } else {
    pending_operation_->Reject(
        file_error::CreateDOMException(result->error_code));
  }
  pending_operation_ = nullptr;
}

void NativeFileSystemWritableFileStream::TruncateComplete(
    mojom::blink::NativeFileSystemErrorPtr result) {
  DCHECK(pending_operation_);
  if (result->error_code == base::File::FILE_OK) {
    pending_operation_->Resolve();
  } else {
    pending_operation_->Reject(
        file_error::CreateDOMException(result->error_code));
  }
  pending_operation_ = nullptr;
}

ScriptPromise NativeFileSystemWritableFileStream::WriteBlob(
    ScriptState* script_state,
    uint64_t position,
    Blob* blob) {
  if (!mojo_ptr_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError));
  }
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = pending_operation_->Promise();
  mojo_ptr_->Write(position, blob->AsMojoBlob(),
                   WTF::Bind(&NativeFileSystemWritableFileStream::WriteComplete,
                             WrapPersistent(this)));
  return result;
}

ScriptPromise NativeFileSystemWritableFileStream::abort(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError));
}

ScriptPromise NativeFileSystemWritableFileStream::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotSupportedError));
}

ScriptValue NativeFileSystemWritableFileStream::getWriter(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not Implemented");
  return ScriptValue();
}

bool NativeFileSystemWritableFileStream::locked(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  auto result = IsLocked(script_state, exception_state);

  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not Implemented");
  return !result || *result;
}

base::Optional<bool> NativeFileSystemWritableFileStream::IsLocked(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not Implemented");
  return base::nullopt;
}

void NativeFileSystemWritableFileStream::Serialize(
    ScriptState* script_state,
    MessagePort* port,
    ExceptionState& exception_state) {
  DCHECK(port);
  DCHECK(RuntimeEnabledFeatures::TransferableStreamsEnabled());
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Not Implemented");
}

void NativeFileSystemWritableFileStream::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(pending_operation_);
}

}  // namespace blink
