# INVERSO BERIMBASS v0.1.0: baixo de berimbau, banjo e balde (VST3i)

Instrumento VST3 para Windows x64, feito em JUCE 8. É um baixo com timbre de berimbau, e também de banjo bass e de bucket bass (baixo de balde), feito por modelagem física de corda, sem samples. Usa os mesmos blocos de DSP e o mesmo visual dos instrumentos ABISMO.

## Como soa
A corda é simulada por guia de onda digital: um atraso fracionário, um amortecimento de um polo, dois all-pass de dispersão (STIFFNESS: corda de aço rígida, levemente inarmônica), a perda por ciclo e o zumbido de uma pedra tocando a corda. O atraso é corrigido pelo atraso de fase desses filtros, por isso as notas ficam afinadas: os testes medem menos de 1 cent de erro de E1 a E3, nas quatro técnicas.

**TÉCNICA**
- **Pizzicato**: o dedo. POSITION é o ponto da corda em que o dedo bate e muda o timbre.
- **Bambu**: a baqueta do berimbau, um golpe curto e duro com o estalo "tch" do bambu na corda.
- **Arco**: a corda friccionada, com BOW ATTACK lento. Sustenta enquanto a tecla está pressionada.
- **Pedra**: o dobrão encostado na corda. Dá o zumbido metálico e uma subida de afinação no ataque, de STONE BEND semitons abaixo até a nota, em 80 ms.

**CORPO**
- **Cabaça**: o "uá-uá" do berimbau. GOURD abre e fecha a cabaça, e o **mod wheel** soma a isso. Com GOURD RATE e GOURD DEPTH o movimento fica automático.
- **Banjo**: pele esticada, com ressonâncias curtas e twang.
- **Balde**: a tina de metal do washtub bass, escura e oca.
- **Nenhum**: só a corda.

**Expressão**
- **Aftertouch** pressiona o dobrão: a afinação sobe STONE BEND semitons e o zumbido aumenta.
- **MODE Mono** (o padrão) desliza entre notas tocadas legato (GLIDE), sem bater de novo na corda. **Poly** permite acordes, até 6 notas.
- Pitch bend (BEND), vibrato com entrada gradual, pedal de sustain, SUB (seno uma oitava abaixo, que segue a corda), DRIVE, TONE e um reverb curto.

## Presets
Berimbau Gunga, Gunga Cabaça Viva, Dobrão Zumbindo, Gunga no Arco, Berimbass Sub, Banjo Bass, Banjo de Bambu, Banjo Arqueado, Balde do Quintal, Tina Arqueada, Balde de Pedra e Corda Seca.

## Instalação
No GitHub, abra **Actions > Build INVERSO BERIMBASS Windows VST3**, escolha a execução verde mais recente e baixe `INVERSO_BERIMBASS_Setup_0.1.0_x64` (instalador) ou a versão portátil (copie a pasta `INVERSO BERIMBASS.vst3` para `C:\Program Files\Common Files\VST3`). Feche a DAW antes de instalar e, se o plugin não aparecer, faça um novo scan de plugins. O instalador não é assinado digitalmente.

## Limitações
- Foi testado em CI e por um host JUCE simulado, **não** dentro de Cubase, Studio One, Ableton Live ou Reaper. Teste em uma cópia do projeto antes de usar em produção.
- O arco é uma excitação de ruído contínuo (rosin), não um modelo completo de atrito arco-corda. Soa arqueado, mas não reproduz o *stick-slip* de um contrabaixo de verdade.
- A pedra é tocada pelo aftertouch e pelo STONE BEND. Não há o gesto real de afastar e encostar o dobrão no meio da nota, além do aftertouch.

## Testes
```bash
cmake -S Berimbass -B Berimbass/build-dsp -DINVERSO_DSP_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build Berimbass/build-dsp
ctest --test-dir Berimbass/build-dsp --output-on-failure
Berimbass/build-dsp/berimbass_engine /caminho/para/pasta   # demos em WAV
```
- `berimbass_engine` verifica:
  - a afinação das quatro técnicas em E1, E2 e E3, a 44,1, 48 e 96 kHz (menos de 6 cents de erro);
  - que cada combinação de técnica e corpo soa, fica abaixo de 0 dBFS e silencia após o release;
  - o glide mono legato sem nova batida e o aftertouch subindo exatamente STONE BEND;
  - a cabaça abrindo, que leva energia para os agudos, e o modo poly segurando acordes;
  - os presets numa linha de baixo e a automação aleatória.
- `InversoBerimbassHostSmoke` (JUCE):
  - toca uma linha de baixo em blocos de 1 a 1024 amostras, com mod wheel e aftertouch, em todos os presets;
  - testa All Sound Off, a restauração de sessão e a abertura do editor.
  - Com `BERIMBASS_SNAPSHOT=arquivo.png`, salva uma imagem da interface.
