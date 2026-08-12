# ROS 2 芝刈りロボットワークスペース

[`develop_plan.md`](develop_plan.md) で定義した自律走行芝刈りロボット向けのROS 2 Jazzy 開発ワークスペースです。
Raspberry Pi 5向けのarm64環境と、開発PC向けのx86_64環境の両方で動作します。
CSIカメラの仕様・起動方法・実機検証状況は [`docs/camera_libcamera.md`](docs/camera_libcamera.md) を参照してください。
ハードウェア依存の機能はROS 2パッケージの境界で分離します。

## 開始方法

推奨環境は、同梱のDev Containerです（Dockerが必要です）。

1. VS Codeでこのリポジトリを開き、**Reopen in Container** を選択します。
2. コンテナがROS依存関係を解決し、ワークスペースを自動ビルドします。
3. ROSコマンドを使用する前に、新しいターミナルで `install/setup.bash` をsourceします。

Ubuntu 24.04とROS 2 Jazzyを使うネイティブ環境では、次を実行します。

```bash
source /opt/ros/jazzy/setup.bash
rosdep update
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

## パッケージの責務

| パッケージ | 責務 |
| --- | --- |
| `mower_description` | ロボットモデル、座標フレーム、センサー搭載位置 |
| `mower_bringup` | 起動・設定とライフサイクルの統合 |
| `mower_camera` / `mower_imu` | CSIカメラおよびBMI270のインターフェース |
| `mower_can` | SocketCAN-ROSゲートウェイとCANプロトコル検証 |
| `mower_control` | 速度指令、ウォッチドッグ、車輪制御 |
| `mower_localization` | VIO、車輪オドメトリ融合、自己位置推定 |
| `mower_navigation` | カバレッジ計画とナビゲーション |
| `mower_simulation` | Gazebo・仮想CAN・HILシミュレーション |

ハードウェアドライバー、CAN ID、および安全停止の実装は、この雛形には含めません。
これらには計画書で定めた、電気設計およびMCUプロトコル仕様のレビューが必要です。
MCUが管理する安全停止機能や、独立したE-stopを迂回する用途には使用しないでください。

## 品質確認

ローカルの品質ゲートは `colcon build`、`colcon test`、`colcon test-result --verbose` です。
これらはGitHub Actionsでも実行されます。ビルド生成物とrosbag2データはGitの管理対象外です。
