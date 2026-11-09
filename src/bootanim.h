// Image ou GIF de démarrage, envoyé depuis le web flasher par USB (Web Serial) et rangé dans la partition flash
// "bootanim". Format (petit-boutiste) : "M5BA", u16 nombre d'images, u16 0, u32 taille totale, puis par image
// {u32 position, u32 taille, u16 durée ms, u16 0}, puis les images JPEG 240 x 240.
#pragma once
#include <stdint.h>
#include <stddef.h>

void bootAnimInit();             // au démarrage : trouve la partition et lit l'animation enregistrée
void bootAnimService();          // dans loop() : commandes "BOOTANIM ..." reçues sur le port USB
bool bootAnimDraw(uint32_t now); // dessine l'image courante dans canvas ; false s'il n'y a pas d'animation
bool bootAnimParse(const uint8_t* d, size_t n, uint16_t& count, uint32_t& totalMs);  // en-tête valide ?
void bootAnimSelfTest();
