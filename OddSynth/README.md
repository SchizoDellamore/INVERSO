# INVERSO OddSynth 0.1.0

Instrumento VST3 para Windows x64, com aplicativo independente. Projeto de F. Caldeira-Scherner / INVERSO STUDIOS. Interface laranja e grafite.

## Instalar e tocar

Feche a DAW e execute o instalador. Ele cria a pasta VST3 de sistema quando necessario. No Cubase, adicione uma **pista de instrumento** e procure **INVERSO OddSynth**. Nao carregue como efeito de insercao. Escolha uma entrada MIDI ou clique no teclado do plugin. No aplicativo independente, selecione sua saida de audio nas configuracoes.

## Instrumento

Quatro motores sintetizados: Materia (parciais inarmonicas e excitacao ruidosa), Garganta (voz sintetica por harmonicos/formantes), Enxame (vozes desafinadas em estereo) e Pulso (excitacao periodica). Nenhuma gravacao de compositores ou de terceiros foi usada. Os nomes de compositores orientam a pesquisa estetica; os motores nao sao emulacoes de instrumentos ou de suas trilhas.

Seis controles: Atrito, Instabilidade, Densidade, Brilho, Garganta e Pulso. Cada um tem valor manual, dois limites de variacao, valor ouvido agora e trava.

- **Manual:** nao ha modulacao aleatoria dos controles.
- **Dentro dos limites:** movimentos suaves dentro das faixas autorizadas.
- **Livre:** movimentos mais irregulares, ainda respeitando faixas e travas.
- **Quantidade zero:** volta aos valores manuais.
- **Travar:** preserva o controle contra movimento automatico, Variar timbre, macro Tensao e morph A/B. O valor manual pode ficar fora da faixa de variacao.
- **Congelar:** para a evolucao dos controles no ponto atual; o som e os envelopes continuam.
- **Semente e Reiniciar percurso:** permitem reproduzir uma variacao. Repetir ao iniciar DAW reinicia o percurso na passagem de parado para tocando. Para repetir o audio inteiro, use as mesmas notas, duracoes, expressoes e efeitos a partir de silencio; efeitos com memoria e notas ainda ativas tambem influenciam o resultado.
- **Variar timbre:** altera os seis valores de forma limitada pela Quantidade, respeitando travas e faixas. Desfazer recupera a configuracao anterior.

12 vozes, velocity, pitch bend de dois semitons, sustain por canal, CC1 para Tensao e CC11 para expressao. A saida comeca em -18 dB. Reverb, delay e distorcao comecam desligados, inclusive nos presets de fabrica.

## A/B e guardar resultados

Guardar A e Guardar B conservam duas alternativas completas, incluindo audio do sample. Ouvir A/B recupera a alternativa. Morph interpola os seis controles e os niveis dos quatro motores e do sample, preservando controles travados; nao interpola arquivos, efeitos, sementes ou faixas de variacao. As alternativas sao salvas com o preset e com o projeto da DAW.

**Guardar audio recente** exporta ate os ultimos 15 segundos ouvidos em WAV estereo de 24 bits. A captura ocorre continuamente, incluindo efeitos. E preciso tocar audio antes de exportar.

## Aba Samples

Carregue um arquivo ou arraste um arquivo de audio para a aba Samples. Suporte a WAV, AIFF, FLAC, Ogg e MP3. Arraste na forma de onda para selecionar o trecho; Inicio e Fim tambem permitem ajuste numerico normalizado. Selecionar tudo restaura a faixa completa.

Reverse trecho, Manter trecho, Apagar trecho, Fade in/out e velocidade 0.5x/2x alteram o audio selecionado e aceitam Desfazer/Refazer. A velocidade altera duracao e afinacao juntas. A leitura por MIDI transpoe a partir de C4. Granular reorganiza o trecho em graos, com duracao ajustavel; Loop e Reverse na reproducao sao controles independentes das edicoes gravadas no audio.

O botao Tocar/parar C4 dispara o patch atual, incluindo os motores ativos. Para ouvir apenas o sample, zere os quatro motores e ajuste o nivel Sample. Exportar WAV exporta o sample editado, sem os motores nem os efeitos do instrumento; Guardar audio recente exporta o som ouvido completo.

Limite de importacao: 120 segundos e 8 milhoes de amostras por canal. Arquivos mono sao reproduzidos nos dois canais. O sample editado e incluido no estado salvo, evitando dependencia do arquivo externo; arquivos grandes aumentam o tamanho do preset/projeto.

O arraste implementado recebe **arquivos**. Um browser ou evento da DAW que nao ofereca um arquivo ao plugin pode nao permitir esse arraste; exporte/bounce o trecho e carregue-o como arquivo. Nao ha captura de uma pista de audio diretamente neste instrumento.

## Validacao e compilacao

Os testes verificam limites, travas, congelamento, repeticao por semente, vozes MIDI, sustain, reproducao e edicao de samples. O fluxo Windows compila o VST3 e aplicativo, instancia o VST3 em um host JUCE, verifica audio e restauracao do sample no estado, registra imagens das duas abas e testa instalacao/desinstalacao. Isso nao substitui um teste no Cubase do computador de destino.

JUCE fixado no commit ea77444750de45e036e0279bb0291144bf8e3688 (8.0.16-security). Para compilar: `cmake -S OddSynth -B OddSynth/build -A x64 -DJUCE_ROOT=CAMINHO_JUCE`, seguido de `cmake --build OddSynth/build --config Release` e `ctest --test-dir OddSynth/build -C Release --output-on-failure`. O motor portavel tambem pode ser testado sem JUCE com `-DODDSYNTH_CORE_ONLY=ON`.
