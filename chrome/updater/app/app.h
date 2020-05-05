// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_H_
#define CHROME_UPDATER_APP_APP_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"

namespace updater {

// Creates a ref-counted singleton instance of the type T. Use this function
// to get instances of classes derived from updater::App.
template <typename T>
scoped_refptr<T> AppInstance() {
  static base::NoDestructor<scoped_refptr<T>> instance{
      base::MakeRefCounted<T>()};
  return *instance;
}

// An App is an abstract class used as a main processing mode for the updater.
// Instances of classes derived from App must be accessed as singletons by
// calling the function template AppInstance<T>.
class App : public base::RefCountedThreadSafe<App> {
 public:
  // Starts the thread pool and task executor, then runs a runloop on the main
  // sequence until Shutdown() is called. Returns the exit code for the
  // program.
  int Run();

 protected:
  friend class base::RefCountedThreadSafe<App>;

  static constexpr base::StringPiece kThreadPoolName = "Updater";

  App();
  virtual ~App();

  // Triggers program shutdown. Must be called on the main sequence. The program
  // will exit with the specified code.
  void Shutdown(int exit_code);

 private:
  // Allows initialization of the thread pool for specific environments, in
  // cases where the thread pool must be started with different init parameters,
  // such as MTA for Windows COM servers.
  virtual void InitializeThreadPool();

  // Implementations of App can override this to perform work on the main
  // sequence while blocking is still allowed.
  virtual void Initialize() {}

  // Called on the main sequence while blocking is allowed and before
  // shutting down the thread pool.
  virtual void Uninitialize() {}

  // Concrete implementations of App can execute their first task in this
  // method. It is called on the main sequence. Blocking is not allowed. It may
  // call Shutdown.
  virtual void FirstTaskRun() = 0;

  // A callback that quits the main sequence runloop.
  base::OnceCallback<void(int)> quit_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_H_
