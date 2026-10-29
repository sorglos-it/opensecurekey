# Wo sich Applet 3.x und 7.2 unterscheiden

Ausgewertet am 24.10.2026 aus NXPs Middleware (github.com/NXP/plug-and-trust,
BSD-3-Clause, Stand des flachen Klons). **Nur die Fundstellen, kein Code.**
Der Klon wurde danach geloescht; diese Liste ist das Ergebnis.

Der APDU-Code liegt dort im Ordner `se05x_03_xx_xx` - also auf der Schicht,
die AN12413 beschreibt. NXP baut die SE051-Unterstuetzung darauf auf.
Die Umschaltung erfolgt ueber `SSS_HAVE_SE05X_VER_GTE_07_02`.

## Fundstellen je Datei

| Datei | Stellen |
| --- | --- |
| `sss/src/se05x/fsl_sss_se05x_apis.c` | 72 |
| `sss/src/se05x/fsl_sss_se05x_policy.c` | 27 |
| `hostlib/hostLib/se05x_03_xx_xx/se05x_04_xx_APDU_impl.h` | 10 |
| `sss/inc/fsl_sss_ftr_default.h` | 8 |
| `hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_apis.h` | 8 |
| `hostlib/hostLib/inc/se05x_enums.h` | 8 |
| `fsl_sss_ftr.h` | 8 |
| `hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_impl.h` | 5 |
| `hostlib/hostLib/inc/se05x_const.h` | 5 |
| `hostlib/hostLib/se05x_03_xx_xx/se05x_04_xx_APDU_apis.h` | 4 |
| `sss/src/se05x/fsl_sss_se05x_mw.c` | 2 |
| `sss/inc/fsl_sss_se05x_types.h` | 2 |
| `hostlib/hostLib/se05x_03_xx_xx/se05x_APDU.c` | 2 |
| `hostlib/hostLib/se05x_03_xx_xx/se05x_pake_APDU_impl.h` | 1 |
| `hostlib/hostLib/se05x_03_xx_xx/se05x_pake_APDU_apis.h` | 1 |
| `hostlib/hostLib/se05x_03_xx_xx/se05x_T4T_APDU_impl.h` | 1 |
| `hostlib/hostLib/se05x_03_xx_xx/se05x_T4T_APDU_apis.h` | 1 |
| `hostlib/hostLib/libCommon/nxScp/nxScp03_Com.c` | 1 |
| `hostlib/hostLib/inc/Applet_SE050_Ver.h` | 1 |

## Genaue Zeilen in den APDU-Dateien

```
hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_apis.h:1354: SSS_HAVE_SE05X_VER_GTE_07_02 || (__DOXYGEN__)
hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_apis.h:1524:#endif // SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_apis.h:1525: !SSS_HAVE_SE05X_VER_GTE_07_02 || (__DOXYGEN__)
hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_apis.h:1741: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_apis.h:1812: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_apis.h:5491: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_apis.h:6291: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/se05x_03_xx_xx/se05x_APDU_apis.h:6295: (SSS_HAVE_APPLET_SE051_H && SSS_HAVE_SE05X_VER_GTE_07_02)
hostlib/hostLib/inc/se05x_enums.h:160: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/inc/se05x_enums.h:165:#endif /* SSS_HAVE_SE05X_VER_GTE_07_02 */
hostlib/hostLib/inc/se05x_enums.h:245: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/inc/se05x_enums.h:253: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/inc/se05x_enums.h:307: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/inc/se05x_enums.h:540: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/inc/se05x_enums.h:723: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/inc/se05x_enums.h:872: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/inc/se05x_const.h:34: SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/inc/se05x_const.h:146: !SSS_HAVE_SE05X_VER_GTE_07_02
hostlib/hostLib/inc/se05x_const.h:160: SSS_HAVE_SE05X_VER_GTE_07_02 // 4.4
hostlib/hostLib/inc/se05x_const.h:163: SSS_HAVE_SE05X_VER_GTE_07_02 // 5.4
hostlib/hostLib/inc/se05x_const.h:167: SSS_HAVE_SE05X_VER_GTE_07_02
```
