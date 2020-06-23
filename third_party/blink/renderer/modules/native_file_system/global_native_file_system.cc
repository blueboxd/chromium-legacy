// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/global_native_file_system.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_choose_file_system_entries_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_choose_file_system_entries_options_accepts.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_picker_accept_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_open_file_picker_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_save_file_picker_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_error.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
// The name to use for the root directory of a sandboxed file system.
constexpr const char kSandboxRootDirectoryName[] = "";

mojom::blink::ChooseFileSystemEntryType ConvertChooserType(const String& input,
                                                           bool multiple) {
  if (input == "open-file" || input == "openFile") {
    return multiple
               ? mojom::blink::ChooseFileSystemEntryType::kOpenMultipleFiles
               : mojom::blink::ChooseFileSystemEntryType::kOpenFile;
  }
  if (input == "save-file" || input == "saveFile")
    return mojom::blink::ChooseFileSystemEntryType::kSaveFile;
  if (input == "open-directory" || input == "openDirectory")
    return mojom::blink::ChooseFileSystemEntryType::kOpenDirectory;
  NOTREACHED();
  return mojom::blink::ChooseFileSystemEntryType::kOpenFile;
}

Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> ConvertAccepts(
    const HeapVector<Member<ChooseFileSystemEntriesOptionsAccepts>>& accepts) {
  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> result;
  result.ReserveInitialCapacity(accepts.size());
  for (const auto& a : accepts) {
    result.emplace_back(
        blink::mojom::blink::ChooseFileSystemEntryAcceptsOption::New(
            a->hasDescription() ? a->description() : g_empty_string,
            a->hasMimeTypes() ? a->mimeTypes() : Vector<String>(),
            a->hasExtensions() ? a->extensions() : Vector<String>()));
  }
  return result;
}

constexpr bool IsHTTPWhitespace(UChar chr) {
  return chr == ' ' || chr == '\n' || chr == '\t' || chr == '\r';
}

Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> ConvertAccepts(
    const HeapVector<Member<FilePickerAcceptType>>& types,
    ExceptionState& exception_state) {
  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> result;
  result.ReserveInitialCapacity(types.size());
  for (const auto& t : types) {
    Vector<String> mimeTypes;
    mimeTypes.ReserveInitialCapacity(t->accept().size());
    Vector<String> extensions;
    for (const auto& a : t->accept()) {
      String type = a.first.StripWhiteSpace(IsHTTPWhitespace);
      if (type.IsEmpty()) {
        exception_state.ThrowTypeError("Invalid type: " + a.first);
        return {};
      }
      Vector<String> parsed_type;
      type.Split('/', true, parsed_type);
      if (parsed_type.size() != 2) {
        exception_state.ThrowTypeError("Invalid type: " + a.first);
        return {};
      }
      if (!IsValidHTTPToken(parsed_type[0])) {
        exception_state.ThrowTypeError("Invalid type: " + a.first);
        return {};
      }
      if (!IsValidHTTPToken(parsed_type[1])) {
        exception_state.ThrowTypeError("Invalid type: " + a.first);
        return {};
      }

      mimeTypes.push_back(type);
      extensions.AppendVector(a.second);
    }
    result.emplace_back(
        blink::mojom::blink::ChooseFileSystemEntryAcceptsOption::New(
            t->hasDescription() ? t->description() : g_empty_string,
            std::move(mimeTypes), std::move(extensions)));
  }
  return result;
}

ScriptPromise GetOriginPrivateDirectoryImpl(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->GetSecurityOrigin()->CanAccessNativeFileSystem()) {
    if (context->GetSecurityContext().IsSandboxed(
            network::mojom::blink::WebSandboxFlags::kOrigin)) {
      exception_state.ThrowSecurityError(
          "System directory access is denied because the context is "
          "sandboxed and lacks the 'allow-same-origin' flag.");
      return ScriptPromise();
    } else {
      exception_state.ThrowSecurityError("System directory access is denied.");
      return ScriptPromise();
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo::Remote<mojom::blink::NativeFileSystemManager> manager;
  context->GetBrowserInterfaceBroker().GetInterface(
      manager.BindNewPipeAndPassReceiver());

  auto* raw_manager = manager.get();
  raw_manager->GetSandboxedFileSystem(WTF::Bind(
      [](ScriptPromiseResolver* resolver,
         mojo::Remote<mojom::blink::NativeFileSystemManager>,
         mojom::blink::NativeFileSystemErrorPtr result,
         mojo::PendingRemote<mojom::blink::NativeFileSystemDirectoryHandle>
             handle) {
        ExecutionContext* context = resolver->GetExecutionContext();
        if (!context)
          return;
        if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
          native_file_system_error::Reject(resolver, *result);
          return;
        }
        resolver->Resolve(MakeGarbageCollected<NativeFileSystemDirectoryHandle>(
            context, kSandboxRootDirectoryName, std::move(handle)));
      },
      WrapPersistent(resolver), std::move(manager)));

  return result;
}

void VerifyIsAllowedToShowFilePicker(const LocalDOMWindow& window,
                                     ExceptionState& exception_state) {
  if (!window.IsCurrentlyDisplayedInFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError, "");
    return;
  }

  Document* document = window.document();
  if (!document) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError, "");
    return;
  }

  if (!document->GetSecurityOrigin()->CanAccessNativeFileSystem()) {
    if (document->IsSandboxed(
            network::mojom::blink::WebSandboxFlags::kOrigin)) {
      exception_state.ThrowSecurityError(
          "Sandboxed documents aren't allowed to show a file picker.");
      return;
    } else {
      exception_state.ThrowSecurityError(
          "This document isn't allowed to show a file picker.");
      return;
    }
  }

  LocalFrame* local_frame = window.GetFrame();
  if (!local_frame || local_frame->IsCrossOriginToMainFrame()) {
    exception_state.ThrowSecurityError(
        "Cross origin sub frames aren't allowed to show a file picker.");
    return;
  }

  if (!LocalFrame::HasTransientUserActivation(local_frame)) {
    exception_state.ThrowSecurityError(
        "Must be handling a user gesture to show a file picker.");
    return;
  }
}

ScriptPromise ShowFilePickerImpl(
    ScriptState* script_state,
    LocalDOMWindow& window,
    mojom::blink::ChooseFileSystemEntryType chooser_type,
    Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
    bool accept_all,
    bool return_as_sequence) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise resolver_result = resolver->Promise();

  // TODO(mek): Cache mojo::Remote<mojom::blink::NativeFileSystemManager>
  // associated with an ExecutionContext, so we don't have to request a new one
  // for each operation, and can avoid code duplication between here and other
  // uses.
  mojo::Remote<mojom::blink::NativeFileSystemManager> manager;
  window.GetBrowserInterfaceBroker().GetInterface(
      manager.BindNewPipeAndPassReceiver());

  auto* raw_manager = manager.get();
  raw_manager->ChooseEntries(
      chooser_type, std::move(accepts), accept_all,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             mojo::Remote<mojom::blink::NativeFileSystemManager>,
             bool return_as_sequence, LocalFrame* local_frame,
             mojom::blink::NativeFileSystemErrorPtr file_operation_result,
             Vector<mojom::blink::NativeFileSystemEntryPtr> entries) {
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context)
              return;
            if (file_operation_result->status !=
                mojom::blink::NativeFileSystemStatus::kOk) {
              native_file_system_error::Reject(resolver,
                                               *file_operation_result);
              return;
            }

            // While it would be better to not trust the renderer process,
            // we're doing this here to avoid potential mojo message pipe
            // ordering problems, where the frame activation state
            // reconciliation messages would compete with concurrent Native File
            // System messages to the browser.
            // TODO(https://crbug.com/1017270): Remove this after spec change,
            // or when activation moves to browser.
            LocalFrame::NotifyUserActivation(local_frame);

            if (return_as_sequence) {
              HeapVector<Member<NativeFileSystemHandle>> results;
              results.ReserveInitialCapacity(entries.size());
              for (auto& entry : entries) {
                results.push_back(NativeFileSystemHandle::CreateFromMojoEntry(
                    std::move(entry), context));
              }
              resolver->Resolve(results);
            } else {
              DCHECK_EQ(1u, entries.size());
              resolver->Resolve(NativeFileSystemHandle::CreateFromMojoEntry(
                  std::move(entries[0]), context));
            }
          },
          WrapPersistent(resolver), std::move(manager), return_as_sequence,
          WrapPersistent(window.GetFrame())));
  return resolver_result;
}

}  // namespace

// static
ScriptPromise GlobalNativeFileSystem::chooseFileSystemEntries(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const ChooseFileSystemEntriesOptions* options,
    ExceptionState& exception_state) {
  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  if (options->hasAccepts())
    accepts = ConvertAccepts(options->accepts());

  return ShowFilePickerImpl(
      script_state, window,
      ConvertChooserType(options->type(), options->multiple()),
      std::move(accepts), !options->excludeAcceptAllOption(),
      options->multiple());
}

// static
ScriptPromise GlobalNativeFileSystem::showOpenFilePicker(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const OpenFilePickerOptions* options,
    ExceptionState& exception_state) {
  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  if (options->hasTypes())
    accepts = ConvertAccepts(options->types(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (accepts.IsEmpty() && options->excludeAcceptAllOption()) {
    exception_state.ThrowTypeError("Need at least one accepted type");
    return ScriptPromise();
  }

  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return ShowFilePickerImpl(
      script_state, window,
      options->multiple()
          ? mojom::blink::ChooseFileSystemEntryType::kOpenMultipleFiles
          : mojom::blink::ChooseFileSystemEntryType::kOpenFile,
      std::move(accepts), !options->excludeAcceptAllOption(),
      /*return_as_sequence=*/true);
}

// static
ScriptPromise GlobalNativeFileSystem::showSaveFilePicker(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const SaveFilePickerOptions* options,
    ExceptionState& exception_state) {
  Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts;
  if (options->hasTypes())
    accepts = ConvertAccepts(options->types(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  if (accepts.IsEmpty() && options->excludeAcceptAllOption()) {
    exception_state.ThrowTypeError("Need at least one accepted type");
    return ScriptPromise();
  }

  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return ShowFilePickerImpl(
      script_state, window, mojom::blink::ChooseFileSystemEntryType::kSaveFile,
      std::move(accepts), !options->excludeAcceptAllOption(),
      /*return_as_sequence=*/false);
}

// static
ScriptPromise GlobalNativeFileSystem::showDirectoryPicker(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const DirectoryPickerOptions* options,
    ExceptionState& exception_state) {
  VerifyIsAllowedToShowFilePicker(window, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  return ShowFilePickerImpl(
      script_state, window,
      mojom::blink::ChooseFileSystemEntryType::kOpenDirectory, {},
      /*accept_all=*/true,
      /*return_as_sequence=*/false);
}

// static
ScriptPromise GlobalNativeFileSystem::getOriginPrivateDirectory(
    ScriptState* script_state,
    const LocalDOMWindow& window,
    ExceptionState& exception_state) {
  return GetOriginPrivateDirectoryImpl(script_state, exception_state);
}

// static
ScriptPromise GlobalNativeFileSystem::getOriginPrivateDirectory(
    ScriptState* script_state,
    const WorkerGlobalScope& workerGlobalScope,
    ExceptionState& exception_state) {
  return GetOriginPrivateDirectoryImpl(script_state, exception_state);
}

}  // namespace blink
