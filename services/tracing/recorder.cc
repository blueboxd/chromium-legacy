// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/recorder.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace tracing {

Recorder::Recorder(mojo::PendingReceiver<mojom::Recorder> receiver,
                   mojom::TraceDataType data_type,
                   const base::RepeatingClosure& on_data_change_callback)
    : is_recording_(true),
      data_type_(data_type),
      on_data_change_callback_(on_data_change_callback),
      receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &Recorder::OnConnectionError, weak_ptr_factory_.GetWeakPtr()));
}

Recorder::~Recorder() = default;

void Recorder::AddChunk(const std::string& chunk) {
  if (chunk.empty())
    return;
  if (data_type_ != mojom::TraceDataType::STRING && !data_.empty())
    data_.append(",");
  data_.append(chunk);
  on_data_change_callback_.Run();
}

void Recorder::AddMetadata(base::Value metadata) {
  base::DictionaryValue* dict;
  bool result = metadata.GetAsDictionary(&dict);
  DCHECK(result);

  metadata_.MergeDictionary(static_cast<base::DictionaryValue*>(&metadata));
}

void Recorder::OnConnectionError() {
  is_recording_ = false;
  on_data_change_callback_.Run();
}

}  // namespace tracing
