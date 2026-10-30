# Configurer

**État :** La clé n’existe pas encore. Ces étapes décrivent le déroulement prévu. Le programme `osk` ne tourne pour l’instant que contre un appareil de test. Il n’existe pas encore d’assistant de configuration qui guide les étapes 3 à 5 : les textes existent, pas le programme.

## Ce dont tu as besoin

- la clé
- un ordinateur avec Windows ou Linux (macOS : jamais essayé)
- un navigateur
- du temps pour lire tranquillement la question de l’étape 3

Tu n’as besoin d’**aucune installation** ni d’**aucun droit d’administrateur** pour le programme `osk`.

## Étape 1 : Récupérer le programme

- `osk` est **un seul fichier** par système d’exploitation.
- Il range ses réglages **à côté de lui**, pas dans le système.
- Il n’existe pas encore de programme prêt à télécharger. Aucune version n’est construite. Si tu veux le construire toi-même, lis [development.md](development.md).

**Il est prévu** que la clé apporte elle-même les programmes : au branchement, elle montre un petit lecteur en lecture seule.

**Alors ceci vaut : « brancher et démarrer » veut dire attendre plus d’une minute.** La clé donne ses données lentement. C’est normal et ce n’est pas une panne. Combien de temps exactement et pourquoi se trouve dans [how-it-works.md](how-it-works.md), section « Le lecteur avec les programmes prend son temps ».

## Étape 2 : Brancher la clé, lancer le programme

1. Branche la clé sur un port USB.
2. Lance `osk`.
3. Le programme écrit une adresse, par exemple `http://127.0.0.1:8101/`.
4. Ouvre cette adresse dans le navigateur.
5. Sous **Appareils**, ta clé doit apparaître. Si « Aucun appareil trouvé. » s’affiche, [troubleshooting.md](troubleshooting.md) t’aide.

L’adresse mène seulement vers ton propre ordinateur. Personne ne peut accéder à la page depuis l’extérieur.

## Étape 3 : La question unique

L’interface te pose d’abord cette question :

> **Une clé de rechange doit-elle rester possible plus tard ?**

**Cette réponse ne pourra plus jamais être modifiée.** Ce n’est pas un simple conseil de prudence, c’est de la technique : un secret qui est né dans la puce de sécurité n’en ressort jamais. On ne pourra donc pas le copier plus tard sur une deuxième clé.

Il y a deux voies. Les deux ont un prix.

### Voie A : « Oui, rechange possible »

- **Ce que tu obtiens :** Tu pourras créer plus tard une deuxième clé qui sait faire la même chose. Si tu en perds une, tu prends l’autre et tu retrouves tous tes accès.
- **Ce que tu paies :** Ton secret principal est créé **une fois en dehors de la puce de sécurité** pendant la configuration. À ce seul moment, un programme malveillant sur ton ordinateur pourrait le lire. Plus jamais ensuite.

### Voie B : « Non, cette clé uniquement »

- **Ce que tu obtiens :** Tout reste pour toujours dans la puce de sécurité. Aucun secret ne la quitte, à aucun moment.
- **Ce que tu paies :** Il n’y a **aucune rechange.** Si la clé est perdue ou cassée, tu ne peux plus accéder aux comptes créés avec elle. Il faudrait tout recommencer sur chaque service.

### Ce que le programme recommande

- **Pour un usage quotidien :** rechange possible. Qui perd sa seule clé perd sinon tous ses accès.
- **Pour un portefeuille de cryptomonnaie :** tu pourras ajouter plus tard un **second profil sans rechange.**

Aucune des deux voies n’est la plus sûre par principe. Cela dépend de ce que tu crains le plus : ce seul moment sur l’ordinateur, ou la perte de ta seule clé.

### Changer d’avis

- Il n’y a qu’un moyen : **créer un nouveau profil** et te **réinscrire sur chaque service.**
- Une clé peut contenir jusqu’à **quatre profils.** Chacun a son propre code PIN.
- Cela **doit** être ainsi : la connexion avec des passkeys ne connaît pas de profils. Même pour un simple passage d’un profil à l’autre, tu te réinscris auprès des services. Voir [usage.md](usage.md), section « Plusieurs personnes sur une clé ».

Avant de continuer, tu dois cocher : « J’ai compris que cela ne pourra plus être modifié ».

## Étape 4 : Code PIN

- Sans code PIN, rien ne marche. Qui trouve la clé n’accède à rien sans le code PIN.
- Le code PIN a au moins **4 caractères.** Les lettres avec tréma ou accent comptent chacune pour un caractère.
- Le nombre total d’essais ratés avant que le code PIN soit bloqué pour toujours **n’est pas encore fixé.** Trois essais faux d’affilée bloquent d’abord seulement jusqu’au débranchement de la clé. Plus de détails dans [troubleshooting.md](troubleshooting.md).
- C’est la **puce de sécurité** qui compte les essais ratés. Personne ne peut remettre ce compteur à zéro en effaçant la mémoire.
- **Ouvert :** L’endroit exact où tu définis le code PIN pour la première fois n’est pas encore décrit.

## Étape 5 : Sécuriser avant d’utiliser la clé

C’est l’étape la plus importante. Fais-la **tout de suite**, pas « plus tard ».

1. **Créer une seconde clé** (seulement avec la voie A). Voir plus bas.
2. **Enregistrer une sauvegarde.** Voir plus bas.
3. **Écrire la feuille de secours.** Voir plus bas.

**Pourquoi cela ne peut pas attendre « plus tard » :** qui oublie sa phrase secrète principale et perd les deux clés **a tout perdu.** Pas « presque tout » et pas « récupérable avec des efforts ». Il n’y a pas de porte dérobée, pas de procédure de secours et personne qui puisse aider. C’est pourquoi la feuille de secours dans l’assistant de configuration **ne peut pas être sautée :** pas de « passer », pas de « me le rappeler plus tard ». Soit une seconde clé est appariée, soit la feuille est écrite.

**État :** cette obligation est prévue et **n’est pas construite.** Tant que l’assistant n’existe pas, personne ne t’oblige. Fais les deux quand même, et fais-le maintenant.

### Appairer la seconde clé

1. Branche les deux clés.
2. Dans l’interface, choisis **Appairer un appareil de secours.**
3. **Les deux clés affichent le même nombre à six chiffres.** Compare-les.
4. Si les nombres sont **identiques :** appuie sur les deux clés.
5. S’ils sont **différents :** n’appuie pas. Quelqu’un s’est alors placé entre les deux. Rien n’est alors transmis.

Ce qui est transmis :

- Seulement les entrées marquées **« peut être copié ».**

Ce qui n’est **pas** transmis :

- tout le reste
- une phrase de portefeuille crypto : elle est créée de façon à ne pas être copiée sur une seconde clé

L’appairage se fait uniquement par USB sur l’ordinateur. Les deux clés sont branchées en même temps. Il n’existe pas de transfert par radio de clé à clé, parce que la clé n’a pas de batterie.

L’ordinateur sur lequel les deux clés sont branchées ne voit que des données chiffrées.

**Lacune connue :** La partie publique des clés SSH, des certificats et des clés PGP n’est pour l’instant **pas** transmise à la seconde clé et ne figure pas dans la sauvegarde. La clé y fonctionne, mais un programme ne peut la lister que lorsque la partie publique a été enregistrée à nouveau.

### Enregistrer une sauvegarde

1. Dans l’interface, ouvre **Sauvegarde**, puis **Enregistrer une sauvegarde.**
2. Tu choisis une **phrase secrète principale.** Elle a au moins **douze caractères.** Pas des chiffres, des caractères.
3. Prends la proposition de la clé : **huit mots.** C’est plus difficile à deviner que presque tout ce que les gens inventent, et plus facile à écrire sur du papier.
4. Le fichier est chiffré. L’ordinateur ne peut pas le lire.
5. Mets le fichier sur un disque ou un NAS.

**Important :**

- Garde le **fichier et la phrase secrète principale séparés.** Les deux ensemble ouvrent tout.
- Avec le fichier et la phrase secrète principale, on peut configurer une **clé toute neuve.** Le fichier ne dépend pas de la clé qui l’a écrit.
- Le programme avertit au chargement : « Le chargement remplace le contenu de l'appareil. » Ne charge donc pas par erreur.

### Feuille de secours

Une feuille de secours est une feuille de papier que tu gardes **dans un endroit sûr.** Elle contient ta **phrase secrète principale.**

Si tu veux que d’autres personnes puissent aussi y accéder en cas d’urgence, cela se fait avec des **enveloppes :**

- La clé découpe la clé de la sauvegarde en plusieurs parties, par exemple cinq.
- Chaque partie est **une ligne de texte.** Imprime-les, une enveloppe fermée par personne.
- **Trois sur cinq** ensemble ouvrent la sauvegarde, sans phrase secrète principale.
- **Une seule enveloppe ne révèle rien.** Pas même une lettre.
- Une partie mal recopiée se remarque. Aucun faux résultat n’en sort.

Tu fixes le nombre de parties et combien il en faut.

**Ouvert :** Une « feuille de secours » déjà imprimée, produite par le programme, n’est pas décrite comme fonction. Écris toi-même ta phrase secrète principale et l’endroit où se trouve la sauvegarde.

## Mise à jour

### Micrologiciel (le logiciel dans la clé)

1. Dans l’interface, ouvre **Micrologiciel**, puis **Écrire le micrologiciel.**
2. **Ne débranche pas la clé pendant l’écriture.**
3. La clé n’accepte **que des images signées.**

Ton coffre est conservé. Une ancienne version du coffre est lue et réécrite à l’usage. Un coffre venant d’un micrologiciel **plus récent** n’est pas ouvert du tout, plutôt que d’être mal lu.

**État :** L’écriture sur une clé n’a jamais été essayée. L’outil prévu pour cela (`tools/flash.py`) n’est pas vérifié.

### Le programme `osk`

Il n’existe pas encore de fonction de mise à jour. Tu remplaces le fichier par un plus récent.

## Supprimer

- **Programme :** supprimer le fichier `osk`. Les réglages sont à côté, pas dans le système.
- **Vider la clé :** Voir [troubleshooting.md](troubleshooting.md), section « Réinitialiser ». **C’est définitif.**
