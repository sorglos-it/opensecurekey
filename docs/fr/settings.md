# Réglages

**État :** Les réglages du programme sont vérifiés contre l’appareil de test. Les réglages dans la clé elle-même n’existent pas encore sur une vraie clé.

Il y a peu de réglages. C’est voulu.

## Dans le programme `osk`

Ouvre dans l’interface la page **Réglages.** Les réglages sont enregistrés dans le fichier `config_osk.json` **à côté du programme**, pas dans le système.

| Réglage | Signification | Valeur par défaut | Valeurs permises |
| --- | --- | --- | --- |
| Langue | Langue de l’interface | `de` | `de`, `en`, `fr`, `it`, `es` |
| Nom de ce programme | Le nom que le programme donne à la clé. Il arrive dans le journal. | `osk` | Texte jusqu’à 64 caractères |
| Port | Port sur lequel l’interface écoute | `8101` | Nombre de 1024 à 65000 |
| Adresse de l'appareil de test | Seulement pour tester : adresse de la clé de test | vide | par exemple `127.0.0.1:8201`, et seulement sur cet ordinateur |

Bon à savoir :

- **Le nom est une affirmation.** La clé l’écrit dans le journal, mais il ne prouve rien. Seule la clé elle-même peut refuser quelque chose.
- Si le port est occupé, le programme cherche un port libre à partir de la valeur réglée. S’il n’en trouve pas, il le dit.
- **Laisse l’adresse de l’appareil de test vide** tant que tu utilises une vraie clé. L’appareil de test est un outil d’essai. N’y mets jamais de vrais secrets. Son code PIN est `123456`, et il ne chiffre rien.
- Si le fichier de réglages est abîmé ou trop grand, le programme démarre quand même. Il prend les valeurs par défaut et dit pourquoi.

Exemple (`config_osk.json`) :

```json
{
  "lang": "de",
  "port": 8101,
  "appName": "osk",
  "simAddress": ""
}
```

## Pour chaque entrée dans la clé

Chaque entrée a trois interrupteurs. Voir [usage.md](usage.md), section « Types d’entrées ».

- peut être copié
- seulement ordinateurs appairés
- protégé en écriture

## Dans la clé elle-même

Ces choses se règlent **sur la clé**, pas dans le programme. **État :** pas encore essayé nulle part.

| Réglage | Valeur par défaut | Ce que cela change |
| --- | --- | --- |
| Profil et code PIN | jusqu’à 4 profils, chacun avec son code PIN | Qui voit quoi |
| Fonction d’effleurement NFC (ouvrir une adresse ou taper un code unique) | peut être désactivée | Un téléphone ouvre une adresse quand on l’approche |
| Signer à l’aveugle | désactivé | La clé signe aussi des choses qu’elle ne peut pas afficher de façon lisible. La question à l’écran a alors un aspect nettement différent. |
| Autorisation de serveur | désactivée | Un serveur peut signer sans appui, pour les clés nommées, 24 heures au plus |
| Bluetooth | seulement pour le programme de gestion | Pas pour la connexion dans le navigateur. Appairage seulement avec comparaison de nombres et appui sur la clé. |

Bon à savoir :

- **Tout ce qui est risqué est désactivé** jusqu’à ce que tu l’actives en connaissance de cause.
- **Le NFC demande toujours le bouton.** Même quand le NFC dure peu. Cela protège contre une attaque depuis la poche, où quelqu’un fait suivre la liaison radio.
- L’adresse radio du Bluetooth change, pour qu’on ne puisse pas te suivre.

## Langue sur la clé

L’écran de la clé connaît les mêmes cinq langues. Si un texte manque, il prend l’anglais, jamais un emplacement vide.

## Changer de langue

Change la langue dans le programme sous **Réglages.** Les fichiers de langue sont dans le dossier `lang`, un par langue.
