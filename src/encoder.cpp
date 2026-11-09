// Molette du M5Dial lue par le compteur matériel PCNT de l'ESP32-S3. La bibliothèque M5Dial (Encoder PJRC) n'a
// pas d'interruption pour les broches 40/41 : elle ne lisait l'encodeur qu'à chaque tour de loop() (1 à 16 ms),
// donc en tournant vite des étapes étaient sautées et le sens tiré au hasard. Ici chaque front est compté en
// matériel, même pendant le rendu d'une image.
// Décodage x4, même sens et même échelle que l'Encoder PJRC (Réglages > Molette reste valable).
#include <Arduino.h>
#include <driver/pcnt.h>
#include "encoder.h"

#define ENC_A 41
#define ENC_B 40
#define UNIT  PCNT_UNIT_0
#define FILTER_APB_TICKS 1000  // fronts plus courts que 12,5 µs ignorés (rebonds de contact)

static long total = 0;
static int16_t last = 0;

void encoderInit() {
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pcnt_config_t c = {};
  c.unit = UNIT;
  c.counter_h_lim = 32767;
  c.counter_l_lim = -32768;
  // Canal 0 : fronts de A, sens selon B (B haut : montant +1, descendant -1 ; B bas : inversé)
  c.channel = PCNT_CHANNEL_0;
  c.pulse_gpio_num = ENC_A;
  c.ctrl_gpio_num = ENC_B;
  c.pos_mode = PCNT_COUNT_INC;
  c.neg_mode = PCNT_COUNT_DEC;
  c.hctrl_mode = PCNT_MODE_KEEP;
  c.lctrl_mode = PCNT_MODE_REVERSE;
  pcnt_unit_config(&c);
  // Canal 1 : fronts de B, sens selon A (A bas : montant +1, descendant -1 ; A haut : inversé)
  c.channel = PCNT_CHANNEL_1;
  c.pulse_gpio_num = ENC_B;
  c.ctrl_gpio_num = ENC_A;
  c.hctrl_mode = PCNT_MODE_REVERSE;
  c.lctrl_mode = PCNT_MODE_KEEP;
  pcnt_unit_config(&c);
  pcnt_set_filter_value(UNIT, FILTER_APB_TICKS);
  pcnt_filter_enable(UNIT);
  pcnt_counter_pause(UNIT);
  pcnt_counter_clear(UNIT);
  pcnt_counter_resume(UNIT);
}

long encoderRead() {
  int16_t now = 0;
  pcnt_get_counter_value(UNIT, &now);
  total += now - last;
  last = now;
  // Remis à 0 bien avant ses limites (±32767, où le PCNT repart de 0 et fausserait l'écart) ; les fronts tombés
  // entre la lecture et la remise à zéro (quelques µs) sont perdus, sans effet à l'échelle d'un cran.
  if (abs(now) > 16000) { pcnt_counter_clear(UNIT); last = 0; }
  return total;
}
