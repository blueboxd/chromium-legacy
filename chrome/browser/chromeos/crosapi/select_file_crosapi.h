// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_SELECT_FILE_CROSAPI_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_SELECT_FILE_CROSAPI_H_

#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Implements the SelectFile mojo interface for open/save dialogs. Wraps the
// underlying Chrome OS SelectFileExtension implementation, which uses the WebUI
// file manager to provide the dialogs. Lives on the UI thread.
class SelectFileCrosapi : public crosapi::mojom::SelectFile {
 public:
  explicit SelectFileCrosapi(
      mojo::PendingReceiver<crosapi::mojom::SelectFile> receiver);
  SelectFileCrosapi(const SelectFileCrosapi&) = delete;
  SelectFileCrosapi& operator=(const SelectFileCrosapi&) = delete;
  ~SelectFileCrosapi() override;

  // crosapi::mojom::SelectFile:
  void Select(crosapi::mojom::SelectFileOptionsPtr options,
              SelectCallback callback) override;

 private:
  mojo::Receiver<crosapi::mojom::SelectFile> receiver_;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_SELECT_FILE_CROSAPI_H_
