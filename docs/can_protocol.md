# MCP2515 CANプロトコル（暫定版 1）

この文書はPi側gatewayの実装に対応する暫定仕様である。モータまたはブレードを有効にする前に、MCUファームウェア担当とID、timeout、各バイトの解釈を承認し、実装値をここへ反映すること。標準11-bit ID、CAN 2.0B、little-endianを使用する。

## 設定値

| 項目 | 暫定値 | 根拠 |
| --- | ---: | --- |
| motion command ID | `0x200` | `mcp2515.yaml`で変更可能 |
| heartbeat ID | `0x201` | `mcp2515.yaml`で変更可能 |
| 周期 | 10 ms | `command_period_ms`（MCUの実行周期に同期） |
| command timeout | 50 ms | `command_timeout_ms`（5送信周期）。MCUは独立に同等以下のtimeoutを持つ |
| protocol version | 1 | version不一致はREADYに遷移させない |

### 周期・timeoutの選定根拠（暫定）

motion commandとheartbeatは、MCUの10 ms実行周期に合わせて10 ms（100 Hz）で送信する。これにより、MCUは各実行周期で最新の指令または通信断を評価できる。

通信timeoutは50 ms（送信周期5回分）とする。単発の送信周期ずれや1フレームの欠落では停止しにくくしつつ、通信断時は短時間で停止要求へ移行するためである。実機最大並進速度0.55 m/sでは、timeoutの間に進む距離は最大27.5 mm（`0.55 m/s × 0.050 s`）である。この距離に、MCUの処理時間、駆動系の応答時間および機械的な制動距離を加えた値が実際の通信断時停止距離となる。

これらの値は暫定であり、CAN負荷、周期ジッタ、MCU側の監視実装、実機で測定した通信断時停止距離および安全要件を用いて確定する。MCU側のheartbeat監視timeoutは50 ms以下とし、通信復旧だけで走行を再開してはならない。

## Motion command（Pi → MCU、DLC=8）

| Byte | 内容 | 表現 |
| ---: | --- | --- |
| 0 | sequence | `uint8`、0からロールオーバー |
| 1 | flags | bit 0: enable、bit 1: stop。enableがfalseのときstopをセット |
| 2–3 | linear velocity | `int16` little-endian、m/s × 1000 |
| 4–5 | angular velocity | `int16` little-endian、rad/s × 1000 |
| 6 | protocol version | `uint8`、値1 |
| 7 | reserved | 0 |

Piはenableがfalse、`/cmd_vel`が未受信、または50 msを超えて期限切れのとき、stopフラグと両速度ゼロを送る。MCUはこの挙動に依存せず、期限・連番・DLC・version・値域・安全状態を検査して停止する。MCUは通信復旧だけで走行を再開してはならない。

## Heartbeat（Pi → MCU、DLC=2）

| Byte | 内容 |
| ---: | --- |
| 0 | protocol version（1） |
| 1 | sequence（`uint8`） |

heartbeatは通信監視用であり、動作許可または再始動許可ではない。

## MCU → Pi

MCU status、wheel odometry、power/thermalのIDとレイアウトはMCU実装と未合意である。このgatewayは受信フレーム数とSocketCAN error frameを`/diagnostics`へ出力するが、未承認フレームを制御・オドメトリ情報へ変換しない。合意後にdecode、`/wheel/odometry`、`/battery/state`、MCU状態messageを追加する。
