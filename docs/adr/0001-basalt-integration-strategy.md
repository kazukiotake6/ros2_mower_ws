# ADR 0001: Basalt統合方式

- 状態: 技術検証待ち
- 日付: 2026-08-19

## 文脈

VIO第一候補はBSD-3-ClauseのBasaltである。上流はCMakeとvcpkgによるライブラリ・CLIビルドを提供するが、本ワークスペースが必要とするROS 2 Jazzyノード、topic契約、lifecycle、diagnosticsはローカルで実装する必要がある。third-party依存のライセンスとPi 5 arm64の再現可能ビルドも未確認である。

## 暫定決定

- Basalt本体を`mower_localization`へ直接コピーしない。
- センサー検証、時刻キュー、ROS変換、状態管理をBasalt非依存のコードとして実装する。
- Basalt依存は将来のvendorパッケージまたは外部インストールへ隔離し、`mower_localization`は薄い推定器interfaceを介して利用する。
- 評価候補は上流リリース0.1.7とするが、x86_64、Pi 5 arm64、CI、ライセンス確認が終わるまで採用版として確定しない。
- 実機入力より先に公開データセットとrosbag2再生でadapterを検証する。

## 採用確定条件

- GUIなし構成がUbuntu 24.04のx86_64とarm64で再現可能にビルドできる。
- VIOに必要なlibrary APIと追跡品質情報を安定して取得できる。
- ROS 2依存、OpenCV、Eigen、TBB等との版衝突を解決できる。
- Basaltと全third-party依存のライセンス、配布物、SBOMをレビューできる。
- Pi 5の性能試験を実施できる。

条件を満たさない場合はOKVISとの比較ADRを追加し、採用判断を更新する。
