# INVERSO EQ v0.5.0 — Windows VST3

Equalizador paramétrico independente (8 bandas), High-Pass e Low-Pass permanentes, analisador de espectro Pre/Post/ambos e controle fino pela roda do mouse. Projeto desenvolvido com C++17 e JUCE.

## Novidades v0.5

- Estimativa de **true peak 4×** por interpolador FIR (16 amostras) em dois pontos: antes e depois da proteção. Detecta picos interamostras que um medidor comum de picos de amostra não detectaria.
- Proteção estéreo vinculada com **~3 ms de lookahead** e liberação suave de 120 ms; proteção ligada por padrão com alvo de -1 dBTP **estimado**. Redução de ganho compartilhada entre canais. Margem interna de 0,2 dB.
- Latência fixa, registrada na DAW, mesmo com proteção desligada, para preservar o alinhamento de canais e de caminhos paralelos.
- Métricas no cabeçalho: entrada (picos de amostra), pré-proteção e saída (estimativas de true peak), aviso próximo ao teto e redução de ganho.
- Quatro fases do interpolador computadas com uma única leitura do histórico, sem alocações no processamento de áudio. Analisador FFT continua sob demanda.
- Testes automatizados do processamento, transientes, latência, automação e 16/32/64 instâncias.

## Limitações importantes

**A medição ainda NÃO possui validação de conformidade com a ITU-R BS.1770 e a proteção NÃO é um limitador brick-wall true peak certificado.** O interpolador é uma aproximação 4×, e a modulação de ganho pode criar pequenos picos interamostras. O alvo de saída deve ser entendido como operacional, não como garantia matemática/contratual. Para mastering e distribuição final, use medidor/limitador true peak de referência na saída principal.

A proteção aplica redução de ganho, especialmente quando há picos fortes; isso altera dinâmica/transientes. Usar 'PROTECT' não substitui monitorar toda a cadeia de sinal no Cubase. O atraso é aproximadamente 3 ms (amostras exatas dependem da frequência de amostragem), compensado pela DAW se houver suporte a delay compensation.

A versão 0.5 preserva IDs de parâmetros e identificador VST3 anteriores. Ainda é necessário verificar a abertura de projetos antigos e automação no Cubase. **Teste primeiro em uma cópia do projeto**; não substitua a única cópia de produção.

## Como instalar

No GitHub, abra **Actions > Build Windows INVERSO EQ VST3 installer** e selecione a execução verde mais recente. Em **Artifacts**, baixe `INVERSO_EQ_Setup_0.5.0_x64`. Extraia o ZIP e execute o `.exe` com o Cubase fechado. O instalador substitui a versão anterior na pasta VST3 padrão.

## Compilação e testes

Compilação Windows automatizada: GitHub Actions utiliza Windows Server 2022, JUCE 8.0.16-security, Visual Studio 2022, CMake e Inno Setup. O instalador não é assinado digitalmente.

Testes DSP portáteis:

```bash
cmake -S . -B build-dsp -DINVERSO_DSP_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp
ctest --test-dir build-dsp --output-on-failure
```

O benchmark em `test_instances.cpp` processa 16/32/64 instâncias de forma sequencial, em um único thread. Mede o custo da execução DSP naquela máquina, **não** o consumo de CPU de uma sessão real de Cubase.

Para compilar manualmente no Windows (VS2022, CMake, Inno Setup e JUCE instalados):

```powershell
.\BUILD_AND_PACKAGE.ps1 -JucePath 'C:\SDK\JUCE'
```

Distribuição depende dos termos de licenciamento JUCE e dos componentes utilizados.
