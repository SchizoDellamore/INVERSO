# INVERSO EQ v0.4.0 — Windows VST3

Versão de aperfeiçoamento do equalizador testado no Cubase. Interface discreta com 8 bandas paramétricas, filtros High-Pass e Low-Pass sempre acessíveis, espectro Pre/Post e ajuste fino pela roda do mouse (Shift / Alt / Ctrl).

## Alterações da 0.4

- Transição de 7 ms nos coeficientes IIR ao modificar frequência, ganho, Q ou inclinação. Reduz saltos instantâneos que poderiam gerar cliques durante a automação.
- Fade curto ao ligar/desligar bandas, HPF e LPF; ganho de saída também suavizado.
- FFT de 4096 pontos no analisador para melhor discriminação dos graves; interpolação entre bins e visualização mais discreta (20 fps).
- Captura do espectro desativada quando a janela do plugin fecha ou o analisador é colocado em Off, reduzindo trabalho de processamento.
- Valores mais curtos na interface (por exemplo, `39,0 Hz`, `1,50 kHz`, `0,1 dB`, `Q 1,00`). Parâmetros retêm a precisão interna.
- Novo teste de automação rápida em 44,1/48/96 kHz.

## Limites e transparência

**PROTECT continua sendo um teto para picos de amostra, não true peak.** A proteção estéreo instantânea não possui lookahead e pode introduzir distorção quando atenua transientes. Não garante ausência de picos interamostras ou clipping em etapas posteriores da DAW. Medição true peak, EQ dinâmico e medição formal de consumo de CPU continuam pendentes.

A versão 0.4 mantém os mesmos IDs dos parâmetros e o mesmo identificador VST3 da 0.3. Foi concebida para preservar sessões anteriores. Ainda são necessários testes de abertura de sessões antigas no Cubase.

## Compilação automática no GitHub

Abra **Actions → Build Windows INVERSO EQ VST3 installer**. Em execução concluída com sucesso, baixe o artifact `INVERSO_EQ_Setup_0.4.0_x64` e extraia o ZIP para encontrar o `.exe`. Feche o Cubase antes da instalação; reabra após instalar.

Para compilar manualmente num Windows com VS2022, CMake, Inno Setup e JUCE:

```powershell
.\BUILD_AND_PACKAGE.ps1 -JucePath 'C:\SDK\JUCE'
```

Testes DSP independentes de JUCE:

```bash
cmake -S . -B build-dsp -DINVERSO_DSP_TESTS_ONLY=ON
cmake --build build-dsp
ctest --test-dir build-dsp --output-on-failure
```

Distribuição: observar os termos de licenciamento JUCE. O instalador não possui assinatura digital.
