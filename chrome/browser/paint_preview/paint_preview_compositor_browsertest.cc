// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/unguessable_token.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace paint_preview {

class PaintPreviewCompositorCollectionBrowserTest
    : public InProcessBrowserTest {
 public:
  PaintPreviewCompositorCollectionBrowserTest() = default;
  ~PaintPreviewCompositorCollectionBrowserTest() override = default;

 protected:
  mojo::Remote<mojom::PaintPreviewCompositorCollection> StartUtilityProcess() {
    return content::ServiceProcessHost::Launch<
        mojom::PaintPreviewCompositorCollection>(
        content::ServiceProcessHost::Options()
            .WithSandboxType(service_manager::SANDBOX_TYPE_PDF_COMPOSITOR)
            .Pass());
  }
};

IN_PROC_BROWSER_TEST_F(PaintPreviewCompositorCollectionBrowserTest,
                       TestInitializationSuccess) {
  base::RunLoop loop;
  mojo::Remote<mojom::PaintPreviewCompositorCollection> compositor_collection =
      StartUtilityProcess();
  EXPECT_TRUE(compositor_collection.is_bound());
  EXPECT_TRUE(compositor_collection.is_connected());

  // If the compositor_collection hasn't crashed during initialization due to
  // lacking font support then this call should succeed.
  compositor_collection->ListCompositors(base::BindOnce(
      [](base::OnceClosure quit,
         const std::vector<base::UnguessableToken>& ids) {
        EXPECT_EQ(ids.size(), 0U);
        std::move(quit).Run();
      },
      loop.QuitClosure()));
  loop.Run();
}

}  // namespace paint_preview
