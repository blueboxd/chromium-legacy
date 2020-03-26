// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_source_id.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/rand_util.h"

namespace ukm {

SourceId AssignNewSourceId() {
  return base::UkmSourceId::New().ToInt64();
}

SourceId ConvertToSourceId(int64_t other_id, SourceIdType id_type) {
  // DCHECK is to restrict the usage of WEBAPK_ID and PAYMENT_APP_ID. WebApk and
  // Payment apps should use |UkmRecorder::GetSourceIdForWebApkManifestUrl()|
  // and |UkmRecorder::GetSourceIdForPaymentAppFromScope()| instead.
  // TODO(crbug.com/1046964): Ideally we should restrict
  // UkmSourceId::FromOtherId() as well.
  DCHECK(id_type != SourceIdType::WEBAPK_ID);
  DCHECK(id_type != SourceIdType::PAYMENT_APP_ID);
  return base::UkmSourceId::FromOtherId(other_id, id_type).ToInt64();
}

SourceIdType GetSourceIdType(SourceId source_id) {
  return base::UkmSourceId::FromInt64(source_id).GetType();
}

}  // namespace ukm
