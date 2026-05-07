# Formula E-Motion

Aplicativo em Qt Widgets para receber `RPM`, `vel` e `tq` via porta serial, exibir os valores em tempo real, desenhar o histórico dos últimos 30 segundos e exportar os dados para CSV separado por `;`.

## Como abrir no Qt Creator

1. Abra o arquivo `guiEMOTION.pro`.
2. Selecione o kit `Desktop Qt 6.11.0 MinGW 64-bit` ou equivalente.
3. Compile e execute.

O executável de teste já foi gerado em `release/guiEMOTION.exe`.

## Formato serial esperado

O programa aceita uma linha por amostra, terminada com `\n`.

Exemplos aceitos:

```text
RPM=3500;VEL=72.4;TQ=120.2
3500;72.4;120.2
RPM:3500 VEL:72.4 TQ:120.2
```

## Taxa de atualização

- O app foi pensado para receber uma amostra a cada `100 ms`.
- O gráfico mostra apenas os últimos `30 s`.
- O botão `SALVAR CSV` exporta toda a sessão capturada.

## Baud rate

O valor padrão está em `mainwindow.cpp`:

```cpp
constexpr qint32 kDefaultBaudRate = 115200;
```

Se o seu Arduino estiver em `9600`, troque esse valor e recompile.
