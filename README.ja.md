\[ [en](README.md) | ja \]

# Chromium-legacy

公式でのサポートが終了しているMac OS X (10.7/10.8/10.9/10.10)向けの最新版Chromium (Chrome Canaryとほぼ同等)です。
10.11以降でも動作自体はしますが、一部機能が対応OS上であっても無効化されていることがあるため、特に理由がなければ公式ビルドの利用をお勧めします。

なお、一日2回(0:00/12:00)自動でビルドされたものが**テストなし**でアップロードされているため、Chromiumの更新状況とタイミングによっては起動しなかったり正常に動作しない可能性もあります。
日常用途としてはなるべく安定しているビルドを使い、頻繁なアップデートは避けた方がいいかもしれません。

## 機能

基本的に下記のようなOSが対応していない機能を除き同じバージョンの公式Chromiumと同等です。
ソース、機能ともに公式のものとなるべく差分が少なくなるよう再構築中です。

## 制限

- UI
  - ウインドウ
    - 10.9以下に対応した統一されたタイトルバーとタブバーの移植ができていないため、従来のタイトルバーと別個のタブバーという構成になっています
      - Chrome 49の頃のUIをforward-portできないか調査中です
  - メニュー/シート
    - 影が表示されず、背景と区別しづらい
      - 10.9以下では透明なwindowの背景をfillしないとダメなようですが、調査中です
  - スクロールバー
    - “スクロール時に表示”オプション時でもスクロールバーが消えない
      - 下記のGPU Compotisiting無効化の影響のようです
- GPU
  - 10.7ではOpenGLが古すぎるためGPUは利用できず、すべてソフトウェアレンダリングやソフトウェアデコードとなります
  - 10.8/10.9では描画に問題が出るため`--disable-gpu-compositing`オプションによってGPU Compotisitingが無効化されています
- サンドボックス
  - 10.9以下ではポリシが新しすぎてロードできないため`--no-sandbox`オプションで無効化されています
    - 完全無効化ではなくポリシ側を書き直してロードできないか調査中です

## ビルド

基本的に[公式手順](docs/mac_build_instructions.md)に従いますが、一部非サポートOS向けのパッチや手順が必要となります。

### 要件

- macOS 11.1 SDK
- Xcode 12.2+
- 強めのCPU
  - `Xeon E5-2690 v4`1台と`Ryzen 9 3950X`を2台の分散ビルドでフルビルドに40分程度
  - `Core i9-9980HK`単体でフルビルドに3-4時間

### TL;DR

手順とかいいからとりあえずビルドしたい向き
(なお簡略化のため10.7/10.8サポートなし)

環境構築とビルド:

```bash
mkdir chromium-project && cd chromium-project
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=`pwd`/depot_tools:"$PATH"
mkdir chromium-legacy && cd chromium-legacy
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/bc158ab43c8766611b9d67b17d460f36f033bc9c/.gclient && gclient sync -D
mkdir -p out/release && cd out/release
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/bc158ab43c8766611b9d67b17d460f36f033bc9c/args.gn
cd ../../src 
gn gen ../out/release
ninja -C ../out/release chrome
```

`/src`更新:

```bash
cd chromium-project/chromium-legacy/src
git pull
gclient sync -D
```

リビルド:

```bash
cd chromium-project/chromium-legacy/src
ninja -C ../out/release chrome
```

### step-by-step

(TBW)

---

# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium

Chromium is an open-source browser project that aims to build a safer, faster,
and more stable way for all users to experience the web.

The project's web site is https://www.chromium.org.

To check out the source code locally, don't use `git clone`! Instead,
follow [the instructions on how to get the code](docs/get_the_code.md).

Documentation in the source is rooted in [docs/README.md](docs/README.md).

Learn how to [Get Around the Chromium Source Code Directory Structure
](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code).

For historical reasons, there are some small top level directories. Now the
guidance is that new top level directories are for product (e.g. Chrome,
Android WebView, Ash). Even if these products have multiple executables, the
code should be in subdirectories of the product.
