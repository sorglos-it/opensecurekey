# OpenSecureKey – Manuel

**Cet appareil n’est pas encore en vente, et personne ne l’a encore utilisé.**

Tout ce manuel décrit ce qui est en cours de construction. Rien de tout cela n’a jamais tourné sur une vraie clé. Aucune clé n’est encore construite. Quand quelque chose a été vérifié seulement sur l’ordinateur, c’est écrit à côté.

## Qu’est-ce que c’est ?

Une clé de sécurité à mettre au porte-clés. Elle garde tes accès, et rien ne sort si tu n’appuies pas toi-même sur le bouton de la clé.

Concrètement :

- Elle te connecte aux sites web sans que tu tapes un mot de passe (passkeys).
- Elle stocke des mots de passe, des codes, des clés et des certificats.
- Elle peut se comporter comme une carte à puce.
- Elle peut signer des paiements en Bitcoin et en Ethereum.
- Elle peut laisser un programme tourner seulement tant qu’elle est branchée (dongle).

La clé a son propre petit écran. Il y est écrit **ce que** tu autorises à l’instant. L’ordinateur ne peut pas te tromper là-dessus.

## Comment ce manuel est construit

| Fichier | À quoi il sert |
| --- | --- |
| [installation.md](installation.md) | Première configuration, pas à pas. La question qui ne pourra plus jamais être modifiée s’y trouve. |
| [usage.md](usage.md) | Au quotidien : se connecter, récupérer un mot de passe, un code, signer. |
| [settings.md](settings.md) | Ce qui peut être réglé. |
| [how-it-works.md](how-it-works.md) | Comment la clé travaille à l’intérieur. Et ce qu’elle **ne** sait **pas** faire. |
| [troubleshooting.md](troubleshooting.md) | Quand quelque chose ne marche pas. Et : **quand la clé est perdue.** |
| [development.md](development.md) | Pour celles et ceux qui construisent le projet eux-mêmes. |

## L’essentiel d’abord

- **Une question lors de la configuration ne pourra plus jamais être modifiée :** une clé de rechange doit-elle rester possible plus tard ? Lis-la tranquillement. Voir [installation.md](installation.md).
- **Sans clé de rechange et sans sauvegarde, une clé perdue est une perte totale.**
- **Qui perd la phrase secrète principale et toutes les clés perd tout.** Il n’y a pas de porte dérobée pour cela. Voir [troubleshooting.md](troubleshooting.md).
- **La feuille de secours lors de la configuration ne peut pas être sautée.** Justement pour cela. Voir [installation.md](installation.md).
- Il n’y a **aucun contrôle extérieur** et **aucun test sur du vrai matériel.** Ne compte encore sur rien que tu ne peux pas remplacer.

## Ce que la clé ne sait pas faire

Détaillé dans [how-it-works.md](how-it-works.md). En bref :

- Elle n’a, dans la première étape de construction, **aucun capteur d’empreinte digitale.** Et pour aucun des capteurs examinés il n’est prouvé qu’il reconnaisse une copie d’empreinte.
- Elle ne te protège **pas** si quelqu’un te force à appuyer sur le bouton.
- Elle ne protège **pas** contre un micrologiciel modifié.
- Elle ne te sauve **pas** si la phrase secrète principale et toutes les clés ont disparu.
- **Elle ne donne aucune clé privée sous forme de fichier. Jamais.** Un logiciel qui exige impérativement un fichier de clé (`.pem`, `.pfx`) ne peut fondamentalement pas s’en servir. Le certificat peut sortir, la clé non. Voir [usage.md](usage.md).
- Elle ne connaît **aucun profil pour les passkeys.** Au changement de profil, tu te réinscris auprès des services.
- **« Brancher et démarrer » veut dire attendre plus d’une minute.** La clé donne ses données lentement.
- Sur la carte mémoire prévue, **un des trois modes de fonctionnement ne chiffre pas.** Et une signature prouve que le contenu est inchangé, pas sa paternité. Voir [how-it-works.md](how-it-works.md).

## État des différentes parties

| Partie | État |
| --- | --- |
| Appareil (carte électronique, boîtier) | pas encore construit |
| Micrologiciel | écrit, testé sur l’ordinateur avec des appareils de test, jamais sur la clé |
| Programme `osk` sur l’ordinateur | vérifié contre un appareil de test, jamais contre une vraie clé |
| Windows et Linux | compilé et testé avec l’appareil de test |
| macOS | jamais essayé |
| Navigateur et sites web | jamais essayé |

Technique plus poussée (en anglais, pour les développeurs) : [`docs/`](../) dans le dossier du projet.
