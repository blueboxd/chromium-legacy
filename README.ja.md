\[ [en](README.md) | ja \]

# Chromium-legacy

公式でのサポートが終了しているMac OS X/OS X、

- Mac OS X 10.7 / Lion
- OS X 10.8 / Mountain Lion
- OS X 10.9 / Mavericks
- OS X 10.10 / Yosemite

向けの最新版Chromium (Chrome Canaryとほぼ同等)です。
一応10.11以降でも動作自体はしますが、一部機能が対応OS上であっても無効化されていることがあるため、特に理由がなければ公式ビルドの利用をお勧めします。

なお、一日2回(0:00/12:00)自動でビルドされたものが**テストなし**でアップロードされているため、更新内容とタイミングによっては起動しなかったり正常に動作しない可能性もあります。
日常用途としてはなるべく安定しているビルドを使い、頻繁なアップデートは避けた方がいいかもしれません。(なおChrome Canaryにおいても同様です)

## 機能

基本的に下記のようなOSが対応していない機能を除き、[同バージョンの公式Chromium](https://chromestatus.com/features)と同等です。
ソース、機能ともにオリジナルとなるべく差分が少なくなるよう再構築中です。

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
- DRM
  - 10.7/10.8ではDRM保護されたメディアは利用できません
  - 10.9以上では[Widevineライブラリをインストール](../../wiki/DRM)する必要があります
- WebAuthn/FIDO2
  - 10.7ではUSBキーの利用に[パッチを当てた `IOHIDFamily.kext`](../../../IOHIDFamily-368.21)が必要です

## ビルド

基本的に[公式手順](docs/mac_build_instructions.md)に従いますが、一部非サポートOS向けのパッチや手順が必要となります。

### 要件

- macOS 11.1 SDK
  - 10.7向けにビルドする場合、SDKの[NSArray.h](https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/97a23ba80d28005f6072053920d979be87213193/NSArray.h) と [NSDictionary.h](https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/97a23ba80d28005f6072053920d979be87213193/NSDictionary.h)にパッチが必要
- Xcode 12.2+
  - 10.7向けの場合、SDKや同梱のclangではなく[パッチを当てたclang](../../../llvm-project)が必要
- 強めのCPU
  - `Ryzen 9 5950X`1台と`Ryzen 9 3950X`を2台の分散ビルドでフルビルドに40分程度
  - `Core i9-9980HK`単体でフルビルドに3-4時間以上

### TL;DR

```bash
curl https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/f02e2154acecd5076293e70baf27c97dcc8f6272/build.sh | bash
```

### 手順

環境構築とビルド:

```bash
# setup working dir
mkdir -pv chromium-project && cd chromium-project

# prepare patched clang
mkdir -pv clang-master && pushd clang-master
curl -LOJ https://github.com/blueboxd/llvm-project/releases/download/main/main.tar.xz
tar xvf main.tar.xz
popd

# setup depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git && export PATH=`pwd`/depot_tools:"$PATH"

# setup project dir
mkdir -pv chromium-legacy && cd chromium-legacy
# place this repo as src
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/97a23ba80d28005f6072053920d979be87213193/.gclient

# checkout sources & dependencies
gclient sync -v --no-history

# setup patched skia
pushd src/third_party/skia
git remote add for-lion https://github.com/blueboxd/skia.git
git fetch for-lion && git checkout for-lion && git checkout for-lion -- .
popd

# setup patch for v8
pushd src/v8/src
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/97a23ba80d28005f6072053920d979be87213193/sp_mut.cc
cd ../
cat BUILD.gn | sed -e 's#"src/wasm/wasm-code-manager.h",#"src/wasm/wasm-code-manager.h",\'$'\n    "src/sp_mut.cc",#g' > BUILD.gn.tmp && mv -fv BUILD.gn.tmp BUILD.gn
popd

# setup patch for libANGLE
pushd src/third_party/angle/src/libANGLE/renderer/
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/65ba4558a17eb47feb38729a87b8d8976d5bb8ad/driver_utils_mac.mm
cd metal
cat BUILD.gn| sed -e 's#"QuartzCore.framework",#"QuartzCore.framework",\'$'\n        "CoreServices.framework",#g'  > BUILD.gn.tmp && mv -fv BUILD.gn.tmp BUILD.gn
popd

# setup out dir
mkdir -pv out/release
pushd out/release
# setup args.gn with basic parameters
curl -OJ https://gist.githubusercontent.com/blueboxd/c1f355fb6fe829e98ff5453880683993/raw/1688a9f9bd71d90b6e83b66836cf16e3796ec6c3/args.gn
popd

# use patched clang
pushd ../clang-master
echo "clang_base_path=\"`pwd`\"" >> ../chromium-legacy/out/release/args.gn
popd

# generate build files
cd src
gn gen ../out/release

# build
time ninja -C ../out/release chrome

# now your build is ready
open -R ../out/release/Chromium.app
```

`src`更新:

```bash
cd chromium-project/chromium-legacy/src
git pull
cd thrid_party/skia
git checkout origin/master -- .
cd ../../
gclient sync -D
cd thrid_party/skia
git fetch for-lion && git checkout for-lion && git checkout for-lion -- .
```

リビルド:

```bash
cd chromium-project/chromium-legacy/src
ninja -C ../out/release chrome
```
