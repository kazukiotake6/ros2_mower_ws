# 依存関係ベースライン

## Bosch BMI270 Sensor API

| 項目 | 値 |
| --- | --- |
| 用途 | `mower_imu`のBMI270初期化・設定 |
| upstream | `https://github.com/boschsensortec/BMI270_SensorAPI` |
| 固定commit | `41129fcfe39c583ee5462d79195741945d51c1fe` |
| ライセンス | BSD-3-Clause |
| vendoring先 | `src/mower_imu/third_party/bmi270/` |
| 取得確認日 | 2026-09-05 |

`bmi2.c`、`bmi2.h`、`bmi2_defs.h`、`bmi270.c`、`bmi270.h`および`LICENSE`だけを取り込む。更新時は固定commit、ライセンス、API差分、x86_64ビルド、Pi 5 arm64ビルドと実機再試験の要否をレビューする。
