// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/isolated_world_util.h"

#include "base/check_op.h"
#include "base/optional.h"
#include "components/translate/core/common/translate_util.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_url.h"

namespace translate {

void EnsureIsolatedWorldInitialized(int world_id) {
  static base::Optional<int> last_used_world_id;
  if (last_used_world_id) {
    // Early return since the isolated world info. is already initialized.
    DCHECK_EQ(*last_used_world_id, world_id)
        << "EnsureIsolatedWorldInitialized should always be called with the "
           "same |world_id|";
    return;
  }

  last_used_world_id = world_id;
  constexpr char kContentSecurityPolicy[] = "script-src 'self' 'unsafe-eval'";

  blink::WebIsolatedWorldInfo info;
  info.security_origin =
      blink::WebSecurityOrigin::Create(GetTranslateSecurityOrigin());
  info.content_security_policy =
      blink::WebString::FromUTF8(kContentSecurityPolicy);
  blink::SetIsolatedWorldInfo(world_id, info);
}

}  // namespace translate
