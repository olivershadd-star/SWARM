# SWARM / 01 — dossier technique / passation Codex

Date : 20 novembre 2026. Version : 0.3 (reconstruction).

## Intention artistique

Synthétiseur à empilements de dents de scie et morphing, fidèle dans l'esprit graphique et l'ergonomie expressive au Super*Saw de Native Instruments, mais avec une identité ANKS originale. Design sombre, accents rose pour Swarm A et vert acide pour Swarm B, Morpher central. Ne pas transformer l'interface en machine industrielle noire/rouille, et ne pas copier pixels/ressources/propriétés intellectuelles NI.

**KICK reste indépendant.** Pas d'héritage ni refactoring du moteur Behringer : les deux projets ont des vocations différentes.

## Modules

```
AudioProcessor (JUCE 8)
  ├─ APVTS (16 paramètres automatisables)
  ├─ 4 scenes atomiques (13 variables par scène)
  ├─ MIDI parse (notes, pitch bend, CC64)
  └─ SwarmEngine (pur C++17)
       ├─ 8 Voice maximum
       │   ├─ 7 oscillateurs PolyBLEP / A
       │   ├─ 7 oscillateurs PolyBLEP / B
       │   └─ 1 enveloppe attack/release
       ├─ mix stéréo
       ├─ filtre passe-bas TPT SVF stéréo
       └─ saturation tanh et sortie stéréo

AudioProcessorEditor
  ├─ 6 knobs deux swarms
  ├─ 7 knobs MASTER
  ├─ 4 boutons CAP scenes
  └─ MorphPad XY + toggle LIVE/MORPH
```

## MIDI / polyphonie

- Jusqu'à 8 voix avec vol de voix : priorité aux voix les plus silencieuses ou en release.
- Les changements MIDI sont appliqués à l'échantillon voulu au sein de chaque bloc.
- Les notes commencent à amplitude nulle ; les notes relâchées suivent une décroissance exponentielle.
- MIDI CC64 et pitch wheel ±2 demi-tons pris en compte.
- Pas de MPE ni de voix liées aux canaux MIDI en v0.1.

## Oscillateurs

- Phase nominale par oscillateur dans [0,1[, rampe saw 2*phase−1.
- Correction PolyBLEP aux discontinuités pour limiter l'aliasing.
- Désaccordage de −detune à +detune cents, mouvement interne triangulaire asynchrone.
- Approximation polynomiale de 2^(cents/1200), uniquement pour petits décalages autour de la note.
- Panoramique individuel et mouvement de gain pour chaque oscillateur.
- Normalisation par nombre d'oscillateurs, puis atténuation globale avant saturation.

## Morpher

- Quatre configurations (A, B, C, D) stockées dans le plugin.
- Les 13 variables de synthèse sont mélangées par les poids :

```
A=(1-x)(1-y), B=x(1-y), C=(1-x)y, D=xy
```

- Cutoff : moyenne pondérée dans le domaine logarithmique.
- X/Y sont exposés comme paramètres VST3 automatisables.
- Le moteur applique un lissage de 12 ms à chaque paramètre pour éviter les clics.
- En v0.1, les scènes capturent les 13 commandes sonores mais pas l'état des 16 paramètres APVTS lui-même (MORPH X/Y et ON/OFF sont indépendants).
- En LIVE, les molettes pilotent directement le moteur. En MORPH, les scènes calculent la cible audio et l’éditeur affiche les valeurs interpolées. Un rappel A–D recharge la scène en LIVE pour l’édition. Un compteur de génération par scène permet au thread audio de rejeter une capture partielle et de réutiliser la dernière scène complète.

## Real-time safety

- SwarmEngine::renderSample ne fait aucune allocation de heap, aucune I/O et aucun verrou.
- Les LFO légers sont triangulaires et asynchrones ; le filtre recalcule ses coefficients tous les 16 échantillons.
- La majorité des accès paramètres sont effectués une seule fois par bloc.
- JUCE et le moteur ne traitent pas les snapshots avec un verrou sur le thread audio.

## Validation réalisée

```
g++ -std=c++17 -O2 -Wall -Wextra -pedantic Tests/test_dsp.cpp -o swarm_test
./swarm_test Tests/preview.wav
```

Tests exécutés : interpolation aux coins et au centre, 4 secondes de synthèse stéréo, absence de NaN/Inf, amplitude non nulle, pic inférieur à 1.0. Test de stress supplémentaire à 8/44,1/48/96 kHz sur huit notes aiguës, drive et résonance extrêmes : aucun échantillon NaN/Inf et sortie bornée. Ces tests ne constituent pas une mesure du temps CPU.

## Travail prioritaire dans Codex

1. Compiler Windows (Visual Studio 2022 + JUCE 8.0.17-security) ; corriger les éventuelles incompatibilités JUCE et obtenir le `.vst3` et le `.exe`.
2. Exécuter VST3 Validator si disponible, vérifier la création du plugin et la reprise de session DAW.
3. Tester dans Ableton : notes, sustain, voix, pitch bend, automation X/Y, plusieurs instances simultanées.
4. Mesurer le CPU en polyphonie maximale à 44.1 / 48 / 96 kHz, puis profiler les parties coûteuses (env std::exp, tanh, PolyBLEP, filtre).
5. Renforcer la synchronisation de capture de scène (double-buffer snapshot ou séquence lock-free) pour éliminer les patches partiels sur un bloc.
6. Revoir les interactions du mode LIVE/MORPH : prévoir l'édition directe de la scène courante sans désactivation du morph, si le test UX le justifie.
7. Tester aliasing et dynamique des très hautes notes, suréchantillonnage éventuel du drive, DC removal, stratégie de voix mono et unison.
8. Ajouter gestion presets JSON et banque d'usine, navigation et tests de non-régression.

## Limites volontaires

Première version sans effets temporels, sans modularité complète ni dépendance à KICK ; priorité au moteur stable, léger et autonome.
