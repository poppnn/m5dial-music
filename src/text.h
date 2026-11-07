// Nettoyage des textes reçus du téléphone avant affichage.
#pragma once
#include <string>

// Retire ce que la police de l'écran (efont) ne sait pas dessiner et qui sortirait en carrés : emojis
// (caractères UTF-8 sur 4 octets), sélecteurs de variante U+FE0E/U+FE0F, liants U+200D, U+20E3 ; puis
// resserre les espaces laissés.
std::string cleanText(const std::string& s);
void cleanTextSelfTest();
