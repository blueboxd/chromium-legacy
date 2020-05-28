// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_vp9_picture.h"

namespace media {

D3D11VP9Picture::D3D11VP9Picture(D3D11PictureBuffer* picture_buffer)
    : picture_buffer_(picture_buffer),
      picture_index_(picture_buffer_->picture_index()) {
  picture_buffer_->set_in_picture_use(true);
}

D3D11VP9Picture::~D3D11VP9Picture() {
  picture_buffer_->set_in_picture_use(false);
}

}  // namespace media
