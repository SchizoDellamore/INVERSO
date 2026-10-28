# INVERSO ABISMO v0.21.0: instrumentos de tensão para trilha (VST3i)

Este pacote instala quatro instrumentos VST3 para Windows x64:
- **INVERSO ABISMO**: sintetizador (descrito logo abaixo).
- **INVERSO ABISMO SAMPLER**: manipulação de samples, clássico e granular.
- **INVERSO ABISMO DRUMS**: sampler de percussão de ferro-velho (gravações reais em domínio público, ou as suas) e serrote melódico.
- **INVERSO RELICÁRIO** (0.10): quatro máquinas esquecidas num instrumento só: loops de fita, harmônio de fole elétrico, diapasão eletromagnético e órgão transistorizado soviético.
- **INVERSO CLEAR** (0.13): tira o embolamento e a aspereza de um som só quando eles aparecem (um efeito).
- **INVERSO TIDE** (0.15): EQ dinâmico para instrumentos (um efeito).
- **INVERSO CABS** (0.15 no instalador): 98 IRs de caixas, em Documentos\INVERSO\INVERSO CABS, pastas Guitar e Bass.
- **INVERSO ENKI** (0.11): gerador de ideias sonoras. Inventa sons musicais ou não, a partir de síntese, das gravações do DRUMS e das suas próprias bibliotecas de samples.

O SAMPLER, o DRUMS, o RELICÁRIO e o ENKI têm seções próprias mais adiante.

**MIDI learn (0.14):** em qualquer knob de qualquer instrumento, clique com o botão direito e escolha *MIDI learn*; o knob pisca até você girar um knob do teclado (Alesis V49, por exemplo), e daí em diante ele segue esse controle. Um ponto colorido no knob mostra que ele está ligado a um CC; *Forget MIDI* desfaz. As ligações ficam salvas no projeto. Substitui o antigo LEARN de aftertouch.

**Presets seus (0.14):** SAVE, ao lado do nome do preset, guarda o som atual em *Documentos/INVERSO/<plugin>/Presets*; o menu do preset (clique no nome) lista os seus embaixo dos de fábrica.

**Idioma (0.10):** a interface dos quatro instrumentos está em inglês: controles, módulos, presets, kits e objetos. Este manual continua em português; onde ele cita um controle pelo nome antigo em português, procure o equivalente em inglês na mesma seção da tela (por exemplo RESSONADOR → RESONATOR, MUTILAÇÃO → MUTILATION, SERROTE → MUSICAL SAW, Ferro-Velho → Scrapyard). Projetos salvos abrem iguais: só os nomes mudaram.

Instrumento VST3 para Windows x64, feito em JUCE 8. É um sintetizador polifônico (16 vozes) voltado para trilha de terror e suspense: clusters dissonantes, metal friccionado, vidro, sinos, drones, glissandos e afinação instável. É um instrumento independente; os outros plugins INVERSO não são alterados.

## Motor sonoro
- **OSCILADOR**: forma de onda contínua seno → triângulo → dente de serra → quadrada (PolyBLEP, com pouco aliasing), unison de 1 a 7 vozes com desafinação e abertura estéreo, oitava de −2 a +2 e sub-oscilador senoidal.
- **DISSONÂNCIA**: cada nota pode ganhar intervalos fixos de 2ª menor, trítono, 2ª menor + trítono, quartos de tom, 9ª menor + trítono ou 7ª maior + 2ª menor. Também tem ruído de respiração (BREATH) e a macro **TENSION**.
- **RESSONADOR** (ampliado na 0.8): banco modal de 12 modos por voz com 24 materiais. São eles: Corda, Madeira, Metal, Vidro, Sino, Abismo, Marimba, Cano (só harmônicos ímpares), Membrana, Gongo, Tigela tibetana (pares de parciais que batem), Mola, Corda de piano, Gamelão, Prato, Osso, Acorde menor, Cluster, Trítono, Ferrugem (cada nota desafina os parciais do seu jeito), Corpo de madeira, Chapa de trovão, Subterrâneo e Sino rachado.
  - É excitado por ruído contínuo (BOW, o arco), impulsos aleatórios (SCRAPE, a raspagem), um ataque curto (STRIKE) e, com OSC INPUT, pelos próprios osciladores, como uma corda soando num corpo. Funciona sem os osciladores: arco em prato, taça de vidro, corrente arrastada.
  - MATERIAL B e MORPH passam aos poucos de um material para outro. TUNE afina o ressonador à parte (±24 semitons). SEGUE NOTA em 100% segue a nota; em 0% ele fica parado perto do dó central, como um corpo fixo que qualquer nota faz soar.
  - STRETCH estica ou comprime os parciais (mais metálico ou mais oco). BRIGHT decide se os agudos duram tanto quanto os graves. STEREO abre parciais alternados para a esquerda e a direita.
  - RATTLE é uma peça solta encostando no corpo: passado um certo balanço ela zumbe, e o zumbido faz o corpo soar de novo.
- **FILTRO**: state-variable (TPT) passa-baixa, passa-banda ou passa-alta, com envelope próprio e keytracking.
- **DESTRUIÇÃO** (0.8), antes do filtro:
  - DRIVE com cinco tipos: Fita (saturação suave), Fuzz (corte duro e assimétrico), Dobra (a onda se dobra sobre si mesma, mais dobras com mais drive), Retificador (onda completa, uma oitava acima e zumbindo) e Estilhaço (curva esmagada com quedas aleatórias que estalam).
  - RING é um modulador em anel com portadora em RING PITCH semitons acima ou abaixo da nota (de −24 a +48): metal, sino quebrado, rádio.
  - WARP é fita derretendo: wow lento, flutter rápido e, de vez em quando, a fita arrasta e cai de afinação.
- **AFINAÇÃO / MOVIMENTO**: GLISSANDO de −24 a +24 semitons ao longo de GLISS TIME (queda ou subida lenta), DRIFT (deriva aleatória da afinação, até 60 cents), GLIDE (portamento a partir da nota anterior) e faixa de pitch bend de 1 a 24 semitons.
- **VOZ**: filtro de 3 formantes que faz morph entre as vogais A-E-I-O-U (VOWEL). FORMANT desloca os formantes ±12 semitons, de demoníaco a infantil, e VOICE mistura com o som original. O LFO→FILTER também move a vogal, como uma boca abrindo e fechando. Com dente de serra + BREATH + DRIVE, dá grunhidos, gemidos e respiração processada no espírito de Cristobal Tapia De Veer (Smile 1 e 2, Utopia).
- **MUTILAÇÃO**: CRUSH (de 16 a 3 bits), DOWNSAMPLE (redução de taxa de amostragem) e STUTTER sincronizado ao tempo da DAW (1/4 a 1/32 e tercinas). DEPTH encurta o gate. CHAOS sorteia, a cada passo, entre silenciar ou repetir o pedaço anterior. REVERSE (0.8) é a chance de cada passo tocar o pedaço anterior de trás para frente; com o STUTTER desligado ele usa uma grade própria de 1/8, sem gate. Com a DAW parada, o stutter segue o último tempo conhecido (120 BPM por padrão).
  - TEMPO (0.8.1, também no SAMPLER e no DRUMS): **BPM** prende o stutter e o reverse à grade da DAW; **LIVRE** solta da DAW e cada passo dura TIME milissegundos (de 10 ms a 2 s), em qualquer andamento. STUTTER em *Off* continua desligando; em LIVRE, qualquer outra opção liga.
- **LFO** para pitch, filtro e amplitude (o tremolo serve para pulso e batimento).
- **ESPAÇO**: reverb FDN de 8 linhas, com decay de 0,3 a 30 s, DARKNESS (amortecimento), REV REVERSE (0.8: a cauda toca de trás para frente em janelas de 350 ms e cresce antes do som, à la reverse reverb de trilha de terror), FREEZE, largura estéreo e ganho de saída. Na saída há um teto suave: o sinal é linear até −0,9 dBFS e nunca passa de 0 dBFS.

## Interface (0.9)
Os três plugins usam o mesmo desenho:
- **Corpo claro e módulos em cartões**, cada módulo com uma cor (fonte âmbar, ressonador ciano, filtro violeta, destruição vermelho, mutilação laranja, envelope verde, movimento azul, voz rosa).
- **Uma tela escura em cada módulo**, que desenha o que o som está fazendo:
  - a forma de onda e as cópias do uníssono;
  - os 12 parciais do ressonador (com os do material B, durante o morph);
  - a curva do filtro (e para onde o envelope a leva);
  - o envelope;
  - a curva de cada tipo de drive;
  - a grade do stutter, com os passos ao contrário marcados;
  - as vogais;
  - a cauda do reverb (congelada ou ao contrário).
  - No SAMPLER, a forma de onda clicável e a nuvem de grãos; no DRUMS, a gravação do pad.
- **Os seletores principais ficam na tela**, com setas ‹ ›; clicar no nome abre a lista.
- **Knobs brancos com o arco na cor do módulo**. Os bipolares (afinação, envelope do filtro) partem do centro. Duplo clique volta ao valor padrão, a roda do mouse ajusta e SHIFT + roda faz o ajuste fino.
- **Páginas**: cada módulo mostra no máximo 6 controles; o resto fica nas abas do título (ex.: RESSONADOR › TOM, EXCITAÇÃO, CORPO).
- **MASTER** com o volume de saída, um segundo controle, o medidor de pico e as vozes soando.
- **Presets/kits** numa barra central: ‹ › trocam, clicar no nome abre a lista.
- A janela abre em 90% do tamanho de projeto e pode ser redimensionada pelo canto (de 60% a 150%), mantendo a proporção.
- Fontes embutidas: Poppins e IBM Plex Mono, ambas sob a SIL Open Font License (`Fonts/`).

## Controles apagados (0.8.1)
Uma auditoria mediu o efeito de cada controle dos três plugins: o som foi gerado com o controle no mínimo e no máximo, em vários contextos, e as mudanças foram comparadas. Nenhum controle repete o que outro faz; os pares que parecem semelhantes só mexem no volume ou no brilho geral por caminhos diferentes. O que existia era controle sem efeito por depender de outro. Agora ele fica apagado na tela enquanto não age (continua ajustável) e acende quando passa a valer. Exemplos: o corpo do ressonador com LEVEL em 0, MATERIAL B com MORPH em 0, DRIVE TYPE com DRIVE em 0, o envelope do filtro com ENV AMT em 0, VOWEL e FORMANT com VOICE em 0, os grãos no modo clássico do SAMPLER e os loops no granular, a sala do DRUMS com MIX em 0. O KEYTRACK do ressonador passou a se chamar SEGUE NOTA, para não confundir com o do filtro.

## Controle em tempo real
- **Mod wheel (CC1)** soma à TENSION. Mais tensão significa mais cluster, mais deriva, mais arco no ressonador, mais vibrato, mais ressonância do filtro e mais CHAOS no stutter. Ideal para automação de cena.
- **Aftertouch** (canal ou polifônico) aumenta a pressão do arco e abre o filtro.
- **Pedal de sustain (CC64)**, pitch bend, All Notes Off e All Sound Off.

## Presets de fábrica
Init, Cordas Mortas, Arco no Metal, Vidro Rachado, Queda Livre, Drone do Abismo, Sussurro no Porão, Caixa de Música Quebrada, Batimento, Tensão Infinita, Coro Afogado e Corrente Arrastada. Na linha de voz processada e picotada: Sorriso Torto, Grunhido, Respiração Picotada e Lamento Mastigado.
- Novos na 0.8, com o ressonador ampliado: Tigela do Exorcismo, Gongo ao Contrário, Piano Enterrado, Corda Enferrujada, Mola do Porão, Coral de Ossos, Trovão de Chapa, Sino Rachado do Campanário, Cluster Respirando, Gamelão Fantasma, Marimba do Sótão, Corpo Oco, Subterrâneo, Tritono Pendular e Membrana Viva.
- Novos na 0.8, destrutivos e ao contrário: Rádio Morto, Fuzz Infernal, Dobra Cerebral, Retificador de Almas, Lembrança Derretida, Reverso Sussurrado, Picotado ao Contrário, Anel de Fogo e Colapso Total. Ficam no menu do plugin e também aparecem como *programs* do VST3 na DAW. Os níveis foram equilibrados em cerca de −27 dB RMS.

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
- **Biblioteca** (0.21): 195 objetos e 1225 tomadas, 13 minutos, gravações CC0 (domínio público) do Versilian Community Sample Library, do sfzinstruments body_percussion e do Karoryfer Big Rusty Drums (uma bateria velha e enferrujada). A lista de fontes está em `Library/SOURCES.md` e o script que monta a biblioteca em `Tools/build_drum_library.py`.
  - **0.21, biblioteca nova**: além dos 48 anteriores, 147 objetos de fontes e articulações diferentes, por categoria no menu OBJECT: Kick (15), Snare (24), Tom (16), Hat (11), Cymbal (8), Perc (24), Wood (5), Metal (6), Body (6), Tonal (14, instrumentos de notas tocados como golpes uma oitava abaixo) e Ruin (18, gravações reais rebaixadas, cortadas e esmagadas). Os números de OBJECT 0 a 63 não mudaram (sessões salvas e ENKI seguem iguais); os novos são 64 e acima. A faixa abaixo de 28 Hz de todas as gravações foi removida (era ruído de microfone e deslocamento DC, até 80% da energia de alguns bumbos). 27 kits novos, cada um com sons próprios, num total de 38. `Tools/build_drum_library.py` gera `Library/drums.bin`, `Source/DSP/DrumsObjects.h` e `Source/DSP/DrumsKitsGenerated.h` juntos, e `Tools/measure_drum_levels.cpp` com `Tools/make_drum_trim.py` regeneram `DrumsTrim.h`.
  - A biblioteca (69 MB) fica dentro da pasta do plugin, em `INVERSO ABISMO DRUMS.vst3\Contents\Resources\drums.bin`. O instalador e a versão portátil já a levam. Se ela faltar, o plugin avisa no topo e cada pad toca o objeto sintetizado mais parecido.
  - **Em WAV**: o workflow **Build ABISMO DRUMS WAV library** baixa as fontes (`Tools/fetch_drum_sources.sh`, nas versões fixadas), remonta a biblioteca, confere que ela é idêntica byte a byte à `Library/drums.bin` e publica as 1225 tomadas em WAV (`ABISMO_DRUMS_WAV_0.7.0`), uma pasta por objeto, para usar em qualquer sampler.
  - Gravações como estão: Ossos (claves), Caixa de Fanfarra (caixa de corda), Palma, Caixote (cajón), Pisão, Tronco (slit drum), Matraca, Freio de Carro, Bigorna, Mola (flexatone), Raspador (guiro), Chocalho de Mola (vibraslap), Pandeiro de Lata, Tábua Batendo, Peito, Estalo e Calcanhar.
  - Montados a partir de gravações: Tambor de Óleo (bumbo de concerto com um freio de carro uma oitava abaixo, saturado), Tampa de Lixo (prato de choque abafado, mais grave e sujo), Chimbal Velho, Lata de Lixo (tom com um prato abafado chacoalhando dentro), Chapa de Metal (gongo), Prato Rachado (prato suspenso com o zumbido de uma rachadura) e Sino Enferrujado (cowbell sete semitons abaixo, saturado).
  - Da bateria enferrujada (0.7): Bumbo Enferrujado, Bumbo Oco (sem abafador), Caixa Enferrujada, Aro, Casco, Esteira Solta (a esteira chacoalhando sozinha), Pilha de Pratos (três pratos empilhados), China Velho, Prato de Rebites, Condução Rebitada, Chimbal Enferrujado, Pedal Rangendo (o barulho do próprio pedal) e Surdo Velho.
  - Montados na 0.7: Tanque (tímpano muito abaixo, abafado e saturado), Cano de Ferro (sinos tubulares uma oitava abaixo, abafados), Talheres (três a seis peças pequenas de metal batendo umas nas outras), Corrente (uma sequência curta de guizos graves, sem cauda longa), Agogô Torto, Bilha de Barro (darbuka), Chocalho de Lata, Areia (punhado jogado na pele), Vidro Gemendo (taça friccionada uma oitava abaixo), Gongo Raspado e Sino de Mão.
- **Como toca**: cada objeto tem várias tomadas, ordenadas da mais fraca para a mais forte. A velocity escolhe a tomada, VARIATION sorteia em volta dela e a mesma tomada nunca toca duas vezes seguidas. TUNE é varispeed, como fita: mais grave fica mais longo. DECAY abaixo de 100% encurta a gravação, TONE abaixo de 50% escurece e acima clareia, RUST suja e RATTLE põe peças soltas colidindo junto.
- **Seus samples**: selecione um pad e use **CARREGAR SAMPLE**, ou arraste arquivos ou uma pasta para cima de um pad. Vários arquivos (até 16) viram tomadas: o plugin ordena pela força do ataque e usa como dinâmica e alternância. **VOLTAR AO OBJETO** limpa o pad. WAV, AIFF, FLAC, OGG e MP3, até 20 s por arquivo, somados em mono.
  - O projeto guarda o caminho dos arquivos, não o áudio. Se um arquivo sumir, o pad toca o OBJECT, mostra "FALTA ARQUIVO" e o caminho continua salvo.
- **OBJECT**: os 48 primeiros são as gravações das versões 0.7 a 0.20; os 16 seguintes, marcados "(sint)", são os objetos sintetizados da 0.5 (Tambor de Óleo, Colheres, Caixa de Fanfarra, Porta de Garagem, Tampa de Lixo, Caixote, Talheres, Panela, Corrente, Cano de Ferro, Mola de Cama, Lata de Lixo, Chapa de Metal, Prato Rachado, Bigorna e Garrafa).
- **Notas**: os pads ficam em C1 a D#2 (MIDI 36–51), no layout General MIDI. C1 é o bumbo (Tambor de Óleo), D1 a caixa (Caixa de Fanfarra), F#1 o chimbal (Chimbal Velho) e C#2 o prato de ataque (Prato Rachado).
- **Por pad**: OBJECT, TUNE, DECAY, TONE, RATTLE, RUST, VARIATION, LEVEL e PAN. Clicar num pad toca o som e mostra os controles dele.
- **Serrote**: do C3 (MIDI 60) para cima, monofônico, desliza no legato.
- **MÁQUINA QUEBRADA** (STUMBLE de até 80 ms, stutter de disco arranhado, CRUSH), **CARÁTER** (MEGAPHONE, GRAMOPHONE) e **SALA**.
- **Kits**: Ferro-Velho, Galpão Abandonado, Fanfarra Bêbada, Cozinha Assombrada, Oficina de Graxa, Chain Gang (só corpo: pisão, palma, peito, estalo, calcanhar), Rádio de Válvula, Bateria Enferrujada (no layout General MIDI, com sucata no lugar dos pratos), Fábrica, Porão e Ferro-Velho Sintético (os 16 objetos sintetizados).
- **Correção da 0.7.2** (na 0.8 também no ABISMO e no SAMPLER, para presets): no Cubase, trocar de kit pelo menu do plugin podia não funcionar, voltar sozinho para o kit anterior ou fazer ruído. O plugin não avisava a DAW da troca, e a DAW reimpunha o kit antigo. Agora a DAW é avisada, um kit reenviado pela DAW não apaga suas edições, e a troca abafa o kit anterior em poucos milissegundos e começa limpa (sem cauda de sala nem resto de efeito); uma nota tocada no instante da troca soa no kit novo.
- **Correção da 0.7.1**: com CHAOS ligado, o stutter podia trocar uma batida pelo trecho anterior (silêncio) e o pad parecia não tocar, principalmente nos kits Oficina de Graxa e Fábrica. Agora a repetição se soma à batida, e um teste verifica que todos os pads de todos os kits soam.
- **Mudança da 0.8 no ABISMO**: o ressonador passou de 6 para 12 modos. Os materiais antigos mantêm os mesmos 6 primeiros parciais e o mesmo volume, mas ganham brilho nos agudos.
- **Mudança da 0.6/0.7**: a numeração do OBJECT mudou. Projetos salvos com a 0.5 abrem com gravações no lugar dos objetos sintetizados; para voltar ao som antigo, escolha o objeto "(sint)" equivalente ou o kit Ferro-Velho Sintético.

## RELICÁRIO

**Novo na 0.14 (a partir do retorno de uso):**
- Cada máquina ganhou uma página ADSR com quatro faders (ataque, decaimento, sustentação, soltura) só para ela. O módulo ENVELOPE, também em faders, molda o instrumento inteiro. O ADSR de cada máquina vem aberto, então só muda algo quando você o ajusta. KEYS ficou com velocity, glide, bend, afinação fina e AGE.
- Botão ON/OFF em cada módulo (máquinas e efeitos), independente do volume; desliga e liga sem estalo.
- TAPE LOOPS: volume próprio para cada loop (LOOP 1, LOOP 2), no lugar do balanço.
- Alto-falante quebrado do órgão soviético: cone pequeno, bobina que bate no fundo e aba rasgada que vibra nos movimentos grandes; a distorção acompanha a música, sem chiado.
- Diapasão: a faísca é um estalo e um zumbido no tom do diapasão; o clang são os modos de uma barra de aço (6,27x, 17,55x, 34,4x a fundamental), com batimento.
- DESTROY: Tape aquece (mantém o volume dos sons baixos e arredonda o agudo, em vez de realçar o chiado); Rectify mantém a nota; Shatter nunca deixa a nota sumir; o bit crusher conta os bits em relação ao volume do som (o grão acompanha a música e as partes quietas não viram chiado).
- Chop, troca de tipo de drive e de filtro: sem estalos.
Instrumento polifônico (12 vozes) no espírito dos instrumentos "esquecidos" da Rhythmic Robot (Loopscape, Magnus, Spark Gap, órgãos soviéticos). Não usa gravações nem copia nada deles: as quatro máquinas são sintetizadas, imitando o mecanismo de cada uma. Cada máquina tem seu próprio LEVEL; em 0% ela fica desligada e os controles dela aparecem apagados. Dá para tocar uma de cada vez ou as quatro juntas. A interface é vintage: madeira, placas creme com parafusos, knobs de baquelite, telas de fósforo verde e um VU de ponteiro.

**As máquinas**
- **TAPE LOOPS** (à la Loopscape): duas formas de onda gravadas em loops de fita (Saw, Double saw, Triangle, Square, Sine, Static), com comprimentos diferentes (L e L×1,37), então os defeitos nunca se repetem no mesmo ponto. WEAR espalha quedas de volume, brilho e afinação pela fita; WOW é a oscilação lenta e o flutter; SATURATION e TONE dão o caráter. LOOP 2 pode ir uma oitava abaixo ou acima, com DETUNE em cents.
- **REED ORGAN** (à la Magnus): ERA vai do modelo de baquelite, escuro, ao de plástico dos anos 70. BREATH é o ar na palheta e SPEAK o tempo que ela leva para "falar". Em BELLOWS, cada nota a mais segurada puxa ar e o motor não dá conta: o som perde força e cai de afinação (BELLOWS SAG). MOTOR é o zumbido do ventilador.
- **TUNING FORK** (à la Spark Gap): garfo mantido por eletroímã. CLANG é a pancada inicial; DRIVE é quanto o circuito mantém o garfo soando (em 0% ele morre como um diapasão batido); SPARK é o estalo do contato; BEAT é um segundo garfo desafinado.
- **SOVIET ORGAN** (à la Junost/Lel): ondas quadradas divididas de um oscilador só. Registros 16', 8', 4' e UPPER (Octave, Fifth ou Third). Em SOUND: TONE, TREMOLO e RATE do próprio órgão, SPEAKER (o falante rasgado) e HUM (50 Hz da rede).
- **KEYS**: envelope ADSR das quatro máquinas, velocity, oitava, afinação fina, glide e bend.
- **AGE** (KEYS › Age, 0.14.0): quanto as máquinas são velhas e vivas. Cada tecla tem o seu pequeno erro de afinação e o seu próprio pedaço de fita; afinação e volume derivam devagar; o harmônio ganha uma segunda palheta levemente desafinada e tremulação; o órgão perde a quadrada perfeita (divisores assimétricos, bordas arredondadas, contato de tecla que repica); a fita passa por um pré-amplificador que satura de leve e pelo grave do cabeçote; as máquinas ganham sala (reflexões curtas) e largura estéreo. Em 0% o instrumento fica limpo e exato, como saído da fábrica; o padrão é 60%.

**O rack de efeitos** (0.10; o som passa por eles nesta ordem)
1. **DESTROY**: DRIVE com cinco tipos (Tape, Fuzz, Fold, Rectify, Shatter), CRUSH (bits) e DOWNSAMPLE. Na página RING, um modulador em anel com frequência fixa (20 Hz a 3 kHz).
2. **FILTER**: filtro ressonante Low pass, Band pass ou High pass (Off desliga). CUTOFF, RESONANCE (até quase oscilar; um limitador suave segura o volume), ENVELOPE (positivo abre, negativo fecha; reinicia a cada nota) com ENV DECAY, e um LFO próprio.
3. **CHOP**: picote no tempo da DAW (BPM) ou em milissegundos (Free). DIVISION, GATE, CHAOS (silencia ou repete passos) e REVERSE (passos tocados de trás para frente).
4. **MODULATION**, uma página para cada: CHORUS (mix, rate, depth), FLANGER (mix, rate, depth, feedback positivo ou negativo), PHASER (seis estágios, com feedback), TREMOLO (Sine, Square ou Pan, que alterna entre os lados) e VIBRATO (até 100 cents, em todas as máquinas).
5. **ECHO**: eco de fita com MIX, TIME, REPEATS e TONE (repetições mais escuras ou mais claras). SYNC prende o tempo à DAW (1/4, 1/8, 1/8 pontuada, 1/16, tercinas).
6. **SPACE**: reverb com MIX, SIZE, DECAY e DARKNESS, mais DUST (estalos de vinil enquanto as máquinas tocam).
- **Mod wheel** aumenta o desgaste e o wow da fita e o tremolo do órgão. **Aftertouch** enche o fole e aumenta a faísca do diapasão.

### Presets do RELICÁRIO (29)
Init, Eno Tape, Worn Loop, Basement Static, Drifting Tapes, Tape Bass, Bakelite, Plastic 1974, Empty Church Harmonium, Ghost Accordion, Laboratory Fork, Spark Gap, Two Forks Beating, Boing, Junost in the Hall, Broken Case, Soviet Fifth, Crooked Third, Club Bass, Organ and Tape, Fork over Harmonium, Whole Relic, Attic at 3 AM e, usando os efeitos: Chopped Tape, Phased Junost, Jet Harmonium, Dead Radio, Resonant Fork e Shattered Relic.

## RESAMPLE (RELICÁRIO, ENKI, SAMPLER)
Módulo próprio, no lugar do antigo bit crush. RATE escolhe a taxa de amostragem (20000, 10000, 7500, 5000, 2500, 1000 ou 500 Hz): o som passa por um filtro íngreme na nova metade da taxa, é amostrado nela e volta, como um áudio renderizado numa taxa baixa e trazido de volta. Os agudos somem, o corpo e o volume ficam, sem o chiado de redução de bits. SATURATION dá peso (arredonda os picos e levanta os detalhes baixos). ON/OFF no cabeçalho; trocar a taxa tocando não estala. Sons antigos que usavam crush ou downsample passam a usar a taxa mais próxima.

## Garantia de qualidade
Cada release é gerado no GitHub (Windows) e só é publicado se: os testes de som passarem (afinação, níveis, cliques, estabilidade com automação aleatória, todos os presets), e cada plugin passar pelo pluginval (Tracktion) no rigor máximo: abrir e fechar, todas as taxas de amostragem e tamanhos de bloco, automação de todos os parâmetros, estado salvo e recarregado, editor. Antes de cada envio, os testes de host rodam também no Linux.

## TIDE
EQ dinâmico para instrumentos. Vai num insert. Sem latência. Mono ou estéreo (os dois lados recebem a mesma curva).

- **Cinco bandas:** LOW (shelf de graves), BAND 2, 3 e 4 (sinos) e HIGH (shelf de agudos). Cada uma tem ON, FREQ, GAIN, WIDTH (ou SLOPE nos shelves), THRESHOLD e RANGE.
- **Fixa ou dinâmica:** com RANGE em zero, a banda é um EQ comum (GAIN). Com RANGE diferente de zero, ela escuta a própria faixa (o sino escuta a si mesmo; o shelf de graves, o que está abaixo; o de agudos, o que está acima) e, quando ela passa do THRESHOLD, move o ganho até RANGE: negativo corta, positivo realça. Abaixo do limite, nada acontece.
- **CUTS:** corte de graves e de agudos (12 dB/oitava). **RESPONSE:** ATTACK e RELEASE dos detectores.
- **Gráfico:** espectro ao fundo, a curva como ajustada (branca) e a curva se mexendo (azul). Arraste um nó: esquerda/direita é a frequência, cima/baixo é o ganho. Com ALT ou botão direito, cima/baixo define o RANGE (a linha tracejada mostra até onde a banda pode ir, e o ponto mostra onde está agora). Roda do mouse: largura. Duplo clique: zera ganho e range.
- Presets: Boomy Acoustic, Harsh Guitar Rider, Pick Attack, Bass Note Evener, Bass Growl, Vocal De-Ess, Vocal Presence, Synth Resonance Tamer, Bright When Loud. Os limites dos presets supõem um sinal com picos em torno de -6 dBFS; com sinais mais baixos, desça o THRESHOLD.

## CLEAR
Limpeza dinâmica: age só quando o problema aparece e deixa o resto como está. Vai num insert de qualquer canal, grupo ou master. Mono ou estéreo (os dois lados recebem o mesmo corte, a imagem não se mexe).

- **HARSHNESS:** na faixa entre FROM e UP TO, cada frequência é comparada com a vizinhança. O que salta (ressonância, apito, "zumbido" de caixa) é abaixado até DEPTH. Se a faixa inteira está brilhante demais em relação ao corpo do som, ela desce por igual. SENSITIVITY diz o quanto pode saltar antes de agir. WIDTH vai de largo (problemas amplos) a estreito (ressonâncias finas).
- **MUD:** procura uma "barriga" nos médios-graves em volta de WHERE, comparada à linha que liga os graves aos médios. A inclinação natural de uma mistura não conta, só o acúmulo. DEPTH é o máximo que tira.
- **RESPONSE:** SPEED (devagar e suave, ou rápido para transientes), MIX e **DELTA**, para ouvir só o que está sendo tirado. É o jeito de ajustar: se no DELTA você ouve a música inteira e não o problema, está tirando demais.
- A tela mostra o espectro e, embaixo, o que sai em cada frequência (âmbar no embolamento, rosa na aspereza).
- Com DEPTH em zero nos dois, o som sai idêntico (reconstrução exata). Latência: 2048 amostras (43 ms a 48 kHz; 4096 em 88,2 kHz ou mais), informada à DAW, que compensa.

## ENKI
Um gerador de ideias: em vez de presets, um botão que inventa sons.

- **GENERATE** cria um som novo na categoria escolhida: *Musical* (tocável, afinado), *Textures* (camadas longas), *Hits* (golpes e impactos), *Rhythmic* (pulsado, segue o andamento da DAW) ou *Weird* (qualquer coisa). O volume é nivelado sozinho.
- **MUTATE** altera o som atual; o knob AMOUNT diz quanto (pouco = variação, muito = parente distante).
- **LOCK** (nas seções SOURCE A, SOURCE B, SHAPE, MOTION e SPACE) protege essa parte do GENERATE e do MUTATE.
- **< e >** andam pelo histórico (até 100 sons). **SAVE** guarda o som em *Documentos/INVERSO ENKI/Favorites*; **FAVORITES** abre de volta.
- **WHAT ENKI MADE** descreve em palavras de que o som é feito.

Duas fontes, cada uma de um tipo: Wave, FM, Noise, Sample (gravação), Grains (granular sobre a gravação), Resonator (24 materiais, percutido ou friccionado) e Reed. Depois: filtro (passa-baixa, passa-banda, passa-alta ou comb afinado na nota), drive (5 tipos), crush, ring modulation, dois envelopes, LFO, gate rítmico, glitch e fatias invertidas, eco e reverb.

**Suas bibliotecas de samples.** No painel SOUNDS, aba MY SOUNDS: **IMPORT FOLDER** copia uma pasta inteira (uma biblioteca, com subpastas) para *Documentos/INVERSO ENKI/Sounds*; **IMPORT FILES** copia arquivos soltos para *Sounds/Imported*. Os originais não são movidos nem alterados. Lê WAV, AIFF, FLAC, OGG e MP3. Arquivos já copiados (mesmo nome e tamanho) são pulados. **DRAW FROM** escolhe de onde o GENERATE sorteia gravações (todas, só as do ENKI, ou uma biblioteca). TO A / TO B (ou duplo clique) põe o arquivo numa fonte. Gravações longas são cortadas em 45 s.

**Freesound.** Aba FREESOUND: busca sons livres (licença CC0, sem obrigação de crédito) em freesound.org. Precisa de uma chave gratuita, uma vez só: **GET A FREE KEY** abre freesound.org/apiv2/apply; crie a conta, gere a chave, cole no campo e clique **SAVE KEY**. GET > A / GET > B baixam o som para *Sounds/Freesound* e já tocam na fonte; DOWNLOAD só baixa. Um arquivo *credits.txt* registra de onde veio cada som.

## MASTER e as pistas de bateria (0.18)

Efeitos de mixagem e masterização no mesmo motor, cada um num plugin, para usar sempre como padrão.

**MASTER**, no master da sessão: EQ (passa-altas e passa-baixas de 24 dB, shelves e dois sinos; estéreo, só centro ou só laterais) > compressor com quatro caráteres (Var-mu, estilo Fairchild; VCA; FET; Opto) > compressor de cola > saturação (Tape, Tube, Transformer, com o volume compensado) > largura estéreo com graves em mono > limitador de true peak com lookahead (4x, latência de 107 amostras a 48 kHz, proporcional em outras taxas e informada ao DAW) > dither de 16, 20 ou 24 bits. O painel mostra loudness momentâneo, de curto prazo e integrado (LUFS, ITU-R BS.1770) e o pico; um clique zera. 14 presets: Clean Master, Gentle Glue, Abbey Road, Tape Warmth, Pop Loud, Rock Punch, Electronic Wide, Acoustic, Jazz and Orchestral, Dark Tape, Cinematic, Synthwave, Limiter Only e Metering Only.

**KICK, SNARE, TOMS, HATS, CYMBALS e OVERHEADS**: um por peça, cada um com os módulos que ela pede. Gate com foco de frequência (bumbo, caixa, tons e chimbau), transientes (ataque e sustain), EQ, tame (reduz o brilho áspero só quando passa do limiar), compressor, saturação, e largura nos pratos e overheads. Sem latência. 6 a 8 presets cada, por estilo (Rock, Metal, Pop, Jazz, Electronic, Lo-fi); o primeiro de cada um é o ponto de partida.

Os presets são pontos de partida: foram escolhidos por números (nível, redução, pico) e não por escuta; ajuste o limiar ao nível da sua gravação. Os testes medem o EQ contra a fórmula, a compensação de volume da saturação, o aliasing, as curvas e os tempos dos compressores, o true peak do limitador contra uma reconstrução independente 8x e o loudness contra a referência EBU (-23 LUFS).

## CAPTURE (0.19, editor na 0.20)

Pega um pedaço de uma faixa da DAW, edita no próprio plugin e manda para outra faixa. É um efeito de insert, na faixa de onde vem o áudio.

1. **Captura.** O plugin guarda o que passa enquanto a DAW toca. **CAPTURE** fixa os últimos 5, 10, 20, 30 ou 60 s (escolha em *Capture the last*). **REC** grava exatamente o que passar entre o primeiro e o segundo clique. Um plugin não enxerga a timeline nem as outras faixas: o trecho tem de passar pelo plugin, tocando.
2. **Edição.** Arraste as duas pontas na onda para a região; *gain*, *pitch* (a velocidade acompanha, como um sampler), *reverse*, *fade in/out* e *loop*. **PLAY** toca o trecho por cima da faixa (*track through* e *clip level* misturam os dois). O que se ouve é o que vai para o arquivo.
3. **BAKE / UNDO / NORMALIZE.** BAKE fixa o que você ouve e continua editando a partir dali; UNDO volta um passo; NORMALIZE leva o pico a -1 dB.
4. **Para outra faixa.** Arraste a área **DRAG TO A TRACK** para uma faixa da timeline (o plugin gera um WAV de 24 bits com a edição), ou **SAVE WAV** para guardar em qualquer pasta. No Cubase isso é arraste de arquivo; se a sua versão não aceitar, use SAVE WAV.


**Editor (0.20).** A edição é calculada em segundo plano a cada ajuste, e a onda *What you get* mostra o resultado: é o que PLAY toca e o que o arquivo contém. *Hear original* compara com a parte escolhida, sem edição.

- **Time & Pitch.** *Stretch* de 10% a 1600% sem mudar o tom, em três modos: *Beats* (ritmo e transientes), *Tones* (sons sustentados, com phase vocoder travado em fase) e *Texture* (fases aleatórias: qualquer som vira textura, por mais que se estique). *Pitch* muda o tom sem mudar a duração. *Tape* liga o tom à velocidade, como numa fita (*Pitch* soma por cima). Um esticamento fica limitado a cerca de 2 minutos de resultado. *Tape stop* (para ou parte). *Reverse*, fades e gain.
- **Tone.** Filtro (passa-baixas, passa-altas, passa-faixa) com varredura de *From* a *To*; drive (tape, tube, transformer); crush (bits e taxa); ring.
- **Space & Rhythm.** Eco (tempo, feedback, escuridão), reverb (tamanho, escuridão) e chop (gate ou stutter por fatias).
- **Abrir arquivos.** *Open* ou soltar um WAV, AIFF, FLAC ou MP3 no plugin: edita áudio que não veio da faixa (até 60 s).

O trecho capturado fica salvo na sessão (FLAC 24 bits dentro do estado do plugin, até 60 s).

## Instalação
No GitHub, abra **Releases > INVERSO ABISMO 0.20.0** e baixe `INVERSO_ABISMO_Setup_0.20.0_x64.exe` (instalador) ou `INVERSO_ABISMO_PORTABLE_VST3_0.20.0.zip` (copie a pasta `INVERSO ABISMO.vst3` para `C:\Program Files\Common Files\VST3`). Feche a DAW antes de instalar e, se os plugins não aparecerem, faça um novo scan de plugins. Os sete aparecem como plugins da Inverso Studios: ABISMO, RELICÁRIO e ENKI em *Synth*, CLEAR e TIDE (*EQ*) nos efeitos, ABISMO SAMPLER em *Sampler* e ABISMO DRUMS em *Drum*. A versão portátil traz as sete pastas `.vst3` e a pasta INVERSO CABS. O instalador põe as IRs em Documentos\INVERSO\INVERSO CABS (Guitar e Bass). Só as IRs: `INVERSO_CABS_IRs.zip` no mesmo release.

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
- `relicario_engine` verifica a afinação das quatro máquinas (440 Hz em 44,1/48/96 kHz), que o desgaste da fita varia ao longo do loop, que o fole cede com muitas notas, que o diapasão sustenta com eletricidade e morre sem ela, a quinta do órgão, o zumbido do falante rasgado e o release até o silêncio. Testa cada efeito do rack (todos mudam o som e ficam abaixo de 0 dBFS, o filtro ressonante no máximo em qualquer frequência, o chop cortando no tempo, o eco sincronizado em 1/4). Também grava uma demo WAV de cada preset e faz automação aleatória de todos os parâmetros.
- `relicario_host_smoke` (JUCE) toca todos os presets em vários tamanhos de bloco, cada máquina sozinha, o pedal de sustain, o all sound off e o salvamento do estado.
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
