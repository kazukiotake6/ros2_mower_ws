# 文書体系・構成管理の目標構成

## 目的

この文書は、ROS 2芝刈りロボットの設計、実装、実機運用、検証を再現可能にするための文書体系を定義します。現時点ではカメラ関連文書と全体計画が中心であり、以下は不足文書を計画的に追加するための正とします。

安全認証そのものを満たすことは目的に含みません。ただし、安全要求、設計根拠、検証証跡を追跡できる形で管理します。

## 現状と不足

現在追跡されている文書は、ルートの`README.md`、`develop_plan.md`、カメラ仕様、カメラ較正手順です。`develop_plan.md`にはCANプロトコル文書の作成が要求されていますが、`docs/can_protocol.md`は未作成です。

優先して追加する文書は次の5件です。

1. `interfaces/can-protocol.md` — CAN ID、方向、周期、DLC、バイト配置、単位、CRC、timeout、版互換性。
2. `safety/safety-requirements.md` — 安全要求、Pi/MCU/E-stopへの責務割当て、禁止事項。
3. `hardware/wiring-and-pinout.md` — GPIO/SPI/CSI/CAN、電圧整合、終端、電源・配線確認。
4. `configuration-management/plan.md` — 管理対象、ベースライン、変更承認、試験時の記録項目。
5. `verification/test-strategy.md` — 単体、統合、SIL、HIL、実機の試験範囲と合否判定。

これらが承認されるまで、CAN実車指令、最終安全停止の変更、未確定の配線を前提とする実装を固定しません。

## 目標ディレクトリ構成

```text
README.md                              # プロジェクト入口
AGENTS.md                              # 実装エージェントの必読事項
CONTRIBUTING.md                        # 開発・レビュー・文書更新規約
CHANGELOG.md                           # リリース差分
SECURITY.md                            # 脆弱性・安全上の報告手順
docs/
├── README.md                          # 文書ポータル
├── documentation-structure.md         # 本文書
├── architecture/
│   ├── system-overview.md             # Pi、MCU、E-stopを含む全体構成
│   ├── responsibility-boundaries.md   # 非安全系と安全系の境界
│   ├── ros-interfaces.md              # node/topic/service/action/QoS
│   ├── coordinate-frames.md           # TF、単位、軸向き
│   └── time-synchronization.md        # 時刻源、精度、許容ジッタ
├── hardware/
│   ├── hardware-baseline.md           # BOM、ハードウェア版、対象機材
│   ├── wiring-and-pinout.md           # 配線、GPIO、電圧、終端
│   ├── power-and-protection.md        # 電源、保護、接地、EMC/ESD
│   └── assembly-inspection.md         # 組立て・起動前確認
├── interfaces/
│   ├── can-protocol.md                # Pi-MCU CANプロトコル
│   ├── mcu-state-machine.md           # MCU状態と遷移
│   ├── control-interface.md           # 速度・停止・許可のROS/CAN契約
│   └── diagnostics-interface.md       # 故障コード、診断、ログ項目
├── subsystems/
│   ├── camera/{specification,calibration,acceptance-test}.md
│   ├── imu/{specification,calibration}.md
│   ├── localization/specification.md
│   ├── navigation/specification.md
│   └── simulation/hil-specification.md
├── safety/
│   ├── safety-requirements.md         # 安全要求と責務割当て
│   ├── hazard-analysis.md             # 危険源、対策、残存リスク
│   ├── safe-state-and-recovery.md     # 停止、通信断、復帰
│   └── safety-verification.md         # 安全要求の検証
├── operations/
│   ├── development-setup.md
│   ├── deployment.md
│   ├── calibration-operation.md
│   ├── troubleshooting.md
│   └── logging-and-data-handling.md
├── verification/
│   ├── test-strategy.md
│   ├── acceptance-criteria.md
│   ├── traceability-matrix.md         # 要求→設計→試験→結果
│   └── test-records/                  # 版付き試験記録
├── configuration-management/
│   ├── plan.md
│   ├── versioning-and-release.md
│   ├── change-control.md
│   ├── dependency-baseline.md
│   ├── calibration-data.md
│   └── sbom-and-licenses.md
├── adr/                               # Architecture Decision Record
└── templates/                         # 試験・変更・校正の記録テンプレート
config/
├── profiles/                          # 開発・実機用の追跡対象プロファイル
├── hardware/                          # 機体・センサー構成
└── calibration/                       # 承認済みの個体別校正値
```

既存の`docs/camera_libcamera.md`と`docs/camera_calibration.md`は移行完了まで維持します。移行時はリンク切れを作らず、旧パスには移行先への案内を残します。

## 文書種別と責務

| 種別 | 記載する内容 | 更新の契機 |
| --- | --- | --- |
| 仕様 | 外部契約、制約、状態、単位、合否条件 | 実装・インターフェース変更の同一PR |
| 手順 | セットアップ、較正、導入、復旧の実施方法 | 実施手順・依存関係の変更 |
| 試験計画 | 範囲、環境、期待値、合否判定 | 要求・設計・リスクの変更 |
| 試験記録 | 実施日時、構成、結果、未解決事項 | 各実機/HIL評価の完了時 |
| ADR | 比較した選択肢、決定、理由、影響 | 変更が長期的な設計判断を伴う場合 |

## 構成管理の最小要件

`configuration-management/plan.md`を作成するまでの暫定規則として、次を適用します。

- 管理対象はソース、ROSパッケージ、Dockerイメージ、OS/ROS/libcamera依存関係、MCUファームウェア、回路・BOM、設定、校正値、試験手順・結果です。
- 実機/HIL試験では、Gitコミット、Pi OS、ROS、libcamera、MCUファームウェア、ハードウェア版、設定プロファイル、校正ファイルの識別子を記録します。
- CAN、配線、停止状態、校正値の変更はレビュー対象とし、互換性と再試験要否を明示します。
- 校正値は機体ID、カメラID、解像度、取付状態、作成日、手順版、承認者、有効条件を識別できるようにします。
- rosbag2などの大容量データはGit管理しません。保管先・保持期間・試験記録からの参照方法を運用文書に記載します。
- CIにはビルド・テストに加え、Markdownリンク検査、設定YAML検証、CAN仕様と実装の整合確認を順次追加します。

## 追加順と完了条件

1. 優先5文書を作成し、`AGENTS.md`の将来参照先を実在する文書へ更新する。
2. CAN・安全・配線のレビューを終え、試験可能なベースラインをタグ付けする。
3. 各サブシステム仕様、ROSインターフェース、運用手順を追加する。
4. 試験計画・試験記録・トレーサビリティを導入し、HILと実機評価へ適用する。
5. リリース、SBOM、ライセンス、依存関係のベースラインを整備する。
