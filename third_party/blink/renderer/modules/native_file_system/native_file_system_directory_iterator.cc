// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_iterator.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_iterator_entry.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

NativeFileSystemDirectoryIterator::NativeFileSystemDirectoryIterator(
    NativeFileSystemDirectoryHandle* directory,
    ExecutionContext* execution_context)
    : ContextLifecycleObserver(execution_context), directory_(directory) {
  directory_->MojoHandle()->GetEntries(
      WTF::Bind(&NativeFileSystemDirectoryIterator::OnGotEntries,
                WrapWeakPersistent(this)));
}

ScriptPromise NativeFileSystemDirectoryIterator::next(
    ScriptState* script_state) {
  if (error_ != base::File::FILE_OK) {
    return ScriptPromise::RejectWithDOMException(
        script_state, file_error::CreateDOMException(error_));
  }

  if (!entries_.IsEmpty()) {
    NativeFileSystemDirectoryIteratorEntry* result =
        NativeFileSystemDirectoryIteratorEntry::Create();
    result->setValue(entries_.TakeFirst());
    return ScriptPromise::Cast(script_state, ToV8(result, script_state));
  }

  if (waiting_for_more_entries_) {
    DCHECK(!pending_next_);
    pending_next_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    return pending_next_->Promise();
  }

  NativeFileSystemDirectoryIteratorEntry* result =
      NativeFileSystemDirectoryIteratorEntry::Create();
  result->setDone(true);
  return ScriptPromise::Cast(script_state, ToV8(result, script_state));
}

void NativeFileSystemDirectoryIterator::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(entries_);
  visitor->Trace(pending_next_);
  visitor->Trace(directory_);
}

void NativeFileSystemDirectoryIterator::OnGotEntries(
    mojom::blink::NativeFileSystemErrorPtr result,
    Vector<mojom::blink::NativeFileSystemEntryPtr> entries) {
  if (!GetExecutionContext())
    return;
  if (result->error_code != base::File::FILE_OK) {
    error_ = result->error_code;
    if (pending_next_) {
      pending_next_->Reject(file_error::CreateDOMException(error_));
      pending_next_ = nullptr;
    }
    return;
  }
  for (auto& e : entries) {
    entries_.push_back(NativeFileSystemHandle::CreateFromMojoEntry(
        std::move(e), GetExecutionContext()));
  }
  waiting_for_more_entries_ = false;
  if (pending_next_) {
    ScriptState::Scope scope(pending_next_->GetScriptState());
    pending_next_->Resolve(
        next(pending_next_->GetScriptState()).GetScriptValue());
    pending_next_ = nullptr;
  }
}

}  // namespace blink
