# Test DSP autonome

Le moteur `SwarmEngine.h` n'a **aucune dépendance JUCE**. Sous Windows (Developer Command Prompt Visual Studio) :

```cmd
cl /std:c++17 /EHsc /O2 Tests\test_dsp.cpp /Fe:swarm_test.exe
swarm_test.exe preview.wav
```

Sous Linux/macOS (sans installer JUCE) :

```sh
g++ -std=c++17 -O2 -Wall -Wextra -pedantic Tests/test_dsp.cpp -o swarm_test
./swarm_test preview.wav
```

Le programme vérifie les quatre coins du Morpher, la sortie audio numérique (aucun NaN) et écrit 4 secondes d'une séquence de test en WAV stéréo 16 bits.

Test de stress additionnel : `g++ -std=c++17 -O2 Tests/stress_dsp.cpp -o swarm_stress && ./swarm_stress`.
