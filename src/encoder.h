// Molette du M5Dial : compteur matériel PCNT (aucune étape perdue, même en tournant vite).
#pragma once

void encoderInit();
long encoderRead();  // position cumulée (4 impulsions par période de quadrature, comme l'Encoder M5Dial)
