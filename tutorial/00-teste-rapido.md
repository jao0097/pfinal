# Teste rápido: 1 ponte H e 2 motores

[Índice](../README.md) · [Montagem completa: ligação elétrica →](04-ligacao-eletrica.md)

Use esta montagem para **validar o código** quando você só tem **1 ponte H L298N e 2 motores**.
O código é **o mesmo da garra completa, sem nenhuma mudança**: a ponte H é ligada exatamente
como a "Ponte H 1" da montagem final.

O desenho é da **ESP32 DevKit V1 (30 pinos)**, onde os pinos vêm impressos com "D" na frente
(D32, D33…). Na Arduino IDE, escolha a placa **DOIT ESP32 DEVKIT V1** ou **ESP32 Dev Module**: as duas funcionam.

![Teste com 1 ponte H L298N e 2 motores: pinos 32, 33, 25 e 26 do ESP32 nas entradas IN1 a IN4, motores nos bornes OUT, fonte e GND do ESP32 nos bornes de alimentação](../docs/img/teste-1-ponte-h.png)

## O que você precisa

| Item | Qtd. |
|---|---|
| ESP32 com o firmware gravado ([capítulo 3](03-gravar-firmware.md)) | 1 |
| Ponte H L298N | 1 |
| Motor amarelo TT | 2 |
| Fonte 7–9 V, 2 A ou mais | 1 |
| Jumper fêmea‑fêmea | 4 |
| Jumper macho‑fêmea (GND do ESP32) | 1 |

Não precisa de protoboard: o fio − da fonte e o jumper do GND do ESP32 vão juntos no
**mesmo borne GND** da ponte H.

## Ligação

| ESP32 | L298N |
|---|---|
| D32 | IN1 |
| D33 | IN2 |
| D25 | IN3 |
| D26 | IN4 |
| GND (o que fica ao lado do VIN) | borne GND (junto com o − da fonte) |

| Motor / fonte | Borne do L298N |
|---|---|
| Motor 1 | OUT1 e OUT2 |
| Motor 2 | OUT3 e OUT4 |
| Fonte − | GND |
| Fonte + | +12V |
| — | **+5V fica vazio** |

Os jumpers pretos **ENA** e **ENB** ficam no lugar.

## O que acontece no app

| Cartão do app | Controla |
|---|---|
| **Base** (Esquerda / Direita) | Motor 1 |
| **Ombro** (Desce / Sobe) | Motor 2 |
| Cotovelo e Garra | Nada neste teste (os pinos deles não estão ligados) |

> Se os motores já estiverem presos na garra em outras juntas (por exemplo, abrir e fechar
> a pinça), o nome no app não vai bater com o movimento. Isso é normal no teste: o cartão Base
> sempre move o Motor 1 e o cartão Ombro sempre move o Motor 2.

## Roteiro de validação

| # | Faça | Resultado esperado |
|---|---|---|
| 1 | Ligue só o ESP32 no USB e abra o Monitor Serial (115200) | Aparece `Rede: Garra-ESP` e `http://192.168.4.1` |
| 2 | Conecte o celular na rede `Garra-ESP` e abra `192.168.4.1` | O app abre e o indicador fica **verde** |
| 3 | Ligue a fonte. Segure **Base → Direita** | O Motor 1 gira. Ao soltar, para |
| 4 | Segure **Base → Esquerda** | O Motor 1 gira para o **outro** lado |
| 5 | Segure **Ombro → Sobe** e depois **Desce** | O Motor 2 gira para um lado e para o outro |
| 6 | Segure Base e Ombro **ao mesmo tempo** | Os dois giram juntos, e o ESP32 não reinicia |
| 7 | Mova o slider de **Velocidade** de 30% a 100% | A força do motor muda |
| 8 | Segure um botão e aperte **PARAR TUDO** | O motor para na hora |
| 9 | Segure um botão por mais de 6 s | O motor para sozinho e o app mostra "solte o botão" |
| 10 | Segure um botão e desligue o Wi‑Fi do celular | O motor para em menos de 1 s |

Se os 10 itens passarem, o código está validado. Para a montagem final, é só acrescentar a
segunda ponte H com os outros 2 motores ([capítulo 4](04-ligacao-eletrica.md)).

## Se algo não funcionar

**Primeiro, descubra se o problema é na montagem ou no app.** Grave o programa
[`firmware/teste_motores/teste_motores.ino`](../firmware/teste_motores/teste_motores.ino). Ele não usa
Wi‑Fi nem app: gira cada motor (Base, Ombro, Cotovelo e Garra) para um lado e para o outro, sem parar.
Com só 1 ponte H, Cotovelo e Garra simplesmente não giram na vez deles.

| Com o programa de teste | Significa |
|---|---|
| Os motores giram | A montagem está certa. O problema é no app ou no código principal |
| O LED azul da placa pisca, mas os motores não giram | Problema na ligação ou na fonte (veja a tabela abaixo) |
| O LED azul não pisca | O ESP32 não está ligado ou o programa não foi gravado |

Depois do teste, grave de novo o `garra_esp32.ino`.


| Sintoma | Verifique |
|---|---|
| Nenhum motor gira | Jumpers ENA/ENB no lugar? Fonte ligada no +12V e GND? |
| Motor só gira para um lado | Um dos 2 jumpers de sinal daquele motor está solto ou no pino errado |
| Motor gira ao contrário do botão | Troque os 2 fios dele no borne OUT |
| ESP32 reinicia quando o motor liga | O GND do ESP32 não está no borne GND da ponte H |
| Motor fraco ou só zumbe | Aumente a velocidade no app. Confira se a fonte é de 7–9 V |

Mais soluções em [problemas comuns](09-problemas.md).
