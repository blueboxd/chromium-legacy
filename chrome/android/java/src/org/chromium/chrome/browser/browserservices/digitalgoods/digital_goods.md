# Digital Goods

Websites viewed in a Trusted Web Activity can use the Digital Goods API to
communicate with their corresponding APK to query information about products
that the website wants to sell to the user.
This is required for Digital Goods because according to Play Store policy,
all Digital Goods sales on Android must use Play Billing, so the Play Billing
Android APIs are the source of truth for information such as price.

## Code

* `DigitalGoodsImpl` implements the `DigitalGoods` mojo API and is where
  requests come from. It is created by the `DigitalGoodsFactory`.
* `TrustedWebActivityClient` is the class that talks to Trusted Web Activities.
* `DigitalGoodsAdapter` sits between `DigitalGoodsImpl` and
  `TrustedWebActivityClient`, transforming between appropriate data types.
* `DigitalGoodsConverter` contains the lower level transformations that
  `DigitalGoodsAdapter` uses.
