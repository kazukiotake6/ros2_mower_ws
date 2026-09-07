# Basaltオフライン技術検証

## 目的と判定範囲

Basalt候補版を実機センサーなしで取得し、固定ソースとx86_64依存解決を再現できることを確認する。本検証はVIO精度、Pi 5実時間性、arm64採用可否、実機較正および最終採用判定を含まない。

## 検証基線

| 項目 | 固定値 |
| --- | --- |
| upstream | `https://gitlab.com/VladyslavUsenko/basalt.git` |
| tag | `0.1.7` |
| commit | `6d8637b9d68ea18a1a63c1baa72818779e156932` |
| vcpkg submodule | `1e199d32ad53aab1defda61ce41c380302e3f95c` |
| vcpkg registry baseline | `05442024c3fda64320bd25d2251cc9807b84fb6f` |
| Basalt license | BSD-3-Clause。third-partyは個別確認が必要 |

機械可読な正は`config/dependencies/basalt-0.1.7.json`とする。タグだけでなくcommitと依存基線を検査し、上流差分を暗黙に取り込まない。

## 自動検証

`Basalt offline verification` workflowは次を行う。

1. 固定タグを一時領域へ取得し、commit、vcpkg submodule、registry baseline、主要依存、LICENSE、CLI契約を検査する。
2. Ubuntu 24.04 x86_64でvcpkgをbootstrapし、`CXX_MARCH=generic`、動的リンク設定でCMake configureと全依存解決を行う。

ローカルのソース検査は次のコマンドで再現できる。

```bash
python3 scripts/verify_basalt_source.py \
  --checkout /path/to/basalt \
  --baseline config/dependencies/basalt-0.1.7.json
```

浅いvcpkg cloneにはmanifestが指定するregistry baselineが含まれないため、configure前にそのcommitを明示的にfetchする。

## 判明した制約

- `basalt_vio`は`--show-gui 0`で画面表示を無効化できる。
- 一方、候補版はCMake configure時にPangolinを無条件に要求する。このため「ヘッドレス実行」は可能だが「GUI依存なしビルド」は未達である。
- manifestはOpenCV、TBB、Boostに加えてPangolin、RealSense、ROS 1 rosbag等を含む。ROS 2ワークスペースとの版競合と配布ライセンスは引き続き評価する。
- 上流CMake presetは`CXX_MARCH=native`を既定とする。再配布可能な成果物とCIでは`generic`へ上書きし、Pi 5向け最適化はarm64実測後に別プロファイルで決める。

## 次のゲート

公開EuRoCデータセット1系列を固定checksumで取得し、`--show-gui 0`でtrajectoryと実行統計を保存する。データセットの配布条件、期待出力、評価スクリプトを固定してからCIへ追加する。その後、third-partyライセンス一覧、SBOM、ROS 2 adapterに必要なAPIと追跡品質情報、Pi 5 arm64ビルドを確認する。
