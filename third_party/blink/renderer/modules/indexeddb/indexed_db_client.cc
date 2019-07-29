// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_client.h"

#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

IndexedDBClient::IndexedDBClient(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

IndexedDBClient::IndexedDBClient(WorkerClients& clients)
    : Supplement<WorkerClients>(clients) {}

IndexedDBClient* IndexedDBClient::From(ExecutionContext* context) {
  if (auto* document = DynamicTo<Document>(context)) {
    return Supplement<LocalFrame>::From<IndexedDBClient>(document->GetFrame());
  }

  WorkerClients* clients = To<WorkerGlobalScope>(*context).Clients();
  DCHECK(clients);
  return Supplement<WorkerClients>::From<IndexedDBClient>(clients);
}

bool IndexedDBClient::AllowIndexedDB(ExecutionContext* context) {
  DCHECK(context->IsContextThread());
  SECURITY_DCHECK(context->IsDocument() || context->IsWorkerGlobalScope());

  if (auto* document = DynamicTo<Document>(context)) {
    LocalFrame* frame = document->GetFrame();
    if (!frame)
      return false;
    if (auto* settings_client = frame->GetContentSettingsClient()) {
      return settings_client->AllowIndexedDB(
          WebSecurityOrigin(context->GetSecurityOrigin()));
    }
    return true;
  }

  WebContentSettingsClient* content_settings_client =
      To<WorkerGlobalScope>(context)->ContentSettingsClient();
  if (!content_settings_client)
    return true;
  return content_settings_client->AllowIndexedDB(WebSecurityOrigin());
}

// static
const char IndexedDBClient::kSupplementName[] = "IndexedDBClient";

void IndexedDBClient::Trace(blink::Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
  Supplement<WorkerClients>::Trace(visitor);
}

void ProvideIndexedDBClientTo(LocalFrame& frame, IndexedDBClient* client) {
  Supplement<LocalFrame>::ProvideTo(frame, client);
}

void ProvideIndexedDBClientToWorker(WorkerClients* clients,
                                    IndexedDBClient* client) {
  Supplement<WorkerClients>::ProvideTo(*clients, client);
}

}  // namespace blink
