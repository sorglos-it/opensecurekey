# Construire et développer soi-même

Ce chapitre est pour celles et ceux qui construisent le projet eux-mêmes. La technique détaillée est en anglais dans le dossier [`docs/`](../) (par exemple `architecture.md`, `threat-model.md`, `vault.md`, `build.md`).

**État :** Il n’y a pas encore de clé. On construit et on teste sur l’ordinateur, avec des appareils de test pour la puce, la mémoire et l’écran.

## Dossiers

| Dossier | Contenu | Langage |
| --- | --- | --- |
| `apps/firmware` | Logiciel dans la clé (nRF52840 + SE051) | C / Zephyr |
| `apps/desktop` | Programme de gestion `osk` avec interface locale | Go |
| `apps/web` | Extension de navigateur | JavaScript |
| `shared/` | Protocole, format du coffre, bibliothèque `osk-guard` | |
| `hardware/` | Schéma, brochage, notes de mise en page | |
| `cad/` | Boîtier | |
| `tools/` | Outils de construction et de vérification | Python |

## Ce dont tu as besoin

| Partie | Outil | Sans cet outil |
| --- | --- | --- |
| toutes | Python 3 | rien ne marche |
| `apps/desktop` | Go 1.24 ou plus récent | est ignoré |
| `apps/web` | Node.js 20 ou plus récent (seulement pour vérifier) | Construire marche, vérifier est ignoré |
| `apps/firmware` | nRF Connect SDK (Zephyr, `west`) | est ignoré |

« Ignoré » veut toujours dire : **pas construit, pas vérifié.**

## Tout vérifier

```powershell
python tools/check.py
```

- Il vérifie ce qui est possible sans matériel.
- À la fin, il indique combien d’étapes ont tourné, ce qui a été ignoré et combien de tests ont été exécutés.
- Le code de sortie n’est 0 que si rien n’a échoué **et** si au moins un test a tourné.

## Construire

```powershell
python tools/build.py desktop
```

```powershell
python tools/build.py web
```

```powershell
python tools/build.py firmware
```

Les résultats se trouvent dans `dist/`.

Le programme `osk` peut aussi être construit directement. Il n’a besoin d’aucun paquet externe ni de cgo (sauf pour macOS).

```powershell
go build -C apps\desktop -trimpath -ldflags "-s -w" -o ..\..\dist\osk.exe .
```

## Tester sans clé

La clé de test (l’appareil de test) ne tourne qu’en mémoire. Elle ne chiffre rien et fait seulement comme si tu avais appuyé sur le bouton. **N’y mets jamais de vrais secrets.**

```powershell
go run -C apps\desktop .\tests\simulator -port 8201
```

Ensuite tu saisis `127.0.0.1:8201` sous **Réglages → Adresse de l'appareil de test.** Le code PIN de l’appareil de test est `123456`.

Les tests du programme :

```powershell
go test -C apps\desktop ./...
```

## Quatre niveaux de « vérifié »

Chaque affirmation sur « vérifié » désigne l’un de quatre niveaux :

1. écrit
2. compilé
3. dans l’image
4. a tourné sur la clé

Ce projet en est au **niveau 2, plus des essais sur l’ordinateur avec des appareils de test.** Les niveaux 3 et 4 manquent partout.

## Décisions et leur raison

| Décision | Raison |
| --- | --- |
| Deux puces : la puce radio parle, la puce de sécurité garde les secrets | La protection en lecture de la puce radio a déjà été contournée. |
| Deux voies de configuration, à choisir une seule fois | La puce de sécurité ne dérive aucune clé en elle-même. Un secret né dans la puce ne peut jamais être copié. |
| Écran et bouton à chaque autorisation | L’ordinateur ne doit pas décider ce que tu autorises. |
| Pas d’autorisation sans écran qui fonctionne | Tu ne pourrais pas voir ce que tu autorises. |
| Les noms et les adresses ne sont jamais coupés | Un destinataire coupé est pire que pas d’écran. |
| Niveau 1 sans capteur d’empreinte digitale | Avec FIDO2, il remplace le code PIN, il ne le complète pas. Aucun capteur ne remplissait les quatre conditions. |
| Place de mise à jour dans la mémoire externe | La mémoire interne de la puce radio est limitée. |
| Police propre et vérificateurs propres pour le protocole | Aucune bibliothèque externe de graphisme ou d’analyse, limites dures, pas de mémoire dynamique. |
| Pas de code externe | Tout est écrit d’après la spécification. Le chiffrement vient toujours de bibliothèques vérifiées. |

## Cinq langues

- Aucun texte n’est écrit en dur dans le code.
- Chaque langue est un fichier dans le dossier `lang/` : `de.json`, `en.json`, `fr.json`, `it.json`, `es.json`.
- Un test signale les clés manquantes comme erreur.
- Si un texte manque, c’est l’anglais qui vaut.

## Assemblage

**Nouveau :** cette section est nouvelle. D'autres consignes d'assemblage viendront ici dès que le matériel sera sur la table.

- Il n'y a pas encore de consigne contraignante pour l'assemblage. L'ancienne consigne sur le film de ferrite sous la batterie tombe avec la batterie.
- Les cotes et les consignes ne sont écrites que dans `hardware/layout-notes.md`. Ici, volontairement, aucun chiffre, pour qu'il ne se périme pas de nouveau.

## Participer

Ne change jamais seulement la technique. Quand la fonction, l’utilisation, les réglages ou la structure changent, la documentation en fait partie : `docs/` et ce manuel.
