# Diagnóstico de solda — motores do micromouse

Registros do diagnóstico físico feito na placa do micromouse após o robô
apresentar erros de rotação nos motores, e da revisão do firmware de
controle dos motores feita em conjunto com o time de software.

Relatório completo: [`../../Relatorio_Diagnostico_Solda_Motores.pdf`](../../Relatorio_Diagnostico_Solda_Motores.pdf)

## Fotos de bancada (diagnóstico de hardware)

- `01_bancada_teste_continuidade_protoboard.jpeg` — teste de continuidade
  dos fios de alimentação e sinal na protoboard do ESP32 com o multímetro.
- `02_teste_continuidade_ponte_h.jpeg` — verificação ponto a ponto da solda
  da placa, checando continuidade entre trilhas e conectores.
- `03_bancada_teste_completo_motores.jpeg` — bancada completa de teste após
  o reparo, com a ponte H, a bateria e os motores conectados para validação
  final.

Problemas encontrados: mau contato em fios/conectores (falha intermitente)
e uma trilha da ponte H com solda fria/parcial, que causava erros de
rotação em um dos motores.

## Capturas do firmware (colaboração com o time de software)

- `04_codigo_pinagem_motores_encoders.jpeg` — pinagem dos motores, evitando
  os pinos reservados aos encoders.
- `05_codigo_configuracao_pwm_motores.jpeg` — configuração dos canais PWM
  (ledc) das duas pontes H.
- `06_codigo_teste_movimento_motores.jpeg` — rotina de teste automático de
  movimento (frente/trás) usada para validar a correção após o reparo.
