# MCP2515 CANプロトコル（暫定版 1）

この文書はPi側gatewayの実装に対応する暫定仕様である。モータまたはブレードを有効にする前に、MCUファームウェア担当とID、timeout、各バイトの解釈を承認し、実装値をここへ反映すること。標準11-bit ID、CAN 2.0B、little-endianを使用する。

## 設定値

| 項目 | 暫定値 | 根拠 |
| --- | ---: | --- |
| motion command ID | `0x200` | `mcp2515.yaml`で変更可能 |
| heartbeat ID | `0x201` | `mcp2515.yaml`で変更可能 |
| 周期 | 20 ms | `command_period_ms` |
| command timeout | 100 ms | `command_timeout_ms`、MCUは独立に同等以下のtimeoutを持つ |
| protocol version | 1 | version不一致はREADYに遷移させない |

## Motion command（Pi → MCU、DLC=8）

| Byte | 内容 | 表現 |
| ---: | --- | --- |
| 0 | sequence | `uint8`、0からロールオーバー |
| 1 | flags | bit 0: enable、bit 1: stop。enableがfalseのときstopをセット |
| 2–3 | linear velocity | `int16` little-endian、m/s × 1000 |
| 4–5 | angular velocity | `int16` little-endian、rad/s × 1000 |
| 6 | protocol version | `uint8`、値1 |
| 7 | reserved | 0 |

Piはenableがfalse、`/cmd_vel`が未受信、または100 msを超えて期限切れのとき、stopフラグと両速度ゼロを送る。MCUはこの挙動に依存せず、期限・連番・DLC・version・値域・安全状態を検査して停止する。MCUは通信復旧だけで走行を再開してはならない。

## Heartbeat（Pi → MCU、DLC=2）

| Byte | 内容 |
| ---: | --- |
| 0 | protocol version（1） |
| 1 | sequence（`uint8`） |

heartbeatは通信監視用であり、動作許可または再始動許可ではない。

## MCU → Pi

MCU status、wheel odometry、power/thermalのIDとレイアウトはMCU実装と未合意である。このgatewayは受信フレーム数とSocketCAN error frameを`/diagnostics`へ出力するが、未承認フレームを制御・オドメトリ情報へ変換しない。合意後にdecode、`/wheel/odometry`、`/battery/state`、MCU状態messageを追加する。
