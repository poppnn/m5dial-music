// Mise à jour sans fil (OTA) : par le partage de connexion du téléphone, choisi et son mot de passe tapé à la molette
// (ou envoyés depuis le web flasher), télécharge le firmware publié sur GitHub Pages pour cette plateforme s'il change.
#pragma once

void otaSelfTest();  // saisie du mot de passe à la molette
void otaRun();  // Réglages > Update : prend tout l'écran, ne revient pas (redémarre à la fin, même en cas d'échec)
