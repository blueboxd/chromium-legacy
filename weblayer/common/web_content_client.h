// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_COMMON_WEB_CONTENT_CLIENT_H_
#define WEBLAYER_COMMON_WEB_CONTENT_CLIENT_H_

#include "content/public/common/content_client.h"

namespace weblayer {

class WebContentClient : public content::ContentClient {
 public:
  WebContentClient();
  ~WebContentClient() override;

  base::StringPiece GetDataResource(int resource_id,
                                    ui::ScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_COMMON_WEB_CONTENT_CLIENT_H_
