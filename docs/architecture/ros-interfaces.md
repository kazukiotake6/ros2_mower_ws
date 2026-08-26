# ROS 2インターフェース

## VIO初期実装

| 方向 | 名前 | 型 | 契約 |
| --- | --- | --- | --- |
| Subscribe | `/image_raw` | `sensor_msgs/msg/Image` | 取得時刻、`camera_optical_frame`、承認済み画像形式 |
| Subscribe | `/camera_info` | `sensor_msgs/msg/CameraInfo` | Imageとstamp・frame・サイズが一致 |
| Subscribe | `/imu/data_raw` | `sensor_msgs/msg/Imu` | SI単位、`imu_link`、orientation未設定可 |
| Publish | `/vio/odometry` | `nav_msgs/msg/Odometry` | `vio_odom`基準の`base_link`推定。TRACKING時のみ正常出力 |
| Publish | `/vio/status` | 初期段階は`diagnostic_msgs/msg/DiagnosticStatus` | 状態、理由、入力age、drop、処理遅延、追跡品質 |
| Publish | `/diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | VIOおよび入力健全性 |

## QoS

- ImageとImuはSensor Data QoSを基準とする。
- CameraInfoは対応するImageを復元できるQoSとする。
- statusとdiagnosticsは状態監視側が直近値を取得できるQoSを選定し、結合試験で確認する。

## TF責務

- `mower_description`が固定変換を発行する。
- VIO単体評価時だけ、`basalt_vio_node`が動的な`vio_odom -> base_link`を発行できる。
- 車輪融合導入後は融合ノードを`odom -> base_link`の唯一の発行者とし、VIOノードのTF出力を無効化する。
- 同じ親子frameのTFを複数ノードが発行しない。

## 上位停止要求との境界

VIOは品質状態と診断を公開する。VIOノードはCANへ直接指令せず、上位監督ノードが承認済み制御インターフェースに従って速度制限または停止要求へ変換する。外部MCUと独立安全回路の停止責務は変更しない。
