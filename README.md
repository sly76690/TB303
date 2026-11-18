# OXIDE — synthétiseur acide monophonique (dossier VST303)

Basse acide monophonique dans l'esprit des petites machines à basse des années 80, avec son propre design et ses propres sons.
Projet issu de Nocturne (même base JUCE, même panneau, mêmes effets, même installeur), avec un moteur neuf.

## Règles

- Principes publics uniquement : aucune façade, aucun nom ni son d'usine d'un instrument existant.
- Pas de nom de marque ni de modèle d'un autre fabricant dans le produit.

## Le son

- VCO : dent de scie ou carré, TUNE.
- VCF : passe-bas 4 pôles résonant (boucle de résonance filtrée), CUTOFF, RESONANCE, ENV MOD, DECAY.
- ACCENT : plus fort, plus brillant ; des accents qui se suivent ouvrent le filtre de plus en plus.
- SLIDE : une note jouée en tenant la précédente glisse, sans nouvelle attaque.
- VCA DECAY, DRIVE (TONE), DELAY, REVERB.

## Construire (macOS)

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
TMPDIR="$PWD/.local-cache" cmake --build build --config Release --parallel 4
ctest --test-dir build --output-on-failure
tools/paquet.sh
```
