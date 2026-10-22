# INVERSO WaveMover 0.1.0

Editor criativo de samples: plugin VST3 de efeito para Windows x64 e aplicativo independente.

## Usar

1. Instale com o Cubase fechado. Abra **INVERSO WaveMover** como efeito de insercao.
2. Use **Abrir audio**, solte um arquivo real sobre a forma de onda ou clique **Capturar pista**, reproduza o trecho na DAW e clique **Finalizar captura**. Captura: ate 120 segundos; arquivos mono/estereo ate 256 MB de audio decodificado.
3. Arraste sobre a forma de onda para selecionar. Clique numa fatia para seleciona-la. Alt+clique posiciona um cursor sem selecionar uma fatia. Shift estende a selecao.
4. Crie splits manualmente, pela grade musical ou por ataques. A grade usa o BPM indicado e comeca no inicio do sample. Splits sozinhos nao modificam o som.
5. Escolha uma transformacao, ajuste A/B e clique **Aplicar a selecao**. Cada operacao deixa o resultado selecionado para a proxima transformacao. Desfazer/refazer e Restaurar original preservam o material de origem.
6. Clique **Ouvir**. No plugin, a previa sai pela DAW e precisa de seu motor de audio ativo. Durante a previa, o sample substitui temporariamente o sinal da entrada. **Ouvir original** reproduz o arquivo original inteiro.
7. **Exportar WAV** escreve o resultado em 32-bit float. **Exportar selecao** limita a exportacao ao trecho marcado. Para arrastar, clique **Preparar arraste**, espere ficar pronto e arraste o botao **ARRASTE para a DAW**.

Arquivos anunciados: WAV, AIFF, FLAC, MP3 e OGG, sujeitos a codecs e arquivos validos. O primeiro teste automatizado de importacao/exportacao utiliza WAV float mono e estereo. Arrastes de bancos proprietarios ou eventos internos da DAW dependem de ela fornecer um arquivo compativel. Capturar a entrada e abrir um arquivo sao caminhos independentes desse arraste.

## Ferramentas

Reverse, corte, copia/colagem, duplicacao, trim, silencio, fades ajustaveis e por alcas, crossfade no cursor, ganho, normalizacao explicita, stretch preservando afinacao, pitch preservando duracao, rampa linear de BPM, velocidade de fita, tape stop/start, reverb com cauda, reverberacao isolada e invertida, delay, filtros, saturacao, repeticoes, ratchet, ping-pong, freeze granular e embaralhamento de fatias com semente.

Stretch: 25 a 400% da duracao. Pitch: -24 a +24 semitons. A rampa de BPM usa o primeiro valor como tempo do arquivo original e como inicio da rampa; o segundo e o tempo no final, calculado ao longo da posicao no material original. Para uma conversao de BPM fixa, escolha **BPM de origem para BPM de destino** e informe os dois valores. As transformacoes alteram o audio dentro do editor e nao escrevem no mapa de tempo da DAW. Tape stop/start incluem um fade de 5 ms na ponta correspondente.

O crossfade sobrepoe as amostras dos dois lados do cursor pelo campo Crossfade ms e encurta o arquivo pelo tamanho da sobreposicao. Repetir aumenta a duracao; ratchet mantem a duracao e repete o inicio da selecao em subdivisoes. Reverb e delay estendem o arquivo e podem sobrepor audios dentro da selecao; as operacoes substituem a selecao e deslocam o material posterior. Caudas sao incluidas na exportacao do resultado inteiro.

Nenhum efeito esta aplicado ao abrir um arquivo. Operacoes sao renderizadas na selecao, com original e historico preservados; esta versao nao possui um rack reeditavel de efeitos por fatia, desenho livre de curvas de BPM, ARA, nem automacao dos efeitos offline pelo host. O estado da sessao inclui o audio original, o editado, splits e selecao; o historico de desfazer nao e salvo. Arquivos e sessoes grandes consomem memoria e disco proporcionalmente. O historico e limitado a 32 entradas e um orcamento aproximado de 256 MB.

## Validacao

O workflow Windows compila DSP, plugin, aplicativo e host de teste. Testa cortes, fades, comprimentos, afinacao, rampas, granular, caudas, captura de blocos variaveis, WAV float, historico, estado com audio incorporado, editor nativo e carregamento do VST3 compilado. Isso nao substitui uma verificacao no Cubase real; ela ainda nao foi executada pelo desenvolvedor nesta sessao.

Compilar requer JUCE 8.0.16-security e os commits de Signalsmith indicados no workflow. Fontes proprias em C++17. Signalsmith Stretch e Linear sao MIT; os avisos de terceiros acompanham os pacotes. O uso e distribuicao de JUCE seguem os termos de licenciamento JUCE aplicaveis.
