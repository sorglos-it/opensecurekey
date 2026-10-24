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
- **Batterie :** pour le NFC et le Bluetooth sans ordinateur, et pour que l’horloge continue de tourner.

## Chemins vers l’ordinateur

- USB comme appareil de connexion (FIDO2)
- USB comme lecteur de carte à puce (PIV, OpenPGP)
- USB comme clavier (**prévu, pas encore construit**)
- Canal USB pour le programme de gestion
- Lecteur USB, en lecture seule, avec les programmes dessus (**prévu**)
- NFC
- Bluetooth, seulement pour le programme de gestion

## Pourquoi une batterie ?

La puce radio ne peut pas tirer son courant du champ NFC. Une autre clé le peut, parce que là une simple puce de carte répond. Ici, c’est le micrologiciel qui répond. La clé a donc besoin de son propre courant.

- La batterie se recharge par USB.
- **Autonomie :** Sur le papier, environ **7 mois** à dix utilisations par jour. Le calcul est plein d’hypothèses, par exemple pour l’autodécharge. **Rien n’est mesuré.**
- On ne sait pas encore si la batterie tient une session NFC.

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
- **Mac :** jamais essayé.
- **La place dans le micrologiciel** est limitée. On ne sait pas encore si toutes les fonctions y tiennent.

## Le programme et la clé se parlent

- Le programme `osk` parle avec la clé par un canal USB propre.
- Chaque réponse de la clé est traitée comme hostile : formats stricts, limites dures de taille et de profondeur, et le numéro d’ordre doit correspondre.
- L’interface locale n’écoute que sur ton ordinateur. Chaque requête doit venir de l’adresse propre. Un site étranger dans le même navigateur ne peut pas y accéder.
- Chaque requête indique le nom du programme. La clé l’écrit dans le journal. Cela reste un nom, pas une preuve.
