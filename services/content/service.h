// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CONTENT_SERVICE_H_
#define SERVICES_CONTENT_SERVICE_H_

#include <map>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"

namespace content {

class ServiceDelegate;
class NavigableContentsFactoryImpl;
class NavigableContentsImpl;

// The core implementation of the Content Service. This takes responsibility for
// owning top-level state for an instance of the service, binding incoming
// interface requests, etc.
//
// NOTE: This type is exposed to ServiceDelegate implementations outside
// of private Content Service code. The public API surface of this class should
// therefore remain as minimal as possible.
class Service {
 public:
  // |delegate| is not owned and must outlive |this|.
  explicit Service(ServiceDelegate* delegate);
  ~Service();

  ServiceDelegate* delegate() const { return delegate_; }

  void BindNavigableContentsFactory(
      mojo::PendingReceiver<mojom::NavigableContentsFactory> receiver);

  // Forces this instance of the Service to be terminated. Useful if the
  // delegate implementation encounters a scenario in which it can no longer
  // operate correctly. May delete |this|.
  void ForceQuit();

 private:
  friend class NavigableContentsFactoryImpl;
  friend class NavigableContentsImpl;

  void AddNavigableContentsFactory(
      std::unique_ptr<NavigableContentsFactoryImpl> factory);
  void RemoveNavigableContentsFactory(NavigableContentsFactoryImpl* factory);

  void AddNavigableContents(std::unique_ptr<NavigableContentsImpl> contents);
  void RemoveNavigableContents(NavigableContentsImpl* contents);

  ServiceDelegate* const delegate_;

  std::map<NavigableContentsFactoryImpl*,
           std::unique_ptr<NavigableContentsFactoryImpl>>
      navigable_contents_factories_;
  std::map<NavigableContentsImpl*, std::unique_ptr<NavigableContentsImpl>>
      navigable_contents_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace content

#endif  // SERVICES_CONTENT_SERVICE_H_
