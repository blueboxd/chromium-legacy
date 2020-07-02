// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_HANDWRITING_RECOGNIZER_MANAGER_H_
#define CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_HANDWRITING_RECOGNIZER_MANAGER_H_

#include "chromeos/services/machine_learning/public/mojom/handwriting_recognizer_requestor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace machine_learning {

// TODO(916760): This class is a temporary fix until the work in crbug/916760
// is completed. Once completed this class should be removed, along with
// mojom::HandwritingRecognizerRequestor.
class HandwritingRecognizerManager
    : public mojom::HandwritingRecognizerRequestor {
 public:
  HandwritingRecognizerManager();
  ~HandwritingRecognizerManager() override;

  static HandwritingRecognizerManager* GetInstance();

  void AddReceiver(
      mojo::PendingReceiver<mojom::HandwritingRecognizerRequestor> receiver);

 private:
  void LoadGestureModel(
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      LoadHandwritingModelCallback callback) override;

  void LoadHandwritingModel(
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      LoadHandwritingModelCallback callback) override;

  mojo::ReceiverSet<mojom::HandwritingRecognizerRequestor> receivers_;

  DISALLOW_COPY_AND_ASSIGN(HandwritingRecognizerManager);
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_HANDWRITING_RECOGNIZER_MANAGER_H_
