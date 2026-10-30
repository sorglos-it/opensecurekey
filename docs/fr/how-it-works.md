# Comment la clé travaille à l’intérieur

**État :** Tout ici est un projet, vérifié sur l’ordinateur. Rien n’a tourné sur une vraie clé.

## Deux puces, deux tâches

- **La puce radio (nRF52840)** parle avec l’ordinateur et le téléphone.
- **La puce de sécurité (SE051)** garde les secrets. Elle ne donne aucune clé.

Pourquoi deux ? La protection de la puce radio a déjà été contournée par le passé avec des pointes de tension. Une puce radio cassée ne révèle donc pas à elle seule quoi que ce soit.

À cela s’ajoutent :

- **Mémoire (64 Mo) :** le coffre, chiffré, et le journal.
- **Écran :** montre ce que tu autorises.
- **Un bouton :** c’est ton autorisation.
- **Pas de batterie :** la clé ne fonctionne que sur le courant USB. L’ordinateur règle l’horloge des codes uniques quand tu la branches.

## Chemins vers l’ordinateur

- USB comme appareil de connexion (FIDO2)
- USB comme lecteur de carte à puce (PIV, OpenPGP)
- USB comme clavier (**prévu, pas encore construit**)
- Canal USB pour le programme de gestion
- Lecteur USB, en lecture seule, avec les programmes dessus (**prévu**)
- NFC (seulement tant que l’USB alimente la clé)
- Bluetooth, seulement pour le programme de gestion (seulement tant que l’USB alimente la clé)

## Pourquoi pas de batterie ?

La clé n’a volontairement pas de batterie. Elle ne fonctionne que tant qu’elle est branchée sur un port USB.

- L’horloge des codes uniques basés sur le temps n’a pas besoin d’alimentation continue. L’ordinateur la règle quand tu branches la clé, et le code ne naît qu’au moment où tu l’utilises. **Ébauche, pas encore construit.**
- Pour l’horodatage d’une signature, la clé demande l’heure à un serveur d’horodatage. Elle n’utilise pas sa propre horloge.
- Une batterie n’aurait apporté que le NFC et le Bluetooth sans ordinateur. En face, il y avait de la longueur en plus (combien : voir `hardware/layout-notes.md`) et une pièce d’usure de quelques années, dans une clé censée durer des décennies.

**À quoi tu renonces :**

- La puce radio ne peut pas tirer son courant du champ NFC. Le NFC et le Bluetooth n’existent donc que tant que la clé est branchée sur un ordinateur.
- Tenir la clé libre contre un téléphone ne marche plus.
- Le Bluetooth vers ta propre application ne marche que tant que la clé est branchée sur un ordinateur. Avec la clé au porte-clés, cela ne marche plus.
- Une deuxième clé ne s’appaire que par USB sur l’ordinateur. Pour cela, les deux clés sont branchées sur le même ordinateur.

C’est un choix voulu. La technique est dans `docs/architecture.md`.

## Les deux voies de configuration

C’est la technique derrière la question unique de [installation.md](installation.md).

### « Cette clé uniquement »

- Pour chaque accès, il naît dans la puce de sécurité **une paire de clés propre.** La partie privée n’existe **nulle part ailleurs.**
- Il n’y a rien à voler et rien à extraire.
- Le prix : chaque accès occupe de la place dans la puce. Le nombre de services est **fini.** Combien il y en a dépend de la version de puce livrée (46 ou 104 ko) et **n’est pas mesuré.** Le programme doit avertir à temps.
- Il n’y a pas de deuxième appareil pour ces accès.

### « Rechange possible » (copié)

- Un secret principal est créé lors de l’appairage **en dehors des deux puces** et écrit dans les deux.
- Le prix : il se trouve brièvement dans la mémoire de la puce radio.
- Après l’écriture, le micrologiciel peut **seulement l’utiliser**, pas le lire, pas l’écraser, pas le sortir. Ce n’est pas de la discipline, c’est la puce qui l’impose ainsi.

### Pourquoi on ne peut pas le changer ?

Un secret né dans la puce ne la quitte jamais, pas même chiffré. On ne peut donc jamais le copier après coup. Un secret principal copié doit naître **avant** sa naissance au bon endroit.

Aucune des deux voies n’est plus sûre par principe. Une seule clé perdue détruit plus que ce que fait en général une clé volée.

## Le code PIN

- La clé ne reçoit jamais le code PIN en clair, seulement une valeur de contrôle qui en est tirée.
- C’est la **puce de sécurité** qui le vérifie et qui compte les essais ratés. Aucun effacement de la mémoire ne remet le compteur à zéro.
- Trois codes PIN faux d’affilée bloquent, jusqu’à ce que tu débranches la clé.
- PIV et OpenPGP utilisent **le même** code PIN. Les codes PIN d’administration (PUK, code PIN admin) sont dans le coffre, chacun avec son propre compteur.
- Chaque essai avec un code PIN d’administration pose la question à l’écran. C’est correct ainsi.

## Le coffre

- Chaque secret est chiffré séparément (AES-256-GCM).
- **Coupure de courant :** Chaque modification est écrite dans un ordre fixe. Après chaque coupure, l’ancien **ou** le nouvel état est là, et tout est lisible. Cela a été testé à chaque point d’écriture.
- La clé répartit l’écriture sur toute la mémoire. Elle ne s’use donc pas à un seul endroit.
- **Chercher sans ouvrir :** Certaines entrées portent une courte marque. Elle dit à quel service l’entrée appartient. Ainsi la clé trouve vite une passkey. La marque est elle-même chiffrée.
- La recherche doit parcourir toutes les parties d’une liste. **Le temps que cela prend sur la vraie puce n’est pas mesuré.**
- Une mise à jour du micrologiciel ne perd aucune donnée.

## La sauvegarde

- Un fichier chiffré à partir duquel on peut configurer une nouvelle clé.
- Sa clé vient de la **phrase secrète principale**, pas de la puce. Sinon la sauvegarde dépendrait de la clé qui a été perdue.
- Sur la clé, les valeurs de calcul sont gardées petites. La force doit donc venir de la phrase secrète principale. C’est pourquoi huit mots.
- Les enveloppes partagent la clé du fichier (Shamir).

## Une seconde clé

- Les deux clés créent une clé éphémère et n’échangent que la partie publique.
- Il en naît le nombre à six chiffres. Qui se place entre les deux crée **deux nombres différents.** Cela a été testé.
- Sans nombre confirmé, aucun octet ne quitte la clé.

## Ce qui protège la clé, et de qui

| Qui | Ce qu’il obtient |
| --- | --- |
| trouve la clé | rien sans le code PIN |
| ouvre la clé et examine la carte électronique | La ligne entre les puces est chiffrée **dès que les clés d’usine sont remplacées** (voir plus bas). La mémoire est chiffrée. |
| a un programme malveillant sur l’ordinateur | Chaque autorisation demande le bouton, et l’écran indique le service. Le programme ne peut pas appuyer. |
| exploite un site web imité | Les passkeys sont liées à la vraie adresse. L’extension ne remplit que sur l’adresse enregistrée. |
| détourne le NFC entre le téléphone et la clé | a quand même besoin de l’appui sur la clé |
| copie un serveur | La clé du serveur reste dans la puce. **Seulement si la puce livrée en est capable.** |

## Ce que la clé ne sait pas faire

Ici c’est écrit sans enjoliver.

### Elle ne protège pas contre

- **Quelqu’un qui te force à appuyer sur le bouton.** Aucun appareil ne peut le reconnaître.
- **Un micrologiciel modifié.** La puce de sécurité protège contre les attaques sur la puce, pas contre un micrologiciel qui te ment. Seul le démarrage signé aide contre cela. Il doit être là **avant** que la protection en lecture soit activée.
- **Une lecture du code PIN en cours de route.** Tu le tapes sur l’ordinateur, donc un programme malveillant peut le lire. Il ne peut pas l’utiliser, parce que chaque autorisation demande le bouton.
- **La perte de tout.** Voir plus bas.

### Empreinte digitale

- La première étape de construction n’a **aucun capteur d’empreinte digitale.** Tu autorises avec le code PIN et le bouton.
- La place pour un capteur est prévue sur la carte électronique.
- Avec FIDO2, une empreinte digitale **remplace** le code PIN. Ce n’est pas une protection en plus.
- **Une détection du vivant n’est prouvée pour aucun capteur examiné.** Qui craint un doigt moulé ne gagne rien avec aucun de ces capteurs.
- Les données d’empreinte ne seraient jamais dans le coffre et ne pourraient jamais aller sur une seconde clé.
- Aucun navigateur et aucun site web ne peut enregistrer un doigt. Cela se ferait sur la clé elle-même.

### Perte

**Qui perd la phrase secrète principale et toutes les clés perd tout.**

Si tu perds en même temps la clé, la seconde clé, le fichier de sauvegarde et les enveloppes, le contenu est perdu. Personne ne peut aider. Il n’y a pas de porte dérobée qu’on pourrait voler.

### Autres choses à savoir

- **Aucun test extérieur.** Personne en dehors du projet n’a regardé le chiffrement.
- **Aucun test de certification FIDO** n’a été lancé.
- **La puce arrive avec des clés d’usine publiquement connues.** Lors de la première mise en service, elles **doivent** être remplacées. D’ici là, la ligne entre les puces n’est pas protégée. C’est décrit et prévu, mais **jamais exécuté sur une puce.**
- **Qui peut réécrire la mémoire directement** ne peut rien lire. Il peut cependant remettre un état plus ancien. Il manque encore un compteur dans la puce contre cela.
- Le **compteur de signatures** de la connexion repart de zéro à chaque mise sous tension. Certains sites pourraient y voir une clé copiée.
- La clé envoie une **auto-attestation**, pas un certificat du fabricant. C’est bon pour la protection des données, mais certains sites d’entreprise exigent plus.
- Le fait que la puce livrée autorise l’**accord de clé** (ECDH) dépend de son réglage d’usine. Si elle ne l’autorise pas, **le mode serveur avec la clé dans la puce disparaît.** La signature continue de marcher. Le programme le dit alors honnêtement, au lieu de ressembler à une erreur de code PIN.
- **La clé privée n’existe pas comme fichier.** Les programmes qui exigent un fichier de clé ne peuvent pas travailler avec la clé. Voir [usage.md](usage.md), section « Un logiciel qui exige un fichier de clé ».
- **Mac :** jamais essayé.
- **La place dans le micrologiciel** est limitée. On ne sait pas encore si toutes les fonctions y tiennent.

## Le lecteur avec les programmes prend son temps

**État :** calculé, pas mesuré sur l’appareil. Le lecteur est prévu et n’est pas construit.

Quand on la branche, la clé montre un petit lecteur en lecture seule avec les programmes d’administration. Qui l’ouvre attend.

**Les trois chiffres à ce sujet sont ici. Tous les autres endroits du manuel renvoient ici.**

| Quoi | Chiffre |
| --- | ---: |
| Ce que la puce radio fait passer par l’USB | environ **400 kB par seconde** |
| La taille des programmes | **29,10 Mo** (mesuré le 23.10.2026 sur l’image terminée) |
| Donc la durée de la lecture | environ **73 secondes** |

« Brancher et démarrer » veut dire en réalité : brancher, **attendre plus d’une minute,** démarrer. Qui ne le sait pas croit que la clé est cassée et la débranche.

Pourquoi si lent ? La puce radio est sur un port USB de l’ancien standard 1.1. Sur le papier cela donne 1,5 Mo par seconde, mais la clé doit d’abord sortir chaque morceau de sa mémoire et répondre. Ce qui reste, ce sont les 400 kB. Les 400 kB sont **mesurés** sur une carte de développement ; les deux autres chiffres en sont calculés.

Les programmes grossissent. Si le fichier grossit, l’attente grossit dans la même proportion.

## Le support de données chiffré

**État : du papier.** Il n’y a pas de puce, pas de carte et pas une ligne de programme pour cela. Cette section est ici pour que personne ne croie plus tard quelque chose de faux, pas parce que cela existe.

Une carte mémoire sur la clé est prévue, de 8 à 16 Go, pour tes propres fichiers.

**La carte a toujours besoin d’un openSecureKey.** Dans un lecteur de cartes ordinaire, on n’en fait rien. Elle porte un format qui lui est propre, pas un format Windows. Ce **n’est pas** un support que l’on met dans la main de quelqu’un qui n’a pas de clé.

### Trois modes de fonctionnement, et l’un d’eux n’est pas secret

C’est l’endroit où il est le plus facile de se nuire à soi-même. Voici donc les trois côte à côte :

| Mode | Nom | Qui peut lire | Contenu secret ? |
| --- | --- | --- | --- |
| 1 | **seulement cette clé** | seulement la clé qui a écrit | **oui** |
| 2 | **n’importe laquelle de nos clés, avec le mot de la carte** | toute openSecureKey qui a le mot de la carte | **oui** |
| 3 | **n’importe laquelle de nos clés, seulement scellé** | **toute** openSecureKey, sans mot de passe | **non** |

**Le mode 3 ne chiffre rien.** Il scelle seulement : on reconnaît si quelqu’un a modifié le contenu. Le lire, n’importe qui le peut dès lors qu’il a une openSecureKey quelconque.

**Qui croit avoir chiffré et n’a que scellé donne ses secrets.** C’est la pire faute que cet appareil puisse commettre, et elle ne naît pas d’une attaque mais d’une confusion.

C’est pourquoi il est prévu :

- L’affichage nomme le mode en clair **avant** d’écrire, et ton appui confirme exactement ce texte. Au mode 3 il y est écrit « **OUVERT — seulement scellé** ».
- Le mode 3 se présente nettement autrement à l’écran, pas la même ligne avec un autre mot dedans.
- Tant que la carte est en place, le mode reste à l’écran.
- **Changer de mode veut dire réécrire la carte.** Il n’existe pas de « chiffrer après coup ».

### L’authenticité n’est pas la paternité

La signature sur la carte prouve : **le contenu est inchangé** depuis qu’il a été signé.

Elle ne prouve **pas** de qui il vient.

Qu’une carte vienne d’une personne précise ne découle de la signature **que** si tu connais **à l’avance** la clé publique de cette personne : d’une rencontre, d’une carte de visite, d’une autre voie. Sinon tu sais seulement ceci : une openSecureKey quelconque a signé ici.

| Affirmation | couverte par la signature ? |
| --- | --- |
| « Ce contenu est inchangé. » | **oui** |
| « Une openSecureKey l’a signé. » | oui |
| « *Cet* appareil l’a signé. » | oui |
| « Cet appareil appartient à une personne précise. » | **non** |
| « Le contenu vient de cette personne. » | **non** |

Il est donc prévu : au branchement, l’affichage nomme la clé de l’expéditeur sous forme d’une courte suite de caractères et ajoute « **expéditeur inconnu** » ou le nom que tu as donné une fois à cet expéditeur.

### La vérification se fait à la lecture, pas au branchement

Vérifier **entièrement** une carte de 16 Go veut dire la lire en entier. Cela dure **environ une heure et demie** (calculé avec la vitesse de la carte elle-même, pas celle du port USB de la section au-dessus : ce sont deux chemins différents). Personne n’attend cela au branchement, et c’est pourquoi la clé ne le fait pas.

À la place, elle vérifie **chaque bloc au moment où elle le lit.**

Le bon côté : aucun fichier que tu ouvres ne te parvient sans vérification.

Le mauvais côté : **un bloc modifié dans un fichier que personne n’ouvre ne se remarque pas.**

C’est pourquoi la clé ne dit jamais « carte vérifiée ». Ce serait un mensonge. Elle dit « ce bloc vérifié », et c’est la vérité.

## Le programme et la clé se parlent

- Le programme `osk` parle avec la clé par un canal USB propre.
- Chaque réponse de la clé est traitée comme hostile : formats stricts, limites dures de taille et de profondeur, et le numéro d’ordre doit correspondre.
- L’interface locale n’écoute que sur ton ordinateur. Chaque requête doit venir de l’adresse propre. Un site étranger dans le même navigateur ne peut pas y accéder.
- Chaque requête indique le nom du programme. La clé l’écrit dans le journal. Cela reste un nom, pas une preuve.
