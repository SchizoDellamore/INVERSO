# OddSynth: requisitos e escolhas

## Requisitos aprovados por F. Caldeira-Scherner

- Instrumento para compor desconforto, citando Joseph Bishara, Cristobal Tapia de Veer e Christopher Young como referencias.
- Quatro motores combinaveis: Materia, Garganta, Enxame e Pulso.
- O compositor deve controlar o resultado e poder permitir graus de imprevisibilidade.
- Aba dedicada a manipular samples.
- Identidade da familia de plugins INVERSO: laranja e grafite; alvo Windows x64/VST3/Cubase.
- Efeitos espaciais e distorcao desligados por padrao.

## Escolhas desta implementacao

- Autonomia Manual / Dentro dos limites / Livre; Livre sempre obedece os limites do usuario.
- Valores manuais, dois extremos de faixa, valor ouvido agora e trava por dimensao.
- Trava durante o movimento captura o valor ouvido, evitando saltar de volta ao valor inicial.
- Congelamento do movimento, semente reproduzivel, reinicio explicito e opcional no inicio do transporte.
- A/B completo, morph de controles/motores, desfazer/refazer e captura retrospectiva de audio.
- Samples importados e editados permanecem embutidos no estado; manipulacoes com selecao, undo, exportacao e leitura granular.
- Captura retrospectiva exporta audio; guardar A/B guarda configuracao e posicao do movimento. Nao se promete recriar por preset as fases internas de todas as vozes/efeitos que estavam soando; para preservar exatamente uma surpresa ouvida, exporta-se o audio.
- Nenhum efeito e ativado automaticamente por aleatoriedade ou preset de fabrica.
- Volume de saida e separado da autonomia.
- Samples podem alimentar um motor escolhido, preservando os dois canais. O comportamento granular usa as dimensoes compartilhadas Densidade e Instabilidade.

## Fontes primarias da pesquisa estetica

- Joseph Bishara, entrevista a Michael Gingold na FANGORIA: quarteto de cordas, piano e estrutura de piano abandonada em Insidious; contrastes de dinamica. https://www.fangoria.com/interview-bishara-insidious/
- Cristobal Tapia de Veer, notas do compositor no encarte oficial de Utopia, Silva Screen: gravacoes de campo e transformacoes profundas do som. https://www.silvascreen.com/wp-content/uploads/SILCD1437-UTOPIA-DigitalBooklet.pdf
- Christopher Young, entrevista a Ruby Gartenberg no Pop Disciple: vozes, violoncelo e sons nao instrumentais transformados em Pet Sematary. https://www.popdisciple.com/interviews/christopher-young

Os motores e seus nomes sao escolhas de projeto. A pesquisa nao estabelece uma resposta emocional universal aos timbres e nao permite afirmar equivalencia entre o plugin e as obras citadas.
