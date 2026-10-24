# Quand quelque chose ne marche pas

**État :** Rien de tout cela n’a été vécu sur une vraie clé. Les messages viennent du programme. Les conseils généraux sont marqués comme tels.

## La clé est perdue

C’est la section qui compte. Lis-la **maintenant**, pas seulement quand c’est arrivé.

### D’abord : que peut faire celui qui trouve la clé ?

- **Rien, sans le code PIN.**
- C’est la puce de sécurité qui compte les essais ratés. Aucun effacement ne remet le compteur à zéro.
- Même avec le code PIN, chaque autorisation demande le bouton de la clé.
- Il n’y a **aucun verrouillage à distance.** La documentation technique n’en parle pas.
- Conseil général, pas venant de la clé : si tu as un doute, change les mots de passe de tes comptes les plus importants.

### Ensuite : qu’as-tu ? Cherche ton cas.

#### Cas 1 : Tu as une seconde clé (voie « Rechange possible »)

1. Prends la seconde clé.
2. Elle sait faire la même chose que la perdue, **pour tout ce qui était marqué « peut être copié ».**
3. Ta phrase de portefeuille n’en faisait pas partie. Elle ne va jamais sur une seconde clé.
4. Crée dès que possible une **nouvelle** seconde clé. Sinon tu n’en as de nouveau qu’une.
5. La partie publique des clés SSH, des certificats et des clés PGP n’est pour l’instant **pas** sur la seconde clé. Enregistre-la à nouveau, sinon un programme ne peut pas lister la clé.

#### Cas 2 : Tu as une sauvegarde et la phrase secrète principale

1. Procure-toi une nouvelle clé.
2. Configure-la. Voir [installation.md](installation.md).
3. Charge la sauvegarde. Pour cela, tu as besoin du fichier **et** de la phrase secrète principale.
4. Si tu n’as plus la phrase secrète principale, mais des enveloppes : **Trois sur cinq** (ou le nombre que tu as choisi) ouvrent le fichier sans phrase secrète principale.
5. Attention : le programme avertit que le chargement remplace le contenu de l’appareil.

**Ouvert :** On ne sait pas de façon sûre, d’après la documentation technique, si une nouvelle clé peut aussi réutiliser tes **passkeys** à partir de la sauvegarde. Pour les passkeys, compte sur la seconde clé.

#### Cas 3 : Tu n’as rien de tout cela, et tu as choisi « cette clé uniquement »

- Pour les accès créés avec elle, il n’y a **aucune rechange.**
- Pour chaque service, tu dois recommencer depuis le début, par la voie que le service propose pour les accès perdus.
- La clé elle-même ne peut pas t’aider pour cela.

#### Cas 4 : Tout est perdu : les deux clés et la phrase secrète principale

- **Qui perd la phrase secrète principale et toutes les clés perd tout.**
- Personne ne peut récupérer le contenu. Le projet non plus. Il n’y a pas de porte dérobée qu’on pourrait voler.

### Comment prévenir

Ces quatre choses, de préférence tout de suite lors de la configuration :

1. **Appairer une seconde clé** (seulement avec la voie A).
2. **Enregistrer une sauvegarde**, à ranger séparément de la phrase secrète principale.
3. **Feuille de secours :** la phrase secrète principale sur papier, dans un endroit sûr. Note aussi où se trouve la sauvegarde.
4. **Enveloppes** pour des personnes en qui tu as confiance : une partie par enveloppe, trois sur cinq ouvrent.

Ne garde **jamais** tout au même endroit. Le fichier et la phrase secrète principale ensemble ouvrent tout.

L’ordre de ta sécurité :

- Seconde clé + sauvegarde + feuille : une panne n’est pas un drame.
- Seulement une seconde clé : un incendie ou le vol du sac emporte les deux.
- Rien de tout cela : une clé perdue est une perte totale.

## La clé n’est pas reconnue

Dans le programme, sous **Appareils**, il est écrit « Aucun appareil trouvé. »

Conseils généraux (pas essayés sur la clé) :

1. Essayer un autre port USB.
2. Un autre câble, ou directement sur l’ordinateur, sans répartiteur.
3. Débrancher la clé, la rebrancher, relancer le programme.
4. La batterie est-elle peut-être vide ? La laisser charger.

Ce qui compte aussi :

- **macOS :** Sans le mode de construction particulier (cgo), le programme démarre certes, mais **ne trouve aucune vraie clé** et le dit aussi.
- Si tu veux utiliser la clé de test, saisis son adresse dans les réglages. Voir [settings.md](settings.md).
- L’identifiant de la clé (numéro de fabricant et de produit) est **provisoire** et n’est pas encore accordé avec le micrologiciel.

Messages à ce sujet :

| Message | Signification | Que faire |
| --- | --- | --- |
| Aucun appareil trouvé. | Aucune clé ne répond. | Voir plus haut. |
| Les appareils de cet ordinateur ne peuvent pas être listés (…). | Le système refuse la liste. | Relancer le programme. Si cela n'aide pas, signale-le avec le texte entre parenthèses. |
| L'appareil ne peut pas être ouvert (…). | Cause possible : un autre programme utilise la clé, ou le système ne le permet pas. | Fermer les autres programmes qui utilisent la clé. |
| Impossible de lire depuis l'appareil (…). / Impossible d'écrire vers l'appareil (…). | La liaison a été coupée. | Rebrancher la clé. |
| L'appareil n'a pas répondu à temps. / L'appareil a mis trop de temps. | La clé attend peut-être ton appui. | Regarde l'écran de la clé. |
| La liaison avec l'appareil est perturbée. | Comme ci-dessus. | Rebrancher la clé. |
| Ce système d'exploitation n'est pas encore pris en charge. | Il n'existe pas encore de voie pour ce système. | Prendre Windows ou Linux. |
| L'appareil utilise une autre version du protocole. | Le programme et le micrologiciel ne vont pas ensemble. | Mettre à jour le programme ou le micrologiciel. |

## Le code PIN est bloqué

- **Trois codes PIN faux d’affilée** bloquent la clé jusqu’à ce que tu la **débranches et la rebranches.** Ensuite tu peux réessayer.
- Le compteur est dans la puce de sécurité. Réécrire ou effacer la mémoire ne le remet **pas** à zéro.
- **Le nombre total d’essais ratés** avant que le code PIN soit bloqué pour toujours, et ce qui se passe ensuite, ne figure **pas** dans la documentation technique. Ne devine donc pas. Cherche d’abord le code PIN.
- Pour le côté carte à puce (PIV, OpenPGP), il y a en plus un PUK et un code PIN admin. Ils sont dans le coffre, chacun avec son propre compteur.

| Message | Signification | Que faire |
| --- | --- | --- |
| L'appareil est verrouillé. | Tu n’as pas encore déverrouillé. | **Déverrouiller.** La clé demande l’autorisation sur son propre écran. |
| L'accord sur l'appareil manque. | Tu n’as pas confirmé (ou avec « Non »), ou cela a pris plus de 30 secondes. | Réessayer. Appuyer longtemps, puis appuyer longtemps. |

### Réinitialiser

En cas d’urgence, la partie connexion (FIDO) peut être réinitialisée.

- **C’est définitif.** Le secret principal devient neuf, **chaque ancienne connexion ne vaut plus rien.** Tu dois te réinscrire sur chaque service.
- La réinitialisation ne marche **pas par NFC.**
- Supprimer sur la clé demande deux fois.
- On ne sait pas, d’après la documentation technique, si cela résout aussi un code PIN bloqué.

## Le navigateur n’accepte pas la clé

**État :** Jamais essayé dans un navigateur. Cette liste ne connaît que les **limites connues.**

- **Les anciens sites qui ne connaissent que U2F** n’acceptent peut-être pas la clé. Elle n’envoie pas de certificat d’attestation.
- **Les sites d’entreprise avec certificat obligatoire** la refusent peut-être. Elle s’atteste elle-même.
- La clé se présente comme **FIDO 2.0 et 2.1.** Les commandes pour gérer les passkeys (lister, supprimer) et pour enregistrer des empreintes digitales dans le navigateur **n’existent pas.**
- La clé ne signale **aucune vérification d’utilisateur intégrée** (`uv`) tant qu’aucun capteur n’est installé. C’est correct ainsi. Le code PIN est signalé séparément.
- Par **Bluetooth**, il n’y a **aucune** connexion dans le navigateur. Les navigateurs ne prennent pas FIDO en charge de façon fiable par Bluetooth. Prends l’USB ou le NFC.
- Le **compteur de signatures** repart de zéro à chaque mise sous tension. Un site qui exige des compteurs croissants pourrait prendre la clé pour une copie.
- L’**identifiant du fabricant** (AAGUID) est provisoire.

Messages de l’extension de navigateur :

| Message | Signification | Que faire |
| --- | --- | --- |
| Cette entree appartient a une autre adresse. | L’entrée est enregistrée pour un autre site. | **C’est la protection contre le hameçonnage.** Vérifie l’adresse. Si elle est correcte, change l’adresse dans l’entrée. |
| Cette page n'est pas chiffree. Aucun secret ne va ici. | Le site n’utilise pas https. | Ne pas remplir. |
| Le canal vers le navigateur a ete coupe. | L’extension a perdu la connexion. | Relancer le navigateur. |
| Le fichier d'enregistrement n'a pas pu etre ecrit. | L’extension n’a pas pu s’enregistrer. | Vérifier les droits dans le dossier. |
| Ce n'est pas un identifiant d'extension (…). / Je ne connais pas ce navigateur (…). | Mauvaise indication lors de l’enregistrement. | Vérifier l’identifiant et le nom du navigateur. |

## Messages pour les entrées et la sauvegarde

| Message | Signification | Que faire |
| --- | --- | --- |
| Une entrée avec ce nom existe déjà. | Nom en double. | Choisir un autre nom. |
| Il n'y a plus de place sur l'appareil. | La mémoire du profil est pleine (768 entrées). | Supprimer d’anciennes entrées. |
| L'appareil ne connaît pas cette entrée. | Entrée supprimée ou mal nommée. | Recharger la liste. |
| le nom correspond a plusieurs entrees | Pas univoque. | Nommer plus précisément. |
| Cet appareil ne sait pas encore faire cela. | La fonction manque dans ce micrologiciel. | Mettre à jour le micrologiciel, s’il y en a un plus récent. |
| Le fichier de sauvegarde est vide ou trop grand. | Mauvais fichier. | Choisir un autre fichier. |
| Les données de l'appareil n'ont pas la forme attendue. | Les données sont abîmées, ou elles viennent d’un micrologiciel **plus récent.** Une version plus récente n’est jamais mal lue, mais pas ouverte du tout. | Mettre à jour le micrologiciel. Si cela n’aide pas, charger la sauvegarde. |
| L'image du micrologiciel est vide. | Aucun fichier choisi. | Choisir un fichier. |
| Le fichier de réglages ne peut pas être lu. / Le fichier de réglages est trop grand. / Le fichier de réglages contient une erreur. | Le fichier `config_osk.json` est abîmé. | Le supprimer. Le programme prend alors les valeurs par défaut. |

## Messages pour l’agent ssh et les interfaces

| Message | Signification | Que faire |
| --- | --- | --- |
| Cette place est deja prise (…). | Un autre agent ou service tourne déjà. | Arrêter l’autre. |
| Cette cle est chiffree par un mot de passe. | La clé SSH dans le coffre est encore protégée. | La mettre déchiffrée dans le coffre. |
| Cette cle est trop courte et n'est plus sure. | Clé faible. | Créer une nouvelle clé. |
| Je ne sais pas traiter ce type de cle (…). | Non pris en charge. | Choisir un autre type. |
| Cette cle a ete demandee, mais elle n'est pas dans le coffre. | Le programme demande une clé que tu n’as pas. | Mettre la clé dans le coffre. |
| L'acces n'a pas ete autorise. / Aucune reponse n'est venue a la question de l'autorisation. | Tu as refusé ou tu n’as rien appuyé. | Réessayer. |

Les codes de sortie pour les scripts s’affichent avec :

```powershell
osk codes
```

## Messages pour la protection par dongle

| Message | Signification | Que faire |
| --- | --- | --- |
| Aucune clé de sécurité trouvée. Branchez-la. | La clé n’est pas branchée. | La brancher. |
| La clé branchée n’est pas celle qui est configurée. | Le programme appartient à une autre clé. | Brancher la bonne clé. |
| Le fichier ne peut pas être ouvert : mauvaise clé ou fichier modifié. | La clé ne correspond pas au fichier scellé, ou le fichier a été modifié. | Prendre la clé qui l’a scellé. |
| La licence a expiré. | La licence était authentique et elle est terminée. | Se procurer une nouvelle licence. |
| Il n’y a pas de licence pour … sur cette clé de sécurité. | Pas de licence pour le produit. | Mettre la licence sur la clé. |
| Cette puce ne peut pas faire d’accord de clé. | La puce a été réglée ainsi en usine. Le mode serveur avec la clé dans la puce disparaît. La signature continue de marcher. | Cela ne peut pas être changé. |

## Cela ne marche pas encore

Pour que tu ne cherches pas longtemps :

- **KeePassXC et VeraCrypt** avec la clé
- **Se connecter à l’ordinateur,** déverrouiller le disque dur, la clé comme clavier
- une feuille de secours déjà imprimée par le programme
- **macOS**

## Si tu ne le trouves pas

Signale-le dans le projet sur GitHub (sorglos-it/opensecurekey) et indique : le système d’exploitation, la version de `osk`, le message exact, et ce que tu as fait avant. Les mots de passe et les codes PIN n’ont jamais leur place dans un signalement.
