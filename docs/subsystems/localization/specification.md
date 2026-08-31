# Localization／VIO仕様

## 範囲

`mower_localization`は、カメラとIMUからVIOを計算し、推定結果と品質状態をROS 2へ公開する。将来は車輪オドメトリ融合、地図座標への補正、再ローカライゼーションも担当するが、初期実装は単眼VIOに限定する。

本パッケージは安全系ではない。推定品質は停止・減速判断の入力になり得るが、最終停止を保証しない。

## 入力前提

- ImageとCameraInfoは同じstamp、frame、画像サイズを持つ。
- カメラ内部較正は個体、レンズ、防水窓、取付、解像度、露光・ゲイン条件と対応付いている。
- ImuはSI単位、`imu_link`、単調増加する取得時刻を持つ。
- Camera-IMU外部変換と時刻補正は承認済み較正成果物から読む。
- 未較正、時刻逆行、非有限値、許容範囲を超えるgapがある入力ではTRACKINGへ遷移しない。

## 状態

| 状態 | 意味 | 出力方針 |
| --- | --- | --- |
| `UNCONFIGURED` | パラメータと依存が未ロード | Odometryを出さない |
| `INITIALIZING` | 入力待ちまたは初期化中 | 正常Odometryを出さない |
| `TRACKING` | 合格した入力で追跡中 | Odometryと品質を出す |
| `DEGRADED` | 品質低下または入力遅延 | 状態と診断をWARNで通知する |
| `LOST` | 追跡不能または入力timeout | 古いOdometryを正常値として出さない |
| `ERROR` | 設定、較正、内部処理の復旧不能異常 | ERROR診断、再設定まで停止 |

状態遷移の数値閾値は`docs/verification/vio-acceptance-criteria.md`で承認後に固定する。

## データ処理規則

- センサーstampを取得時刻として使用し、コールバック到着時刻で置き換えない。
- IMUは時刻順に処理し、重複または逆行したサンプルを拒否する。
- キューは有界とし、過負荷時に無制限な遅延を蓄積しない。
- 画像に必要なIMU範囲がそろう前に推定器へ渡さない。
- 入力欠損、drop、処理遅延、追跡状態、再初期化を診断へ記録する。
- 根拠のないOdometry covarianceを生成しない。推定器から得られない場合の表現はROSインターフェース仕様で固定する。

## 推定器adapter契約

- ROS 2の購読、入力検証、lifecycle、診断、Odometry Publishは`BasaltVioNode`が担当し、推定器固有処理は`VioEstimatorAdapter`の実装へ閉じ込める。
- adapterは検証済みImuを時刻順に受け取り、対応するImageとCameraInfoから`INITIALIZING`、`TRACKING`、`LOST`、`ERROR`の推定器状態を返す。
- `TRACKING`では推定器が生成したOdometryを必須とし、stampは入力画像、frameは`vio_odom -> base_link`、poseとtwistは有限値でなければならない。
- ノードはadapterが返したOdometryとcovarianceをそのまま検証・転送し、根拠のない姿勢、速度、covarianceを補完しない。
- `LOST`、`ERROR`、無効Odometry、adapter例外、adapterによるImu拒否ではOdometryをPublishしない。
- configureおよびcleanup時にadapterをresetし、前セッションの推定状態を次のセッションへ持ち越さない。
- Basalt未接続の既定adapterは入力を受け入れても推定結果を生成せず、正常OdometryをPublishしない。

## パラメータ分類

- topic、frame、QoS
- 較正ファイルと期待するセンサー個体ID
- 入力キュー上限、timeout、時刻gap上限
- Basalt設定ファイル
- TF Publishの有無
- diagnostics周期

実機プロファイルは追跡対象YAMLとして管理し、ソースコード既定値だけで実機運用しない。
