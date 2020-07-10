// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_SELECT_FILE_CROSAPI_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_SELECT_FILE_CROSAPI_H_

#include "chromeos/lacros/mojom/select_file.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Implements the SelectFile mojo interface for open/save dialogs. Wraps the
// underlying Chrome OS SelectFileExtension implementation, which uses the WebUI
// file manager to provide the dialogs. Lives on the UI thread.
class SelectFileCrosapi : public lacros::mojom::SelectFile {
 public:
  explicit SelectFileCrosapi(
      mojo::PendingReceiver<lacros::mojom::SelectFile> receiver);
  SelectFileCrosapi(const SelectFileCrosapi&) = delete;
  SelectFileCrosapi& operator=(const SelectFileCrosapi&) = delete;
  ~SelectFileCrosapi() override;

  // lacros::mojom::SelectFile:
  void Select(lacros::mojom::SelectFileOptionsPtr options,
              SelectCallback callback) override;

 private:
  mojo::Receiver<lacros::mojom::SelectFile> receiver_;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_SELECT_FILE_CROSAPI_H_
