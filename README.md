# ANKS LABS — SWARM / 01 (prototype v0.1)

Un **synthétiseur original** à deux essaims de dents de scie et morphing XY. Son esthétique graphique se rapproche volontairement de la philosophie colorée, expressive et immédiate de Native Instruments Super*Saw, **sans reprendre son interface exacte, sa marque ou son code propriétaire**.

**Statut :** moteur DSP implémenté et validé par test audio hors JUCE. **Projet JUCE VST3 / standalone préparé mais compilation Windows non encore vérifiée sur la machine cible.**

## Fonctionnalités présentes dans le code

- Deux bancs A (rose) / B (vert), **7 oscillateurs PolyBLEP chacun**.
- Huit voix maximum, MIDI note-on/note-off, pitch bend (± 2 demi-tons), sustain CC64, all-notes-off.
- Detune (0–60 cents), écartement stéréo et mouvement libre par banc.
- Mélange A/B ; filtre passe-bas résonant SVF ; drive tanh ; enveloppe attack/release.
- MORPH XY : 4 scènes A, B, C, D ; interpolation bilinéaire ; fréquence de filtre interpolée logarithmiquement.
- Commandes exposées à l'automatisation VST3 par APVTS ; sauvegarde/restauration du patch et des quatre scènes.
- Traitement audio sans allocation mémoire dynamique, verrous ou entrées-sorties disque.
- Interface originale noire / rose / vert acide, à taille adaptable.
- Cœur DSP indépendant de JUCE pour tester et réutiliser les composants ailleurs.

## Compiler sur Windows 10/11

**Prérequis :**

- Visual Studio 2022 avec le workload *Desktop development with C++* (MSVC, SDK Windows).
- CMake 3.22 ou supérieur et Git disponibles dans le PATH.
- Accès à Internet **une fois** pour récupérer JUCE 8 (si JUCE n'est pas déjà disponible localement).

**Méthode rapide :** double-cliquer `BUILD_WINDOWS.bat`.

**Méthode manuelle (terminal PowerShell) :**

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target swarm_windows_package
```

Pour réutiliser un JUCE déjà installé et éviter toute connexion réseau :

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DANKS_JUCE_PATH="C:/SDK/JUCE"
cmake --build build --config Release --parallel
```

Résultats attendus :

```
build/ANKS_Swarm_01_artefacts/Release/VST3/ANKS SWARM 01.vst3
build/ANKS_Swarm_01_artefacts/Release/Standalone/ANKS SWARM 01.exe
```

La cible `swarm_windows_package` prépare deux ZIP dans
`build/packages/Release/`. L’archive **Portable** contient l’application
standalone. Extraire son EXE dans un dossier accessible en écriture, puis le
lancer directement. Le patch et les réglages audio/MIDI sont conservés dans
`SWARM.settings`, à côté de l’EXE. Dans **Options**, sélectionner la sortie
audio et l’entrée MIDI. Pour déplacer le standalone avec ses réglages,
emporter l’EXE et ce fichier.

Le dossier `.vst3` est un bundle. Pour installer le VST3 sur Windows, copier ce bundle dans :

```
C:\Program Files\Common Files\VST3\
```

Puis lancer un nouveau scan des plugins VST3 dans Ableton Live. Avec Live Lite, vérifier que la version installée prend en charge les VST3.

**Compilation GitHub Actions :** un workflow `.github/workflows/windows-build.yml` peut compiler automatiquement sur Windows 2022 et produire une archive VST3/Standalone lorsque le dossier est ajouté à un dépôt GitHub . Aucune publication automatique n’est prévue.

## Utilisation rapide

1. Ouvrir l'instrument sur une piste MIDI.
2. Laisser **MORPH OFF** pour piloter directement les molettes roses, vertes et les commandes MASTER.
3. Choisir un réglage, puis cliquer sur **CAP A** pour enregistrer cette configuration comme scène A. Modifier les molettes et cliquer **CAP B**, puis éventuellement **CAP C** et **CAP D**.
4. Activer **MORPH ON**, ou cliquer dans la zone XY (ce qui l'active), puis déplacer le point entre les quatre coins.
5. Réactiver le mode **LIVE** pour modifier les réglages sans remplacer automatiquement les scènes sauvegardées.
6. Enregistrer le projet Ableton : les paramètres et scènes sont sérialisés avec l'état du plugin.

**Important :** dans la v0.1, quand le mode MORPH est activé, les commandes LIVE restent visibles mais **n'affectent pas le son tant que MORPH reste actif**. C'est un choix de fonctionnement explicite pour éviter toute ambiguïté entre l'édition de scènes et leur interpolation.

## Test sans JUCE (réalisé)

Le moteur DSP peut être compilé indépendamment avec un compilateur C++17 :

```sh
g++ -std=c++17 -O2 -Wall -Wextra -pedantic Tests/test_dsp.cpp -o swarm_test
./swarm_test Tests/preview.wav
```

Le fichier `Tests/preview.wav` est inclus pour écouter le résultat du premier test d'accord et de morphing. Il ne remplace **pas** la validation en conditions réelles dans Ableton.

## Organisation

```
Source/DSP/SwarmEngine.h      moteur audio pur C++17
Source/PluginProcessor.*      VST3 / gestion MIDI / état / scènes
Source/PluginEditor.*         interface graphique JUCE
Tests/test_dsp.cpp            test DSP et rendu WAV
Tests/stress_dsp.cpp          test des cas extrêmes / fréquences
Tests/preview.wav             rendu de test de 4 secondes
BUILD_WINDOWS.bat             compilation Windows en un clic
CMakeLists.txt                configuration CMake/JUCE
 docs/ARCHITECTURE.md          détails techniques et feuille de route
```

## Dépendances et droits

- **Aucun code de KICK** ou de Native Instruments n'est utilisé dans ce prototype.
- Le moteur et l'interface créés ici sont destinés au développement privé ANKS Labs ; **ne pas supposer de licence open source** pour leur distribution.
- **JUCE est distribué sous double licence AGPLv3/commerciale** et n'est pas inclus dans cette archive. Il existe une formule Starter gratuite sous conditions d'éligibilité. Pour distribuer un **binaire fermé**, la licence JUCE applicable doit être vérifiée, même si le plugin est gratuit. Voir https://juce.com/get-juce/ et https://github.com/juce-framework/JUCE/blob/master/LICENSE.md.
- Le code de SWARM / 01 n'inclut ni sons, ni presets, ni ressources protégées de Native Instruments.

## Limitations connues et améliorations prévues

- Compilation VST3 Windows à valider (impossible à effectuer dans l'environnement Linux sans JUCE installé).
- Pas encore de preset browser, de chorus, delay, réverbération, arpégiateur ou quantification d'accords.
- Nombre d'oscillateurs fixe à 7 par banc pour contenir la charge CPU.
- Pas de comparaison sonore A/B avec Super*Saw ; l'instrument n'est pas annoncé comme une reproduction à l'identique.
- Pas encore de mesure d'aliasing, de profiler CPU ni de test d'automatisation à l'échantillon près dans un DAW.
- Les captures de scène depuis l'éditeur sont atomiques par paramètre, pas transactionnelles entre tous les champs : une capture simultanée avec le traitement audio peut momentanément mélanger ancienne/nouvelle scène sur un bloc. À renforcer avant version publique.
