// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_PRINT_SPOOLER_ARC_PRINT_RENDERER_H_
#define COMPONENTS_ARC_PRINT_SPOOLER_ARC_PRINT_RENDERER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/mojom/print_spooler.mojom.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// Implementation of PrintRenderer interface. Allows ARC to render print
// documents for Chrome print preview.
class ArcPrintRenderer : public printing::mojom::PrintRenderer,
                         public content::WebContentsUserData<ArcPrintRenderer> {
 public:
  ~ArcPrintRenderer() override;

  // Used to set the PrintRendererDelegate used by the PrintRenderer.
  void SetDelegate(arc::mojom::PrintRendererDelegatePtr delegate);

 private:
  explicit ArcPrintRenderer(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ArcPrintRenderer>;

  // Used to associate the PrintRenderer interface with a RenderFrameHost.
  content::WebContentsFrameBindingSet<printing::mojom::PrintRenderer> binding_;

  // Used to forward render requests to ARC.
  arc::mojom::PrintRendererDelegatePtr delegate_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcPrintRenderer> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ArcPrintRenderer);
};

#endif  // COMPONENTS_ARC_PRINT_SPOOLER_ARC_PRINT_RENDERER_H_
