# VIO合成rosbag2再生試験

## 目的

実センサーを使わず、VIO入力検証とlifecycleノードの異常状態を決定論的に確認する。
この試験は実環境の精度、ドリフト、追跡性能および外部MCUの安全機能を検証しない。

## 生成契約

- シナリオは`src/mower_localization/config/vio_test_scenarios.yaml`で版管理する。
- 生成器は同じcatalog version、scenario、seedから同じ論理メッセージ列を生成する。
- 出力先が既に存在する場合は上書きせず失敗する。
- bagには`/image_raw`、`/camera_info`、`/imu/data_raw`だけを格納する。
- 生成したbag内の`vio_test_manifest.json`にscenario、fault、seed、topic別件数、期待状態を記録する。
- `.db3`および生成済みbagはGitへ追加しない。

## 自動試験シナリオ

| シナリオ | 注入内容 | 観測する状態 |
| --- | --- | --- |
| `normal` | 単調増加する整合済み入力 | `INITIALIZING` |
| `camera_info_mismatch` | 画像とCameraInfoの幅不一致 | `DEGRADED` |
| `imu_gap` | 許容値を超えるIMU間隔 | `DEGRADED` |
| `imu_duplicate` | IMU stampの重複 | `DEGRADED` |
| `image_time_jump` | 画像stampの逆行 | `DEGRADED` |
| `camera_stop` | IMU継続中に画像系列を停止 | `LOST` |

`normal`が`INITIALIZING`に留まるのは、既定adapterが実VIO結果を生成しないためである。
全シナリオで根拠のない`/vio/odometry`がPublishされないことも確認する。

## 実行

```bash
source /opt/ros/jazzy/setup.bash
colcon build --packages-select mower_localization --symlink-install
source install/setup.bash
colcon test --packages-select mower_localization
colcon test-result --test-result-base build/mower_localization --verbose
```

個別bagは`generate_vio_test_bag --catalog <catalog> --scenario <name> --output <path>`で生成できる。
CI試験は一時領域へ生成して`ros2 bag play`で再生し、終了後に破棄する。

## 未実装シナリオ

低FPS、IMU逆行、入力順序乱れ、倍率別再生、ノード再起動と復旧判定は後続作業とする。
実VIO adapter接続後に、同一入力に対する推定結果の再現性を別ゲートで追加する。
