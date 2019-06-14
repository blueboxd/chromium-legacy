// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_KIOSK_NEXT_BROWSER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_KIOSK_NEXT_BROWSER_FACTORY_H_

#include <list>
#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "url/gurl.h"

class Browser;

// This class keeps a list of Browser instances along with their corresponding
// launch URLs.
class KioskNextBrowserList : public BrowserListObserver {
 public:
  // This struct keeps a browser instance and its associated launch url from
  // kiosk next home.
  struct Entry {
    Entry(Browser* browser, const GURL& launch_url);

    // Unowned. Owned by Views hierarchy (BrowserView).
    Browser* const browser;

    const GURL launch_url;
  };

  typedef std::list<std::unique_ptr<Entry>>::iterator Iterator;
  typedef std::list<std::unique_ptr<Entry>>::const_iterator ConstIterator;

  // Returns a pointer to the KioskNextBrowserList instance owned by
  // KioskNextBrowserFactory.
  static const KioskNextBrowserList& Get();

  // This method is called by KioskNextBrowserFactory to check if the upper
  // limit of number of Browsers has been reached.
  bool CanAddBrowser() const;

  // Checks if the browser instance is in the list of browsers created by
  // KioskNextBrowserFactory.
  bool IsKioskNextBrowser(const Browser* browser) const;

  // Returns the corresponding browser if there exists a Browser instance
  // that was launched with the particular URL.
  Browser* GetBrowserForWebsite(const GURL& url) const;

  // Returns the corresponding Entry if browser is a kiosk next browser.
  // Otherwise, return an Entry with nullptr for its |browser|.
  const Entry* GetBrowserEntry(const Browser* browser) const;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;

 private:
  friend class KioskNextBrowserFactory;

  explicit KioskNextBrowserList(size_t max_browsers_allowed);
  ~KioskNextBrowserList() override;

  // Called by KioskNextBrowserFactory to add a new browser instance along with
  // its launch url.
  void AddBrowser(Browser* browser, const GURL& url);

  // Called by KioskNextBrowserFactory when CanAddBrowser() returns false. It
  // removes the LRU browser to allow the factory class to continue creating.
  void RemoveLRUBrowser();

  Iterator FindBrowser(const Browser* browser);
  ConstIterator FindBrowser(const Browser* browser) const;

  size_t max_browsers_allowed_;
  std::list<std::unique_ptr<Entry>> lru_browsers_;

  DISALLOW_COPY_AND_ASSIGN(KioskNextBrowserList);
};

class KioskNextBrowserFactory {
 public:
  // Returns the instance already created for this singleton. Creates a new
  // instance if one hasn't already been created yet.
  static KioskNextBrowserFactory* Get();

  ~KioskNextBrowserFactory();

  Browser* CreateForWebsite(const GURL& url);

  const KioskNextBrowserList& kiosk_next_browser_list() const {
    return browser_list_;
  }

 private:
  friend class KioskNextBrowserFactoryTest;
  friend class base::NoDestructor<KioskNextBrowserFactory>;

  explicit KioskNextBrowserFactory(size_t max_browsers_allowed);

  KioskNextBrowserList browser_list_;

  DISALLOW_COPY_AND_ASSIGN(KioskNextBrowserFactory);
};

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_KIOSK_NEXT_BROWSER_FACTORY_H_
