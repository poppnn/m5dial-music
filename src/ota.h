// Mise à jour sans fil (OTA) : par le partage de connexion du téléphone (nom et mot de passe envoyés une fois depuis
// le web flasher), télécharge le firmware publié sur GitHub Pages pour cette plateforme s'il est plus récent.
#pragma once

void otaRun();  // Réglages > Update : prend tout l'écran, ne revient pas (redémarre à la fin, même en cas d'échec)
