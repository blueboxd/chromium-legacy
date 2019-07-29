/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/filesystem/local_file_system_client.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

LocalFileSystemClient::~LocalFileSystemClient() = default;

bool LocalFileSystemClient::RequestFileSystemAccessSync(
    ExecutionContext* context) {
  DCHECK(context);
  if (IsA<Document>(context)) {
    // TODO(dcheng): Why is this NOTREACHED and handled?
    NOTREACHED();
    return false;
  }

  WebContentSettingsClient* content_settings_client =
      To<WorkerGlobalScope>(context)->ContentSettingsClient();
  if (!content_settings_client)
    return true;
  return content_settings_client->RequestFileSystemAccessSync();
}

void LocalFileSystemClient::RequestFileSystemAccessAsync(
    ExecutionContext* context,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(context);
  auto* document = DynamicTo<Document>(context);
  if (!document) {
    // TODO(dcheng): Why is this NOTREACHED and handled?
    NOTREACHED();
    return;
  }

  if (auto* client = document->GetFrame()->GetContentSettingsClient()) {
    client->RequestFileSystemAccessAsync(std::move(callback));
  } else {
    std::move(callback).Run(true);
  }
}

LocalFileSystemClient::LocalFileSystemClient() = default;

}  // namespace blink
