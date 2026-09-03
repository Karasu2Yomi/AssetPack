# AssetPack（Puzzle Editor）

Object_Connect の `levels.csv` / `nodes.csv` / map csv を編集するための
Windows 向けエディタ。

## 目的

- TileMap JSON/BIN/Diff 機能を廃止
- 日本語 UI で `levels`、`nodes`、map ノードを編集
- `data/levels.csv` で複数の `data/maps/<level>.csv` を管理
- 手動 SaveAll（`Ctrl+S`）のみ

## 起動データ

起動後、左パネルの「データフォルダを開く」から
`levels.csv` と `nodes.csv` を持つ `data` フォルダを選択。

- `resourceRoot = data.parent_path()`
- `levelsPath = data/levels.csv`
- `nodesPath = data/nodes.csv`
- map 参照は `map_path`（resourceRoot からの相対）

## マップの編集と保存

- 新規・複製レベルのマップはメモリ上の下書きとして作成。保存前でも編集でき、
  `Save All` または `Ctrl+S` を実行するまでファイルやマップ用フォルダは作成しない。
- レベルを選択 → ノードひな形を選択 →「マップへ追加」→ キャンバスをクリックして配置。
  ひな形を選択しても現在のレベルとキャンバスは維持される。
- 既存レベルは `map_path` のファイルを読み込む。ファイルが見つからない場合は
  警告付きの空の修復用下書きを開き、保存時にそのパスへ作成する。
  存在するファイルの読み取り・CSV 形式エラーは空のマップに置き換えない。
- 保存失敗時は編集内容を保持する。下書きの保存先に別のファイルが出現した場合は
  上書きせず、問題一覧に競合を表示する。
- `data` 以外の名前のフォルダを開いた場合も、そのフォルダ内の `maps/` を使用する。

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
