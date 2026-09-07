# ドキュメントポータル

このディレクトリには、設計上の契約、実装手順、試験証跡を置きます。`README.md`はプロジェクトの入口、`develop_plan.md`は全体計画・再開記録、この文書は個別文書の入口です。

## 現在利用できる文書

| 文書 | 用途 |
| --- | --- |
| [documentation-structure.md](documentation-structure.md) | 文書体系の目標構成、不足文書、作成順、構成管理方針 |
| [camera_libcamera.md](camera_libcamera.md) | `mower_camera`のCSI/libcamera仕様、起動、診断、実機検証 |
| [camera_calibration.md](camera_calibration.md) | OV9281の較正手順、成果物、受入れ条件、実施記録 |
| [subsystems/localization/implementation-plan.md](subsystems/localization/implementation-plan.md) | VIOの実装順、依存関係、移行条件、未確定事項 |
| [subsystems/localization/specification.md](subsystems/localization/specification.md) | VIOの入力前提、状態、データ処理規則 |
| [architecture/ros-interfaces.md](architecture/ros-interfaces.md) | VIOを含むROS topic、QoS、TF責務 |
| [architecture/coordinate-frames.md](architecture/coordinate-frames.md) | 車体、カメラ、IMU、VIOの座標フレーム契約 |
| [architecture/time-synchronization.md](architecture/time-synchronization.md) | カメラ・IMU時刻源、変換、診断、受入れ値 |
| [verification/vio-acceptance-criteria.md](verification/vio-acceptance-criteria.md) | VIOの段階別ゲートと試験条件 |
| [verification/vio-synthetic-rosbag.md](verification/vio-synthetic-rosbag.md) | VIO合成rosbag2の生成契約、故障シナリオ、実行方法 |
| [../develop_plan.md](../develop_plan.md) | システム計画、設計方針、未完了事項、ハードウェア非依存の先行実装・試験トラック、実装再開記録 |
| [resume-memo.md](resume-memo.md) | 現在の作業ブランチ、実機検証結果、次回の再開手順 |

## 文書を追加・更新する際の原則

- 仕様（将来も守る契約）、手順（実施方法）、試験記録（実施した事実）を分けます。
- 実装と外部インターフェースが変わる場合は、同じ変更で仕様書を更新します。
- 安全、CAN、配線、校正値は、承認済みの版と試験記録を対応付けます。
- 新規文書は[目標構成](documentation-structure.md#目標ディレクトリ構成)の配置と命名に従います。
