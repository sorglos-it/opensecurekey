# Utiliser

**État :** Rien de tout cela n’a tourné sur une vraie clé. Pour chaque fonction, il est écrit jusqu’où elle est vérifiée.

## La règle pour tout

- **Rien ne sort si tu n’appuies pas sur la clé.**
- L’écran de la clé indique **ce que** tu autorises : le service, le destinataire, le montant.
- Ce que la clé ne peut pas afficher de façon lisible, elle ne l’autorise pas.
- Un programme malveillant sur l’ordinateur peut poser la question. Il ne peut pas appuyer sur le bouton, et il ne peut pas changer ce que dit l’écran.

## Le bouton

La clé a **un** bouton et trois façons d’appuyer.

| Appui | Durée | Signification |
| --- | --- | --- |
| court | moins de 0,8 seconde | page suivante. Sur la page de question : **Non.** |
| long | à partir de 0,8 seconde | vers la page de question. Là : **Oui.** |
| double | deux appuis courts, rapprochés | tout relire depuis la page 1 |

Voici comment se déroule une autorisation :

1. **Lire.** Chaque page montre le texte exact. Avec un appui court, tu fais défiler.
2. **Décider.** Un appui long affiche la page de question. Là : long = Oui, court = Non.
3. **Supprimer demande deux fois.** Deux appuis longs sur deux pages de question.

Un appui long sur une page de lecture n’autorise **rien.** Il amène seulement la page de question. Personne n’autorise quelque chose qu’il n’a pas vu.

Après 30 secondes sans appui, cela compte comme « Non ».

Les noms et les adresses longs ne sont **jamais coupés.** Ils s’étalent sur plusieurs pages. Une adresse de destinataire coupée serait pire que pas d’écran du tout.

**État :** Sur un vrai écran, rien n’a encore jamais été affiché. Personne ne sait si 0,8 seconde est une bonne sensation dans la main.

## Se connecter à un site web (passkey)

**État :** Jamais essayé dans un navigateur.

1. Sur le site web, choisis « Se connecter avec une clé de sécurité » ou « Passkey ».
2. Si l’ordinateur demande le code PIN : saisis le code PIN.
3. L’écran de la clé indique le service.
4. Lis. **Appuie longtemps** pour aller à la page de question, puis **appuie longtemps** pour Oui.

Bon à savoir :

- Une passkey ne vaut **que pour la vraie adresse** du site. Sur un site imité, elle ne marche pas.
- La clé comprend les passkeys, l’ancienne connexion U2F et l’extension qui permet à des programmes comme Bitwarden, BitLocker et LUKS de dériver une clé (`hmac-secret`).
- **Les anciens sites qui ne connaissent que U2F** refusent peut-être la clé. Elle n’envoie pas d’attestation.
- Certains sites d’entreprise exigent un certificat d’attestation. La clé n’en a pas.
- Par NFC, chaque autorisation demande aussi le bouton.

## Approcher du téléphone (NFC)

**Seulement avec le courant USB.** La clé n'a pas de batterie. Le NFC ne marche que tant qu'elle est branchée sur un port USB : le téléphone vient vers la clé, pas l'inverse. Tenir la clé libre contre un téléphone ne marche plus. On n'a pas vérifié si un simple chargeur USB sans ordinateur suffit.

**État :** jamais essayé avec un vrai téléphone. La portée n'a pas été mesurée.

- Tiens la clé **plus près du lecteur qu'une carte.**
- Cela vient de la forme de l'appareil. Ce n'est pas une panne. Dans le petit boîtier, l'antenne est plus petite que celle d'une carte.
- La distance exacte ne sera connue qu'après une mesure sur l'appareil terminé. C'est pourquoi il n'y a pas de chiffre ici.
- Si cela ne marche pas tout de suite : déplace la clé un peu, lentement. N'appuie pas plus fort.
- Même par NFC, chaque autorisation demande le bouton sur la clé.

## Récupérer un mot de passe

**État :** vérifié contre l’appareil de test, jamais contre une vraie clé.

### Dans l’interface

1. Lance `osk` et ouvre l’adresse dans le navigateur.
2. **Déverrouille** la clé (code PIN).
3. Cherche l’entrée et ouvre-la.
4. Choisis **Afficher.**
5. Confirme sur la clé.
6. Avec **Copier**, tu le mets dans le presse-papiers. Il est vidé de nouveau après environ **20 secondes.**

### Par commande

Le code PIN n’est jamais donné en paramètre. Il arrive par l’entrée standard, parce que les paramètres sont visibles par d’autres programmes.

```powershell
osk unlock
```

```powershell
osk list
```

```powershell
osk get "Mein Konto"
```

Si un nom correspond à plusieurs entrées, le programme ne devine pas. Il dit que le nom n’est pas univoque.

### Faire remplir dans le navigateur

Il existe une extension de navigateur. Elle remplit un champ de connexion **seulement sur l’adresse enregistrée.** Pour une autre adresse, elle refuse. C’est la protection contre les sites imités, pas un défaut de confort. Sur une page non chiffrée (sans https), elle ne donne rien.

**État :** Un champ de connexion a été rempli dans un vrai Chromium, avec l’appareil de test. Avec une clé, jamais.

### Parties publiques sans appui

La partie **publique** d’une clé SSH, d’un certificat ou d’une clé PGP sort **sans** appui sur la clé. C’est voulu : si tu appuyais pour chaque nom, tu finirais vite par appuyer sans regarder. L’unique appui qui compte ne vaudrait alors plus rien : celui avant de signer.

## Un code pour la connexion en deux étapes (2FA)

**État : ouvert.**

- La clé peut stocker la valeur de départ des codes uniques, comme entrée « Code unique (temps) » ou « Code unique (compteur) ».
- La clé n’a pas de batterie. L’ordinateur règle l’horloge quand tu la branches (**prévu, pas encore construit**). Si l’horloge de l’ordinateur est fausse, les codes le sont aussi. Les codes basés sur le temps ont besoin de l’heure.
- **La façon dont le code final à six chiffres est affiché dans l’interface ou sur la clé n’est pas décrite dans la documentation technique.** Je n’écris donc rien là-dessus ici.
- La commande `osk codes` ne montre **pas** ces codes, mais la liste des codes de sortie pour les scripts.
- KeePassXC et VeraCrypt fonctionnent avec une méthode de Yubico. Elle manque encore : **ces deux programmes ne marchent pas dans l’état actuel.**

## Signer un fichier ou un message

**État :** Le cœur de calcul est écrit et vérifié sur l’ordinateur. Jamais essayé avec un vrai programme de carte à puce (GnuPG, OpenSC).

La clé se comporte comme une **carte à puce.** Windows, Linux et macOS n’ont besoin d’aucun pilote pour cela.

- Elle peut se présenter comme **carte PIV** et comme **carte OpenPGP.**
- Les clés sont dans la puce de sécurité. Elles ne la quittent jamais.
- **Chaque signature demande le bouton.** L’écran indique le service.
- Avec PIV, le code PIN doit être revérifié **juste avant chaque** signature, à l’emplacement « 9C ».
- Les courbes elliptiques sont prises en charge (P-256 et P-384). **RSA pas encore** (le cœur de calcul le refuse).

Déroulement :

1. Demande au programme (par exemple GnuPG) de signer avec la clé de la carte.
2. Saisis le code PIN quand il est demandé.
3. Sur la clé, lis le nom, **appuie longtemps**, **appuie longtemps.**

### Un logiciel qui exige un fichier de clé

**C’est là que la plupart échouent. Lis ceci avant de chercher.**

**Le certificat, la clé le donne.** Un certificat est public : le transmettre ne coûte rien. Pour tout ce qui n’a besoin que de l’attestation, cela suffit.

**La clé privée n’existe pas comme fichier. Jamais.** Il n’y a pas d’export, pas de case à cocher pour cela, pas de mot de passe qui la libère. Ne cherche pas.

Ce **n’est pas un manque mais le but de l’appareil :** ce qui existe comme fichier est copiable. Une clé que l’on peut copier n’est plus une clé matérielle.

**La conséquence, dite franchement :** un logiciel qui exige impérativement un fichier de clé — un `.pem` avec `PRIVATE KEY` dedans, ou un `.pfx` avec mot de passe — **ne peut fondamentalement pas se servir de cet appareil.** Ni par un détour, ni par un réglage.

**L’issue s’appelle PKCS#11.** C’est une interface fixée pour les clés qui restent dans un appareil. Les programmes qui la connaissent acceptent, au lieu d’un chemin de fichier, une **adresse de clé**, et elle a cette allure :

```
pkcs11:object=ma-cle
```

Savent le faire, entre autres, `openssl`, le serveur web nginx, Java, beaucoup de programmes VPN et les outils de signature habituels. Qui sait faire du PKCS#11 n’a pas besoin de fichier.

**Qui ne sait pas reste dehors.** Alors il n’y a qu’une aide : prendre un programme qui parle PKCS#11, ou rester pour ce seul usage avec un fichier de clé sur le disque dur, avec tout ce que cela veut dire.

La bibliothèque pour cela s’appelle `osk-pkcs11` (en `.dll` et `.so`). **État :** avec le simulateur, `openssl` a ainsi vérifié une signature comme « Verified OK ». Avec une vraie clé, jamais.

### Avec SSH et Git

**État :** Vérifié avec l’appareil de test. `git push` via l’agent a fonctionné sous Windows et Linux.

1. Lance l’agent :

```powershell
osk agent
```

2. Affiche les clés du coffre :

```powershell
osk keys
```

3. Tu confirmes chaque signature sur la clé.

Pour les programmes qui parlent PKCS#11, il existe une bibliothèque (`osk-pkcs11`, en .dll et .so). Avec elle, `openssl` a vérifié une signature comme « Verified OK », avec l’appareil de test.

### Donner un secret à un programme

```powershell
osk run NAME=eintrag -- programm
```

Le secret va seulement au programme lancé. Le shell à côté ne le voit pas.

## Se connecter à l’ordinateur

**Cela ne marche pas encore.**

- Se connecter à Windows, Linux ou au Mac avec la clé : pas construit.
- Déverrouiller le disque dur et la clé comme clavier (la clé tape ton mot de passe) : pas construit.
- Ces parties ont besoin une fois des droits d’administrateur pour la configuration. C’est l’affaire du système d’exploitation.

Seuls sont construits et vérifiés avec l’appareil de test : agent ssh, ligne de commande, PKCS#11, extension de navigateur et une interface locale pour tes propres programmes.

## Portefeuille crypto (Bitcoin et Ethereum)

**État :** Vérifié sur l’ordinateur. **Aucun vrai paiement n’a jamais été envoyé.**

- La clé ne signe un paiement que si le **destinataire, le montant et les frais** étaient lisibles sur l’écran.
- Si l’un d’eux manque, la clé dit **Non**, sans interroger la puce.
- Ne sont **pas lisibles**, entre autres, les contrats, les montants de jetons et les Typed Data. La clé ne signe ces choses que si tu as **activé** « signer à l’aveugle » sur la clé.
- Un demi-paiement n’est jamais signé. Si une partie échoue, tout échoue.
- Non pris en charge : les entrées Taproot.
- Ta phrase de portefeuille est créée de façon à ne pas être copiée sur une seconde clé et à n’aller qu’aux ordinateurs appairés.

## Programme seulement avec la clé (dongle) et serveur

**État :** Seulement avec l’appareil de test. Le côté clé n’existe pas encore.

- **Niveau 1** (la clé dit « je suis là ») arrête quelqu’un qui ne fait que **copier** un programme. Qui sait démonter un programme enlève la vérification.
- **Niveau 2** (les données sont scellées avec une clé issue de la puce) est fondamentalement plus fort : sans la clé, les données ne sont pas lisibles.
- Un serveur peut signer avec la clé au lieu d’un fichier de clé. **Sans autorisation**, un serveur s’arrête, parce que personne n’appuie à 3 heures du matin. L’autorisation est **désactivée**, nomme les clés, expire après **24 heures au plus** et est consignée. **Avec** l’autorisation, un programme malveillant sur ce serveur peut signer.

Plus de détails : [how-it-works.md](how-it-works.md).

## Le journal de la clé

La clé tient son propre journal chiffré. Il montre **quel programme** a récupéré **quand** quel secret. Il se trouve dans la clé, pas sur l’ordinateur. Il contient les quelques centaines derniers événements.

```powershell
osk log
```

Le nom du programme dans le journal est une **affirmation** du programme, pas une preuve.

Les secrets ne figurent jamais dans le journal.

## Plusieurs personnes sur une clé

Une clé a jusqu’à **quatre profils**, chacun avec son propre code PIN. Le code PIN de l’un n’en ouvre aucun autre. Un profil verrouillé ne montre rien, pas même les noms des entrées. Place : **768 entrées par profil**, **3072 dans la clé.**

**Une limite qui ne vient pas de la clé :** la connexion avec des passkeys (FIDO) ne connaît **aucun profil.** Cette technique part du principe qu’une clé appartient à une personne. La clé n’y peut rien changer.

Ce que cela veut dire au quotidien :

- Si tu changes de profil, tu dois **te réinscrire auprès des services.** Les passkeys de l’autre profil n’y sont pas disponibles.
- Un profil n’est donc pas un second visage pour le même service, mais un jeu d’accès séparé.
- Qui veut passer souvent d’un profil à l’autre s’en sort mieux avec **deux clés**.

**État :** jamais essayé dans un navigateur.

## Types d’entrées

Identifiant, Code unique (temps), Code unique (compteur), Clé SSH, Certificat, Clé PGP, Clé API, Codes de récupération, Phrase de portefeuille, Clé de licence, Carte bancaire, Identité, Wi-Fi, Note, Fichier.

Chaque entrée porte trois marques, que tu règles toi-même :

| Marque | Effet |
| --- | --- |
| peut être copié | va sur ta seconde clé, et seulement là |
| seulement ordinateurs appairés | n’est donné qu’à un ordinateur appairé |
| protégé en écriture | ne peut être ni modifié ni supprimé, pas même par toi, jusqu’à ce que tu le lèves |

## Une carte mémoire sur la clé

**Cela n’existe pas encore.** Une carte mémoire pour tes propres fichiers est prévue.

Deux choses à son sujet, à connaître **avant** qu’elle existe, car les deux peuvent te coûter des secrets :

- Sur **trois modes de fonctionnement**, un **ne** chiffre **pas**. Il scelle seulement, et toute openSecureKey peut lire.
- Une signature prouve que le contenu est **inchangé**, pas **de qui** il vient.

Les deux en détail dans [how-it-works.md](how-it-works.md), section « Le support de données chiffré ».
