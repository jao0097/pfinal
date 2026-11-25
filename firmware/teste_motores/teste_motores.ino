// ============================================================
//  TESTE DE MOTORES (sem Wi-Fi e sem app)
//
//  Serve para saber se o problema está na MONTAGEM ou no APP.
//  Gira cada motor para um lado (2 s), para (1 s), gira para o
//  outro lado (2 s) e para (1 s). Depois passa para o próximo,
//  na ordem: Base, Ombro, Cotovelo, Garra. Repete para sempre.
//
//  O LED azul da placa (GPIO 2) acende enquanto algum motor gira.
//  O Monitor Serial (115200) mostra qual motor está girando.
//
//  Ligação: a mesma da garra completa
//    Ponte H 1:  D32 -> IN1  D33 -> IN2  (Base)   D25 -> IN3  D26 -> IN4  (Ombro)
//    Ponte H 2:  D23 -> IN1  D22 -> IN2  (Cotovelo) D19 -> IN3  D18 -> IN4 (Garra)
//
//  Funciona também com só 1 ponte H (Base e Ombro): os outros dois
//  motores simplesmente não giram na vez deles.
//
//  Resultado:
//    - Motores giram  -> montagem OK. O problema é no app/código principal.
//    - LED pisca mas um motor não gira -> problema na ligação daquele motor.
//    - LED não pisca -> o ESP32 não está ligado ou o código não foi gravado.
// ============================================================

struct Motor { const char* nome; int a; int b; };

const Motor MOTORES[] = {
  { "Base",     32, 33 },
  { "Ombro",    25, 26 },
  { "Cotovelo", 23, 22 },
  { "Garra",    19, 18 },
};
const int N = sizeof(MOTORES) / sizeof(MOTORES[0]);
const int LED = 2;

void girar(const Motor& m, int sentido) {
  digitalWrite(m.a, sentido > 0 ? HIGH : LOW);
  digitalWrite(m.b, sentido < 0 ? HIGH : LOW);
  digitalWrite(LED, sentido != 0 ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  for (int i = 0; i < N; i++) {
    pinMode(MOTORES[i].a, OUTPUT);
    pinMode(MOTORES[i].b, OUTPUT);
    girar(MOTORES[i], 0);
  }
  Serial.println("\n=== Teste de motores ===");
  delay(1000);
}

void loop() {
  for (int i = 0; i < N; i++) {
    const Motor& m = MOTORES[i];
    Serial.printf("%s -> sentido A\n", m.nome); girar(m,  1); delay(2000);
    Serial.printf("%s parado\n", m.nome);       girar(m,  0); delay(1000);
    Serial.printf("%s -> sentido B\n", m.nome); girar(m, -1); delay(2000);
    Serial.printf("%s parado\n", m.nome);       girar(m,  0); delay(1000);
  }
}
