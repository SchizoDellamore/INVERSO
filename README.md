# INVERSO EQ v0.3.0 — projeto VST3 + instalador Windows

## O que este pacote CONTÉM
- Código C++17 do equalizador e do plugin JUCE (continuidade do protótipo 0.2).
- Projeto CMake para gerar o plugin **VST3 x64**.
- Interface JUCE: espectro Pre/Post, oito bandas, HPF/LPF e ajuste por roda do mouse (Shift extra fino; Alt para Q e Ctrl para Hz sobre pontos).
- Proteção contra **pico de amostra** ativada por padrão; aviso no medidor; isso **não é** um limitador true peak.
- Instalador Inno Setup em `Installer/INVERSO_EQ.iss`, com instalação global na pasta VST3 padrão e desinstalação.
- `BUILD_AND_PACKAGE.ps1`: compila, testa e gera `dist/INVERSO_EQ_Setup_0.3.0_x64.exe` **num Windows com ferramentas instaladas**.
- `.github/workflows/windows-installer.yml`: permite compilar o mesmo instalador em um runner Windows do GitHub Actions.
- Testes DSP C++ independentes de JUCE, executáveis localmente e no Windows.

## IMPORTANTE: o .exe não está incluído
Este ZIP foi gerado em Linux, sem Visual Studio nem o SDK JUCE disponível. Portanto o **VST3 não foi compilado nem executado no Cubase, Studio One ou Ableton**, e o instalador `.exe` não foi gerado. O arquivo `.iss` é uma receita real de instalação, mas só produz o `.exe` após a compilação do VST3 no Windows. Não tente instalar a prévia HTML como VST3.

## Windows 11 x64: compilação e instalador
1. Instale [Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/community/) com **Desenvolvimento para desktop com C++**.
2. Instale [CMake](https://cmake.org/download/), **Git** e [Inno Setup 6.3+](https://jrsoftware.org/isinfo.php).
3. Extraia o pacote em um caminho local sem sincronização, por exemplo `C:\Projetos\INVERSO_EQ`.
4. Abra o PowerShell nessa pasta e execute:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\BUILD_AND_PACKAGE.ps1
   ```
   Se já possuir JUCE 8 instalado, use:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\BUILD_AND_PACKAGE.ps1 -JucePath "C:\SDK\JUCE"
   ```
5. O script baixa JUCE 8.0.16-security apenas se não houver `JUCE` local, configura CMake, compila em Release, executa testes e produz:
   `dist\INVERSO_EQ_Setup_0.3.0_x64.exe`.
6. Feche o Cubase, execute o instalador como administrador, abra o Cubase e faça a varredura VST3. O local padrão é `C:\Program Files\Common Files\VST3\INVERSO EQ.vst3`.

Se falhar qualquer etapa, o script encerra com erro e não anuncia sucesso. O instalador não tem assinatura digital, então o Windows pode emitir alerta SmartScreen.

### Alternativa: compilar sem instalar Visual Studio localmente
O pacote inclui `.github/workflows/windows-installer.yml`. Ao colocar estes arquivos em um repositório GitHub e rodar o workflow **Build Windows INVERSO EQ VST3 installer**, o executável resultante ficará como artefato do workflow se o build passar. Esta opção exige uma conta/repositório no GitHub e acesso à internet.

## Estado técnico e precauções
- 8 bandas bell, 12/24/48 dB/oct HP/LP, até 18 dB em cada banda, ganho de saída; spectrum FFT 2048 no plugin JUCE.
- Automação e estado da sessão JUCE previstos no código.
- Controle de picos por amostra antes/depois da proteção; não mede picos interamostras, e a proteção pode provocar distorção audível quando trabalha forte.
- Mudanças rápidas em filtros podem produzir clicks: **ainda não há interpolação dos coeficientes**.
- Não há EQ dinâmico, processamento de fase linear, oversampling true peak ou garantia de clipping zero em etapas posteriores da DAW.
- **Não substituir o equalizador principal em projetos importantes sem testes de escuta e medição.**
- JUCE usa licenciamento dual open source/comercial: verifique as condições adequadas à distribuição antes de publicar. O framework JUCE não é redistribuído neste ZIP.

## Testes de DSP no Linux (sem JUCE)
```bash
cmake -S . -B build-dsp -DINVERSO_DSP_TESTS_ONLY=ON
cmake --build build-dsp
ctest --test-dir build-dsp --output-on-failure
```

**Versão 0.3 é um marco de empacotamento, não uma alegação de equivalência sonora com a FabFilter.**
