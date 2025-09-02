# OpenCVへの移行とWebP専用化計画

## 概要
- 変換ライブラリをSDL/libwebp/libavifからOpenCVに変更
- WebP以外の変換（AVIF、SVG変換）を削除し、WebPのみサポートする
- 画像リサイズアルゴリズムをLanczosに統一する

## 現在の構成
- C++ソース: `src/libImage.cpp`
- 使用ライブラリ: SDL2/SDL3、libwebp、libavif、libexif、lunasvg
- サポート形式: PNG、JPG、WebP、SVG、AVIF
- ビルド方法: EmscriptenでWASM化

## 変更計画

### 1. ライブラリの置き換え
- [x] SDL2/SDL3の依存関係を削除
- [x] libavifの依存関係を削除  
- [x] lunasvgの依存関係を削除
- [x] libexifの依存関係を削除
- [x] OpenCVのEmscripten対応版を導入

### 2. C++ソースコードの変更 (`src/libImage.cpp`)
- [x] SDLベースの画像処理コードをOpenCVに置き換え
- [x] AVIF関連のコードを削除
- [x] SVG関連のコードを削除
- [x] WebP変換のみに特化したAPIに変更
- [x] OpenCVの`cv::imread`、`cv::imwrite`を使用した実装に変更
- [x] 画像リサイズをLanczosアルゴリズム（`cv::INTER_LANCZOS4`）に統一

### 3. TypeScript型定義の更新
- [x] `src/cjs/libImage.d.ts`の更新
- [x] `src/esm/libImage.d.ts`の更新
- [x] AVIF、SVG関連の型定義を削除
- [x] WebP専用の型定義に簡略化

### 4. ビルドシステムの更新
- [x] `Makefile`の更新
  - SDL、libavif、lunasvgのビルド設定を削除
  - OpenCVのビルド設定を追加
  - WebPライブラリのみのビルドに変更
- [x] `docker/Dockerfile`の更新
  - SDL、libavifなどの不要なライブラリを削除
  - OpenCVのインストールを追加

### 5. パッケージ設定の更新
- [x] `package.json`の説明文を更新
- [x] `README.md`の更新
  - サポート形式の説明をWebPのみに変更
  - OpenCV使用について記載

### 6. テストファイルの更新
- [x] `images/`ディレクトリのテスト画像
  - `test03.avif`を削除
  - `test04.svg`、`test05.svg`を削除
- [ ] テストコードの更新

### 7. API変更による影響
- [x] 各エントリーポイントの確認と更新
  - `src/lib/optimizeImage.ts`
  - `src/esm/index.ts` (要確認)
  - `src/next/index.ts` (要確認)
  - `src/node/index.ts` (要確認)
  - `src/vite/index.ts` (要確認)
  - その他のworkerファイル (要確認)

## 技術的考慮事項

### OpenCVの導入
- Emscriptenで利用可能なOpenCV.jsを使用
- WebAssemblyでのパフォーマンス最適化
- メモリ管理の効率化
- Lanczosリサイズアルゴリズムの品質向上

### WebP専用化のメリット
- ビルドサイズの削減
- 依存関係の簡略化
- メンテナンス性の向上

### Lanczosアルゴリズムの採用
- 高品質な画像リサイズを実現
- OpenCVの`cv::INTER_LANCZOS4`を使用
- エイリアシングを効果的に抑制
- 既存のSDL_rotozoomからの品質向上

### 互換性の注意点
- 既存のAVIF、SVG変換を使用している利用者への影響
- APIの後方互換性の確保（必要に応じて）

## 実装順序
1. ビルドシステムの準備（Docker、Makefile）
2. C++コアライブラリの書き換え
3. TypeScript型定義の更新
4. 各エントリーポイントの更新
5. テストとドキュメントの更新

## リスク評価
- **高**: OpenCVへの移行によるパフォーマンス変化
- **中**: 既存ユーザーへの破壊的変更
- **中**: Lanczosアルゴリズムによる処理時間の増加
- **低**: WebP変換の品質維持

## 完了条件
- [ ] 全てのビルドが成功する
- [ ] WebP変換が正常に動作する
- [ ] Lanczosアルゴリズムによるリサイズが正常に動作する
- [ ] パッケージサイズが削減される
- [ ] テストが全て通過する
- [ ] ドキュメントが更新される
