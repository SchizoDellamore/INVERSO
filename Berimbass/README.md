# INVERSO BERIMBASS v0.2.0: baixo de berimbau, banjo e balde (VST3i)

Instrumento VST3 para Windows x64, feito em JUCE 8. É um baixo com timbre de berimbau, e também de banjo bass e de bucket bass (baixo de balde), feito por modelagem física de corda, sem samples. Usa os mesmos blocos de DSP e o mesmo visual dos instrumentos ABISMO.

## Como soa
A corda é modelada fisicamente, nota a nota, sem samples:
- **Duas polarizações**: duas vibrações da mesma corda, uma 1,5 cent acima e um pouco mais escura. Elas batem entre si e decaem como corda real, não como um tom digital parado.
- **Modulação de tensão**: enquanto a corda vibra largo, ela fica um pouco aguda e assenta conforme decai. É pouco no arame de aço do berimbau, mais na corda do banjo e muito na corda grossa do balde, o "bwong" típico do washtub.
- **Rigidez (STIFFNESS)**: quatro all-pass esticam os parciais do arame, que dão o tinido metálico.
- **Afinação**: o atraso é corrigido pelo atraso de fase dos filtros, então as notas ficam afinadas. Os testes medem até 3 cents de erro de E1 a E3, nas quatro técnicas.

**TÉCNICA**
- **Pizzicato**: o dedo puxa a corda num formato de triângulo e solta; a polpa do dedo arredonda a ponta. POSITION é onde o dedo pega.
- **Bambu**: a baqueta golpeia um trecho estreito da corda, com o estalo de madeira da própria baqueta e o **caxixi** (sementes batendo no cesto) no corpo Cabaça.
- **Arco**: modelo de corda arqueada do STK. O arco divide a corda em dois lados e uma curva de atrito decide, a cada amostra, se a corda gruda no crina ou escorrega. É o "agarra-solta" de um arco de verdade.
- **Pedra**: baqueta mais o dobrão quicando no arame (zumbido) e uma subida de afinação de STONE BEND semitons em 80 ms.

**CORPO**
- **Cabaça**: ressonância de Helmholtz (o ar dentro da cabaça), modos da casca e a reflexão na barriga do tocador. GOURD, mod wheel ou GOURD RATE/DEPTH abrem a boca da cabaça: a ressonância vai de "u" para "a" e a reflexão muda, que é o mecanismo real do "uá-uá". CAXIXI regula o chocalho.
- **Banjo**: pele com modos de Bessel, curta, mais o pico anasalado.
- **Balde**: o fundo de chapa da tina, a parede cilíndrica, a ressonância de ar grave e uma corda grossa e escura.
- **Nenhum**: só a corda.

**Expressão**: aftertouch pressiona o dobrão (sobe STONE BEND e aumenta o zumbido). MODE Mono, o padrão, desliza no legato sem bater de novo na corda; Poly toca até 6 notas. Há também pitch bend, vibrato com entrada gradual, SUB, DRIVE, TONE e reverb curto.

## Presets
Berimbau Gunga, Gunga Cabaça Viva, Dobrão Zumbindo, Gunga no Arco, Berimbass Sub, Banjo Bass, Banjo de Bambu, Banjo Arqueado, Balde do Quintal, Tina Arqueada, Balde de Pedra e Corda Seca.

## Instalação
No GitHub, abra **Actions > Build INVERSO BERIMBASS Windows VST3**, escolha a execução verde mais recente e baixe `INVERSO_BERIMBASS_Setup_0.2.0_x64` (instalador) ou a versão portátil (copie a pasta `INVERSO BERIMBASS.vst3` para `C:\Program Files\Common Files\VST3`). Feche a DAW antes de instalar e, se o plugin não aparecer, faça um novo scan de plugins. O instalador não é assinado digitalmente.

## Limitações
- Foi testado em CI e por um host JUCE simulado, **não** dentro de Cubase, Studio One, Ableton Live ou Reaper. Teste em uma cópia do projeto antes de usar em produção.
- A pedra é tocada pelo aftertouch e pelo STONE BEND. Não há o gesto real de afastar e encostar o dobrão no meio da nota, além do aftertouch.

## Testes
```bash
cmake -S Berimbass -B Berimbass/build-dsp -DINVERSO_DSP_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build Berimbass/build-dsp
ctest --test-dir Berimbass/build-dsp --output-on-failure
Berimbass/build-dsp/berimbass_engine /caminho/para/pasta   # demos em WAV
```
- `berimbass_engine` verifica:
  - a afinação das quatro técnicas em E1, E2 e E3, a 44,1, 48 e 96 kHz (menos de 6 cents de erro; medido: até 2,7 cents);
  - que cada combinação de técnica e corpo soa, fica abaixo de 0 dBFS e silencia após o release;
  - o glide mono legato sem nova batida e o aftertouch subindo exatamente STONE BEND;
  - a cabaça abrindo (de "u" para "a"), o modo poly segurando acordes;
  - os presets numa linha de baixo e a automação aleatória.
- `InversoBerimbassHostSmoke` (JUCE):
  - toca uma linha de baixo em blocos de 1 a 1024 amostras, com mod wheel e aftertouch, em todos os presets;
  - testa All Sound Off, a restauração de sessão e a abertura do editor.
  - Com `BERIMBASS_SNAPSHOT=arquivo.png`, salva uma imagem da interface.
