# Carro 4WD com ESP32 controlado pela web

Carro com chassi de acrílico, **4 motores TT**, **ponte H L298N**, **2 pilhas 18650** com chave,
**ESP32 DevKit V1** e **sensor ultrassônico HC-SR04** na frente.

## Análise da montagem (pelas fotos)

| Peça | O que vi | Observação |
|---|---|---|
| ESP32 DevKit V1 (30 pinos) | Na traseira do chassi, alimentado pelo USB | Os fios saem de um lado só. Não deu pra ler o nome dos pinos nas fotos, por isso a etapa 1 **descobre os pinos** |
| Ponte H L298N | LED vermelho aceso, 2 bornes de motor | 4 motores em 2 canais: os 2 motores de cada lado estão **em paralelo** |
| 2× 18650 + chave | Borne +12V da ponte H | 7,4–8,4 V. O L298N perde ~2 V, então os motores recebem uns 6 V, bom para motor TT |
| HC-SR04 | Na frente, fios trançados até o ESP32 | Funciona com 5 V. O **ECHO sai 5 V**: os resistores na protoboard devem ser o divisor de tensão (ex.: 1 kΩ + 2 kΩ) |
| Protoboard | 2 resistores e jumpers | Confirme que o divisor está entre o ECHO e o pino do ESP32 |

### Pontos de atenção

1. **GND comum**: o GND do ESP32 precisa estar ligado no GND da ponte H. Sem isso o ESP32 manda o sinal e o motor não entende.
2. **Rodas do mesmo lado em paralelo**: se um dos dois motores de um lado estiver com os fios invertidos, as rodas "brigam" (uma pra frente, outra pra trás). O teste mostra isso.
3. **Fios soltos perto das rodas**: o par laranja/branco da frente passa perto do pneu. Prenda com fita ou abraçadeira antes de andar no chão.
4. **Alimentação do ESP32**: por enquanto use o USB. Depois dá pra alimentar pelo +5V da ponte H no VIN, mas com a pilha fraca ou com pico do motor o ESP32 pode reiniciar.
5. **Pinos a evitar**: 6–11 (memória), 34–39 só servem de entrada (podem ser o ECHO), 0/2/12/15 influenciam o boot. Se o ESP32 não ligar com tudo conectado, desconfie de um fio no D12.

## Etapa 1 — teste e descoberta dos pinos ✅

Programa: [`firmware/carro_teste/carro_teste.ino`](firmware/carro_teste/carro_teste.ino) (arquivo único, sem bibliotecas extras).

### Gravar

1. Arduino IDE → placa **ESP32 Dev Module** (ou "DOIT ESP32 DEVKIT V1"), porta USB do ESP32.
2. Abra o `.ino` e clique em **Carregar**. Se travar em "Connecting…", segure o botão **BOOT** da placa.
3. Abra o **Monitor Serial** em 115200: deve aparecer `Rede: Carro-ESP32`.

### Usar

1. **Rodas no ar** (carro em cima de um copo), chave das pilhas **ligada**.
2. No celular, conecte na rede **`Carro-ESP32`**, senha **`carro1234`**. Se o celular reclamar de "sem internet", escolha **manter conexão** e desligue os dados móveis.
3. Abra **http://192.168.4.1** e siga os passos da página:

| Passo | O que acontece |
|---|---|
| 1 · Achar o sensor | O ESP32 testa sozinho qual pino é TRIG e qual é ECHO (pulsos de 12 µs, não mexe motor) |
| 2 · Descobrir os motores | Você toca num pino, ele liga por 1 s, e você diz o que as rodas fizeram (Esquerda ↑, Direita ↓, Nada…) |
| 3 · Configuração | Montada a partir das respostas e **salva na memória do ESP32** |
| 4 · Teste final | Cada lado, frente, ré, giros, com velocidade ajustável |
| 5 · Resultado | Texto pronto para copiar e colar na conversa |

- **Frente** do carro = lado do sensor. **Esquerda/direita** = como se você estivesse sentado nele.
- Todo movimento para sozinho em no máximo 3 s, e o botão **PARAR** fica sempre no rodapé.
- Se **nenhum** pino fizer roda girar no modo normal, os ENA/ENB estão com fio para o ESP32 (sem os jumpers pretos). Troque o modo para "ENA/ENB com fio para o ESP32".
- Sem celular? No Monitor Serial: `p26` liga o pino 26 por 1 s, `x` para, `u` procura o sensor, `c` mostra a configuração.

### O que o teste pode mostrar

| Você viu | Causa | Correção |
|---|---|---|
| Nenhum pino faz nada | GND não comum, chave desligada ou ENA/ENB sem jumper | Ligue GND do ESP32 no GND da ponte H; confira o LED da ponte H; tente o outro modo |
| Rodas do mesmo lado em sentidos opostos | Um motor daquele lado com fios invertidos | Inverta os 2 fios de **um** dos motores daquele lado |
| Só 1 roda gira | Fio solto no outro motor | Aperte o borne / refaça a solda |
| Sensor não encontrado | VCC/GND do sensor, ou ECHO sem chegar no ESP32 | VCC no 5V, GND comum, confira o divisor |

## Etapa 2 — controle pela web (próxima)

O controle (joystick no celular, velocidade, parada automática perto de obstáculo) vai ler a
configuração que a etapa 1 salvou no ESP32, então não precisa mexer em pino no código.
Mande o texto do passo 5 para seguirmos.
