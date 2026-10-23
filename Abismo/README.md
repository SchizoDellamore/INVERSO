# INVERSO ABISMO v0.2.0: sintetizador de tensão para trilha (VST3i)

Instrumento VST3 para Windows x64, feito em JUCE 8. É um sintetizador polifônico (16 vozes) voltado para trilha de terror e suspense: clusters dissonantes, metal friccionado, vidro, sinos, drones, glissandos e afinação instável. É um instrumento independente; os outros plugins INVERSO não são alterados.

## Motor sonoro
- **OSCILADOR**: forma de onda contínua seno → triângulo → dente de serra → quadrada (PolyBLEP, com pouco aliasing), unison de 1 a 7 vozes com desafinação e abertura estéreo, oitava de −2 a +2 e sub-oscilador senoidal.
- **DISSONÂNCIA**: cada nota pode ganhar intervalos fixos de 2ª menor, trítono, 2ª menor + trítono, quartos de tom, 9ª menor + trítono ou 7ª maior + 2ª menor. Também tem ruído de respiração (BREATH) e a macro **TENSION**.
- **RESSONADOR**: banco modal de 6 modos afinado pela nota, com materiais *String, Wood, Metal, Glass, Bell, Abyss*. É excitado por ruído contínuo (BOW, o arco), impulsos aleatórios (SCRAPE, a raspagem) e um ataque curto (STRIKE). Funciona sem os osciladores: arco em prato, taça de vidro, corrente arrastada.
- **FILTRO**: state-variable (TPT) passa-baixa, passa-banda ou passa-alta, com envelope próprio, keytracking e drive antes do filtro.
- **AFINAÇÃO / MOVIMENTO**: GLISSANDO de −24 a +24 semitons ao longo de GLISS TIME (queda ou subida lenta), DRIFT (deriva aleatória da afinação, até 60 cents), GLIDE (portamento a partir da nota anterior) e faixa de pitch bend de 1 a 24 semitons.
- **VOZ**: filtro de 3 formantes que faz morph entre as vogais A-E-I-O-U (VOWEL). FORMANT desloca os formantes ±12 semitons, de demoníaco a infantil, e VOICE mistura com o som original. O LFO→FILTER também move a vogal, como uma boca abrindo e fechando. Com dente de serra + BREATH + DRIVE, dá grunhidos, gemidos e respiração processada no espírito de Cristobal Tapia De Veer (Smile 1 e 2, Utopia).
- **MUTILAÇÃO**: CRUSH (de 16 a 3 bits), DOWNSAMPLE (redução de taxa de amostragem) e STUTTER sincronizado ao tempo da DAW (1/4 a 1/32 e tercinas). DEPTH encurta o gate. CHAOS sorteia, a cada passo, entre silenciar ou repetir o pedaço anterior. Com a DAW parada, o stutter segue o último tempo conhecido (120 BPM por padrão).
- **LFO** para pitch, filtro e amplitude (o tremolo serve para pulso e batimento).
- **ESPAÇO**: reverb FDN de 8 linhas, com decay de 0,3 a 30 s, DARKNESS (amortecimento), FREEZE, largura estéreo e ganho de saída. Na saída há um teto suave: o sinal é linear até −0,9 dBFS e nunca passa de 0 dBFS.

## Controle em tempo real
- **Mod wheel (CC1)** soma à TENSION. Mais tensão significa mais cluster, mais deriva, mais arco no ressonador, mais vibrato, mais ressonância do filtro e mais CHAOS no stutter. Ideal para automação de cena.
- **Aftertouch** (canal ou polifônico) aumenta a pressão do arco e abre o filtro.
- **Pedal de sustain (CC64)**, pitch bend, All Notes Off e All Sound Off.

## Presets de fábrica
Init, Cordas Mortas, Arco no Metal, Vidro Rachado, Queda Livre, Drone do Abismo, Sussurro no Porão, Caixa de Música Quebrada, Batimento, Tensão Infinita, Coro Afogado e Corrente Arrastada. Na linha de voz processada e picotada: Sorriso Torto, Grunhido, Respiração Picotada e Lamento Mastigado. Ficam no menu do plugin e também aparecem como *programs* do VST3 na DAW. Os níveis foram equilibrados em cerca de −27 dB RMS.

## Instalação
No GitHub, abra **Actions > Build INVERSO ABISMO Windows VST3**, escolha a execução verde mais recente e baixe `INVERSO_ABISMO_Setup_0.2.0_x64` (instalador) ou `INVERSO_ABISMO_PORTABLE_VST3_0.2.0_NO_EXE` (copie a pasta `INVERSO ABISMO.vst3` para `C:\Program Files\Common Files\VST3`). Feche a DAW antes de instalar e, se o plugin não aparecer, faça um novo scan de plugins. Ele aparece como instrumento (*Instrument / Synth*) da Inverso Studios.

O instalador não é assinado digitalmente.

## Limitações
- Foi testado em CI e por um host JUCE simulado, **não** dentro de Cubase, Studio One, Ableton Live ou Reaper. Teste em uma cópia do projeto antes de usar em produção.
- O FREEZE é *quase* infinito: a modulação interna das linhas de atraso faz a cauda perder energia muito lentamente.
- A polifonia é fixa em 16 vozes. Notas além disso roubam a voz mais antiga, começando pelas que já estão em release.
- Não há MPE, oversampling nem importação de samples nesta versão.

## Testes
Testes de DSP portáteis (Linux/macOS/Windows, sem JUCE):

```bash
cmake -S Abismo -B Abismo/build-dsp -DINVERSO_DSP_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build Abismo/build-dsp
ctest --test-dir Abismo/build-dsp --output-on-failure
# demos em WAV de todos os presets:
Abismo/build-dsp/abismo_presets /caminho/para/pasta
```

- `abismo_engine` verifica a afinação (A4 = 440 Hz em 44,1/48/96 kHz), pitch bend, troca de oitava, release até o silêncio, pedal de sustain, limite e roubo de vozes, os intervalos de cluster, a chegada exata do glissando, o glide linear, o ressonador sem osciladores, o freeze/unfreeze do reverb, o morph de vogais, o gate do stutter na grade de 1/16 a 120 BPM, a volta do áudio ao desligar o stutter e a quantização do crush.
- `abismo_presets` verifica que todo preset é audível, finito e fica abaixo de 0 dBFS.
- `abismo_stress` faz automação aleatória de todos os parâmetros com MIDI, pitch bend, mod wheel, aftertouch e pedal em três taxas de amostragem. Também estima a CPU do pior caso (16 vozes × 7 unison × 3 camadas de cluster + ressonador + reverb).
- `InversoAbismoHostSmoke` (JUCE) toca MIDI em blocos de 1 a 2048 amostras, troca programas, automatiza o cutoff dentro da nota, testa pedal, All Sound Off, salvar/restaurar sessão e a abertura do editor. Com `ABISMO_SNAPSHOT=arquivo.png`, também salva uma imagem da interface.
