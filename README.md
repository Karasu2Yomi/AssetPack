# AssetPack（Puzzle Editor）

Object_Connect の `levels.csv` / `nodes.csv` / map csv を編集するための
Windows 向けエディタ。

## 目的

- TileMap JSON/BIN/Diff 機能を廃止
- 日本語 UI で `levels`、`nodes`、map ノードを編集
- `data/levels.csv` で複数の `data/maps/<level>.csv` を管理
- 手動の「すべて保存」（`Ctrl+S`）のみ

## 起動データ

起動後、左パネルの「データフォルダを開く」から
`levels.csv` と `nodes.csv` を持つ `data` フォルダを選択。

- `resourceRoot = data.parent_path()`
- `levelsPath = data/levels.csv`
- `nodesPath = data/nodes.csv`
- map 参照は `map_path`（resourceRoot からの相対）

## マップの編集と保存

- 新規・複製レベルのマップはメモリ上の下書きとして作成。保存前でも編集でき、
  「すべて保存」または `Ctrl+S` を実行するまでファイルやマップ用フォルダは作成しない。
- レベルを選択 → ノードひな形を選択 →「マップへ追加」→ キャンバスをクリックして配置。
  ひな形を選択しても現在のレベルとキャンバスは維持される。
- 既存レベルは `map_path` のファイルを読み込む。ファイルが見つからない場合は
  警告付きの空の修復用下書きを開き、保存時にそのパスへ作成する。
  存在するファイルの読み取り・CSV 形式エラーは空のマップに置き換えない。
- 保存失敗時は編集内容を保持する。下書きの保存先に別のファイルが出現した場合は
  上書きせず、問題一覧に競合を表示する。
- `data` 以外の名前のフォルダを開いた場合も、そのフォルダ内の `maps/` を使用する。
- 画面の項目名、操作案内、問題一覧は日本語で表示する。ノードの種類は
  「始点／中継点／終点／障害物」と表示し、CSV 内の値は従来どおり保持する。

## コア構成

- `assetpack_core`
  - `CsvCodec`：CSV parse / serialize（RFC 4180, CRLF）
  - `PuzzleProjectStore`：LoadDataFolder / SaveAll
  - `PuzzleResolver`：preset 継承解決
  - `Model`：`PuzzleProject / LevelRow / NodePresetRow / MapNodeRow`

## ビルド

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## 実行ファイル

- `build/bin/AssetPack.exe`

## 別の Windows PC へ配布

MinGW ビルドでは GCC、C++ 標準ライブラリ、winpthreads の実行時コードを
EXE に静的リンクするため、受け取る PC に MinGW をインストールする必要はない。
ビルド後に EXE の依存関係を検査し、`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、
`libwinpthread-1.dll` への依存が残っていればビルドを失敗させる。

配布には最新の `build/bin/AssetPack.exe` を使用する。
ゲームのデータ・画像は別途リソースフォルダとして渡し、起動後にその中の
`data` フォルダを開く。以前にコピーした EXE は、新しくビルドしたものに差し替える。
