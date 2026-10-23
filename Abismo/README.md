# INVERSO ABISMO v0.7.1: instrumentos de tensão para trilha (VST3i)

Este pacote instala três instrumentos VST3 para Windows x64:
- **INVERSO ABISMO**: sintetizador (descrito logo abaixo).
- **INVERSO ABISMO SAMPLER**: manipulação de samples, clássico e granular.
- **INVERSO ABISMO DRUMS**: sampler de percussão de ferro-velho (gravações reais em domínio público, ou as suas) e serrote melódico.

O SAMPLER e o DRUMS têm seções próprias mais adiante.

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

## ABISMO SAMPLER
Toca qualquer áudio seu (WAV, AIFF, FLAC, OGG ou MP3, de até 10 minutos, mono ou estéreo, em qualquer taxa de amostragem) pelo teclado. O áudio passa pelos mesmos filtro, VOZ, MUTILAÇÃO, reverb e TENSION do ABISMO.
- **Carregar**: use o botão *CARREGAR SAMPLE* ou arraste o arquivo para a janela. *VOZ INTERNA* volta ao sample embutido, uma frase vocal de 5 s gerada pelo próprio ABISMO, que garante que o plugin nunca abra mudo.
- **Modo clássico**: ROOT (a nota em que o sample soa no tom original), TUNE, START, REVERSE e LOOP, que pode ser normal (com crossfade XFADE) ou vai-e-volta, entre LOOP START e LOOP END. Sem loop, a nota termina quando o sample acaba.
- **Modo granular**: o som vira uma nuvem de grãos.
  - POSITION diz de que ponto do sample os grãos saem; SCAN anda por ele a uma velocidade de −200% a +200%, e 0 congela o instante.
  - SIZE (5 ms a 1 s) e DENSITY (1 a 200 grãos/s) controlam o tamanho e a quantidade de grãos.
  - SPRAY espalha a posição em até ±500 ms. PITCH RND dá até ±12 semitons aleatórios por grão, REVERSE % faz parte dos grãos tocar ao contrário e SPREAD abre os grãos no estéreo.
  - A afinação vem do teclado e não depende da velocidade: dá para esticar o tempo sem mudar o tom, ou mudar o tom sem mudar o tempo.
- **Forma de onda**: clicar ou arrastar nela define START (modo clássico) ou POSITION (granular). A linha branca mostra onde a última nota está lendo.
- **LFO → POSITION** faz a região dos grãos passear pelo sample. A TENSION aumenta o SPRAY e a afinação aleatória dos grãos.
- **Presets** (receitas aplicadas ao sample carregado): Init, Congelado, Nuvem de Vozes, Rasgo Reverso, Gagueira, Respiração Esticada, Monstro Grave, Fita Derretida, Enxame e Boca Fechada.
- **Sessões**: o projeto guarda o **caminho** do arquivo, não o áudio em si. Se você mover ou apagar o arquivo, o plugin avisa "ARQUIVO NAO ENCONTRADO", toca a voz interna e continua lembrando o caminho original. Guarde seus samples numa pasta fixa.

## ABISMO DRUMS
Percussão de ferro-velho na linha de Tom Waits, Einstürzende Neubauten e música industrial. Desde a 0.6 é um sampler: toca gravações reais, e qualquer pad aceita os seus próprios arquivos.
- **Biblioteca**: 48 objetos e 454 tomadas, gravações CC0 (domínio público) do Versilian Community Sample Library, do sfzinstruments body_percussion e do Karoryfer Big Rusty Drums (uma bateria velha e enferrujada). A lista de fontes está em `Library/SOURCES.md` e o script que monta a biblioteca em `Tools/build_drum_library.py`.
  - A biblioteca (38 MB) fica dentro da pasta do plugin, em `INVERSO ABISMO DRUMS.vst3\Contents\Resources\drums.bin`. O instalador e a versão portátil já a levam. Se ela faltar, o plugin avisa no topo e cada pad toca o objeto sintetizado mais parecido.
  - **Em WAV**: o workflow **Build ABISMO DRUMS WAV library** baixa as fontes (`Tools/fetch_drum_sources.sh`, nas versões fixadas), remonta a biblioteca, confere que ela é idêntica byte a byte à `Library/drums.bin` e publica as 454 tomadas em WAV (`ABISMO_DRUMS_WAV_0.7.0`), uma pasta por objeto, para usar em qualquer sampler.
  - Gravações como estão: Ossos (claves), Caixa de Fanfarra (caixa de corda), Palma, Caixote (cajón), Pisão, Tronco (slit drum), Matraca, Freio de Carro, Bigorna, Mola (flexatone), Raspador (guiro), Chocalho de Mola (vibraslap), Pandeiro de Lata, Tábua Batendo, Peito, Estalo e Calcanhar.
  - Montados a partir de gravações: Tambor de Óleo (bumbo de concerto com um freio de carro uma oitava abaixo, saturado), Tampa de Lixo (prato de choque abafado, mais grave e sujo), Chimbal Velho, Lata de Lixo (tom com um prato abafado chacoalhando dentro), Chapa de Metal (gongo), Prato Rachado (prato suspenso com o zumbido de uma rachadura) e Sino Enferrujado (cowbell sete semitons abaixo, saturado).
  - Da bateria enferrujada (0.7): Bumbo Enferrujado, Bumbo Oco (sem abafador), Caixa Enferrujada, Aro, Casco, Esteira Solta (a esteira chacoalhando sozinha), Pilha de Pratos (três pratos empilhados), China Velho, Prato de Rebites, Condução Rebitada, Chimbal Enferrujado, Pedal Rangendo (o barulho do próprio pedal) e Surdo Velho.
  - Montados na 0.7: Tanque (tímpano muito abaixo, abafado e saturado), Cano de Ferro (sinos tubulares uma oitava abaixo, abafados), Talheres (três a seis peças pequenas de metal batendo umas nas outras), Corrente (uma sequência curta de guizos graves, sem cauda longa), Agogô Torto, Bilha de Barro (darbuka), Chocalho de Lata, Areia (punhado jogado na pele), Vidro Gemendo (taça friccionada uma oitava abaixo), Gongo Raspado e Sino de Mão.
- **Como toca**: cada objeto tem várias tomadas, ordenadas da mais fraca para a mais forte. A velocity escolhe a tomada, VARIATION sorteia em volta dela e a mesma tomada nunca toca duas vezes seguidas. TUNE é varispeed, como fita: mais grave fica mais longo. DECAY abaixo de 100% encurta a gravação, TONE abaixo de 50% escurece e acima clareia, RUST suja e RATTLE põe peças soltas colidindo junto.
- **Seus samples**: selecione um pad e use **CARREGAR SAMPLE**, ou arraste arquivos ou uma pasta para cima de um pad. Vários arquivos (até 16) viram tomadas: o plugin ordena pela força do ataque e usa como dinâmica e alternância. **VOLTAR AO OBJETO** limpa o pad. WAV, AIFF, FLAC, OGG e MP3, até 20 s por arquivo, somados em mono.
  - O projeto guarda o caminho dos arquivos, não o áudio. Se um arquivo sumir, o pad toca o OBJECT, mostra "FALTA ARQUIVO" e o caminho continua salvo.
- **OBJECT**: os 48 primeiros são as gravações; os 16 seguintes, marcados "(sint)", são os objetos sintetizados da 0.5 (Tambor de Óleo, Colheres, Caixa de Fanfarra, Porta de Garagem, Tampa de Lixo, Caixote, Talheres, Panela, Corrente, Cano de Ferro, Mola de Cama, Lata de Lixo, Chapa de Metal, Prato Rachado, Bigorna e Garrafa).
- **Notas**: os pads ficam em C1 a D#2 (MIDI 36–51), no layout General MIDI. C1 é o bumbo (Tambor de Óleo), D1 a caixa (Caixa de Fanfarra), F#1 o chimbal (Chimbal Velho) e C#2 o prato de ataque (Prato Rachado).
- **Por pad**: OBJECT, TUNE, DECAY, TONE, RATTLE, RUST, VARIATION, LEVEL e PAN. Clicar num pad toca o som e mostra os controles dele.
- **Serrote**: do C3 (MIDI 60) para cima, monofônico, desliza no legato.
- **MÁQUINA QUEBRADA** (STUMBLE de até 80 ms, stutter de disco arranhado, CRUSH), **CARÁTER** (MEGAPHONE, GRAMOPHONE) e **SALA**.
- **Kits**: Ferro-Velho, Galpão Abandonado, Fanfarra Bêbada, Cozinha Assombrada, Oficina de Graxa, Chain Gang (só corpo: pisão, palma, peito, estalo, calcanhar), Rádio de Válvula, Bateria Enferrujada (no layout General MIDI, com sucata no lugar dos pratos), Fábrica, Porão e Ferro-Velho Sintético (os 16 objetos sintetizados).
- **Correção da 0.7.1**: com CHAOS ligado, o stutter podia trocar uma batida pelo trecho anterior (silêncio) e o pad parecia não tocar, principalmente nos kits Oficina de Graxa e Fábrica. Agora a repetição se soma à batida, e um teste verifica que todos os pads de todos os kits soam.
- **Mudança da 0.6/0.7**: a numeração do OBJECT mudou. Projetos salvos com a 0.5 abrem com gravações no lugar dos objetos sintetizados; para voltar ao som antigo, escolha o objeto "(sint)" equivalente ou o kit Ferro-Velho Sintético.

## Instalação
No GitHub, abra **Releases > INVERSO ABISMO 0.7.1** e baixe `INVERSO_ABISMO_Setup_0.7.1_x64.exe` (instalador) ou `INVERSO_ABISMO_PORTABLE_VST3_0.7.1.zip` (copie a pasta `INVERSO ABISMO.vst3` para `C:\Program Files\Common Files\VST3`). Feche a DAW antes de instalar e, se os plugins não aparecerem, faça um novo scan de plugins. Os três aparecem como instrumentos da Inverso Studios: ABISMO em *Synth*, ABISMO SAMPLER em *Sampler* e ABISMO DRUMS em *Drum*. A versão portátil traz as três pastas `.vst3`.

O instalador não é assinado digitalmente.

## Limitações
- Foi testado em CI e por um host JUCE simulado, **não** dentro de Cubase, Studio One, Ableton Live ou Reaper. Teste em uma cópia do projeto antes de usar em produção.
- O FREEZE é *quase* infinito: a modulação interna das linhas de atraso faz a cauda perder energia muito lentamente.
- A polifonia é fixa em 16 vozes. Notas além disso roubam a voz mais antiga, começando pelas que já estão em release.
- Não há MPE nem oversampling nesta versão.

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
- `abismo_sampler` verifica:
  - a afinação do sample (nota 81 sobre root 69 dá 880 Hz em 44,1, 48 e 96 kHz);
  - o fim do one-shot, os loops normal e vai-e-volta e o reverse;
  - o freeze granular estável e a transposição granular independente do SCAN;
  - a troca de sample cortando as vozes;
  - os presets sobre a voz interna e automação aleatória com troca de sample.
- `InversoAbismoSamplerHostSmoke` (JUCE):
  - grava WAVs reais em disco (estéreo de 44,1 kHz e mono de 96 kHz) e carrega cada um com a "DAW" rodando;
  - confere a afinação e troca o sample no meio de uma nota;
  - testa o modo granular, salvar/restaurar a sessão com o caminho do arquivo e o arquivo apagado (que volta para a voz interna e continua lembrando o caminho);
  - abre o editor.
- `abismo_drums` verifica (lê a biblioteca de `Library/drums.bin`):
  - que a biblioteca é lida, tem os 48 objetos e que um arquivo truncado é recusado;
  - que, sem a biblioteca, cada objeto gravado ainda toca pelo substituto sintetizado;
  - que os 64 objetos são audíveis, terminam sozinhos e chegam ao silêncio;
  - que a velocity escolhe tomadas mais fortes, que a mesma tomada não se repete e que TUNE -12 dobra a duração;
  - que um sample do usuário toca afinado e que tirá-lo silencia o pad;
  - que um groove em shuffle lento (84 BPM) com todos os kits fica abaixo de 0 dBFS;
  - que batidas consecutivas variam e que STUMBLE atrasa em até 80 ms;
  - que o serrote afina (A4 = 440 Hz), desliza e para no release, e que as notas dele não disparam pads;
  - que os kits num groove 6/8 com serrote ficam abaixo de 0 dBFS;
  - a automação aleatória em 44,1 e 96 kHz.
- `InversoAbismoDrumsHostSmoke` (JUCE): groove em blocos de 1 a 1024 amostras com todos os kits, audição e luz de pad vindas do editor, serrote, restauração de sessão com pad reatribuído, a biblioteca, um pad com três tomadas próprias (toca, volta na sessão, avisa e guarda o caminho quando os arquivos somem, limpa) e abertura do editor.
- `InversoAbismoHostSmoke` (JUCE) toca MIDI em blocos de 1 a 2048 amostras, troca programas, automatiza o cutoff dentro da nota, testa pedal, All Sound Off, salvar/restaurar sessão e a abertura do editor. Com `ABISMO_SNAPSHOT=arquivo.png`, também salva uma imagem da interface.
