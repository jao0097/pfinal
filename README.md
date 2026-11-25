# Garra hidráulica controlada pelo celular

Braço robótico com garra movido por **8 seringas com água** e **4 motores DC**, controlado
pelo **celular** através da rede Wi‑Fi criada por um **ESP32**. Sem internet, sem app de loja,
sem biblioteca extra e sem emendar fios.

<p align="center">
  <img src="docs/img/braco-vista-lateral.png" alt="Vista lateral do braço com as seringas" width="640"><br>
  <img src="docs/img/app-celular.png" alt="App de controle no celular" width="240">
</p>

```
Celular → Wi-Fi → ESP32 → Ponte H → Motor DC → Manivela → Seringa mestre → água → Seringa escrava → Junta
```

## Só tem 1 ponte H e 2 motores?

Comece pelo **[teste rápido](tutorial/00-teste-rapido.md)**: valida o código com metade das peças,
sem mudar nada no programa.

<p align="center"><img src="docs/img/teste-1-ponte-h.png" alt="Teste com 1 ponte H L298N e 2 motores" width="640"></p>

## Tutorial passo a passo

| # | Capítulo | O que você faz |
|---|---|---|
| 1 | [Materiais](tutorial/01-materiais.md) | Lista de compras completa |
| 2 | [Preparar a Arduino IDE](tutorial/02-arduino-ide.md) | Instalar a IDE, a placa ESP32 e o driver USB |
| 3 | [Gravar o firmware](tutorial/03-gravar-firmware.md) | Abrir, configurar e gravar o código no ESP32 |
| 4 | [Ligação elétrica](tutorial/04-ligacao-eletrica.md) | ESP32 + 2 pontes H L298N + 4 motores + fonte |
| 5 | [Hidráulica](tutorial/05-hidraulica.md) | Montar os pares de seringas sem bolhas |
| 6 | [Montagem mecânica](tutorial/06-montagem-mecanica.md) | Manivelas, estação de controle, braço, base e pinça |
| 7 | [App do celular](tutorial/07-app-celular.md) | Conectar, controlar e pôr o ícone na tela inicial |
| 8 | [Testes e calibração](tutorial/08-testes.md) | Testar uma etapa por vez |
| 9 | [Problemas comuns](tutorial/09-problemas.md) | Erros da IDE, do celular e da montagem |
| 10 | [Como funciona](tutorial/10-como-funciona.md) | Explicação para relatório e apresentação |

## Resumo rápido

| Motor | Ponte H | Pinos do ESP32 | Botões no app |
|---|---|---|---|
| Base | 1 (IN1/IN2) | 32 · 33 | Esquerda / Direita |
| Ombro | 1 (IN3/IN4) | 25 · 26 | Desce / Sobe |
| Cotovelo | 2 (IN1/IN2) | 23 · 22 | Recolhe / Estica |
| Garra | 2 (IN3/IN4) | 19 · 18 | Abre / Fecha |

- **Rede:** `Garra-ESP`, senha `garra1234`, endereço **http://192.168.4.1**
- **Fonte:** 7 V, 2 A ou mais (o L298N perde ~2 V). O ESP32 recebe 5 V do borne +5V da Ponte H 1 no VIN, sem USB
- **Diagrama completo:** [`docs/img/garra-completa.png`](docs/img/garra-completa.png)
- **Código:** um arquivo só, [`firmware/garra_esp32/garra_esp32.ino`](firmware/garra_esp32/garra_esp32.ino)

## Estrutura do repositório

```
├── README.md                     ← você está aqui
├── tutorial/                     ← teste rápido + 10 capítulos
├── firmware/garra_esp32/
│   └── garra_esp32.ino           ← código completo (configuração + app + programa)
├── docs/
│   ├── img/                      ← diagramas e telas do app
│   └── diagramas.html            ← todos os diagramas numa página (abra no navegador)
└── platformio.ini                ← opcional, para quem usa PlatformIO
```

## Licença

Livre para copiar, modificar e usar em projetos escolares.
