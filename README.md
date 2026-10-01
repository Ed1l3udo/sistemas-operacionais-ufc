# Process Lab — simulador de escalonamento de processos

O Process Lab é um simulador acadêmico de políticas de escalonamento de CPU. O
projeto permite descrever uma carga de processos, executar sete algoritmos sobre a
mesma entrada e comparar a timeline e as métricas resultantes.

Todas as regras de escalonamento são implementadas em C17. A mesma aplicação pode
ser usada de duas formas:

- pela linha de comando, com saída textual para leitura humana ou JSON;
- por uma interface web local, que chama o executável C e apresenta os resultados
  de forma interativa.

## Funcionalidades

- simulação discreta, com uma decisão de escalonamento por segundo;
- FCFS, SJF, SRTF, prioridade não preemptiva, prioridade preemptiva,
  Round-Robin e Round-Robin com prioridade e envelhecimento;
- métricas individuais e médias de turnaround, espera e resposta;
- contagem de trocas de contexto e timeline completa da CPU;
- desempates pseudoaleatórios reproduzíveis por semente;
- rastreamento opcional das decisões tomadas pelo motor;
- comparação visual dos algoritmos e reprodução da timeline no navegador.

## Requisitos

- GCC com suporte a C17;
- GNU Make;
- Node.js 18 ou mais recente, somente para a interface web.

O projeto não usa bibliotecas externas nem exige instalação por gerenciador de
pacotes.

### Windows

Use preferencialmente o terminal **MSYS2 UCRT64**, com GCC, GNU Make e Node.js no
`PATH`. Dependendo da instalação, o GNU Make pode estar disponível como
`mingw32-make`; nesse caso, substitua `make` por `mingw32-make` nos comandos deste
documento.

O executável produzido no Windows é `build/scheduler.exe`. Em Linux, o nome é
`build/scheduler`.

## Compilação

Na raiz do repositório, execute:

```sh
make
```

O comando compila os arquivos de `src/` em C17 e gera o executável dentro de
`build/`. Para remover os artefatos de compilação:

```sh
make clean
```

## Execução pela linha de comando

A CLI recebe a lista de processos por `stdin` e exige um arquivo de configuração.
Para executar todos os algoritmos com os exemplos incluídos no projeto:

```sh
./build/scheduler \
  --config examples/config.txt \
  --algorithm all \
  --format text \
  --seed 42 < examples/processes.txt
```

No Windows, execute `./build/scheduler.exe` no terminal MSYS2.

### Formato dos processos

Cada linha da entrada representa um processo e contém três inteiros:

```text
chegada duração prioridade
```

Exemplo:

```text
0 5 2
0 2 3
1 4 1
3 3 4
```

A chegada deve ser maior ou igual a zero; duração e prioridade devem ser
positivas. Prioridades numericamente menores são mais altas. Os IDs `P1`, `P2`,
etc. são atribuídos na ordem das linhas, mesmo que as chegadas estejam fora de
ordem. Linhas vazias e linhas iniciadas por `#` são ignoradas.

### Arquivo de configuração

O arquivo informado em `--config` deve declarar o quantum e a taxa de aging:

```text
quantum:2
aging:1
```

O quantum deve ser positivo. O aging pode ser zero para desativar a melhora
gradual de prioridade no Round-Robin com prioridade.

### Opções da CLI

| Opção | Descrição | Padrão |
| --- | --- | --- |
| `--config ARQUIVO` | Caminho da configuração; é obrigatório. | — |
| `--algorithm NOME` | Algoritmo a executar ou `all`. | `all` |
| `--format FORMATO` | Saída `text` ou `json`. | `text` |
| `--seed N` | Semente inteira sem sinal para desempates. | `42` |
| `--trace` | Inclui as decisões por segundo; exige JSON e um único algoritmo. | desativado |
| `--help`, `-h` | Exibe a ajuda da CLI. | — |

Os nomes aceitos em `--algorithm` são `fcfs`, `sjf`, `srtf`, `priority-np`,
`priority-p`, `rr`, `priority-rr` e `all`.

Para obter JSON:

```sh
./build/scheduler \
  --config examples/config.txt \
  --algorithm all \
  --format json \
  --seed 42 < examples/processes.txt
```

Para inspecionar as decisões de um algoritmo segundo a segundo:

```sh
./build/scheduler \
  --config examples/config.txt \
  --algorithm srtf \
  --format json \
  --trace < examples/processes.txt
```

Resultados válidos são escritos em `stdout`; ajuda e diagnósticos de erro usam os
fluxos apropriados sem misturar mensagens com o JSON. Entradas inválidas encerram
o programa com código diferente de zero.

## Execução da interface web

Compile o motor e inicie o servidor local com:

```sh
make web
```

Depois, acesse [http://127.0.0.1:3000](http://127.0.0.1:3000). Para escolher outra
porta em um shell compatível com POSIX:

```sh
PORT=8080 make web
```

A página permite editar processos, selecionar algoritmos, comparar métricas e
percorrer a timeline. O navegador não reimplementa os algoritmos: ele envia a
entrada ao servidor Node.js, que executa o mesmo binário C usado pela CLI.

## Organização do repositório

```text
include/      contrato público compartilhado pelos módulos C
src/          CLI, leitura de entrada, motor e serialização
web/          servidor Node.js e interface estática
examples/     carga de processos e configuração de demonstração
docs/         documentação técnica e guia de leitura do código
```

Para estudar a implementação arquivo a arquivo, consulte o
[guia de código](docs/guia-de-codigo.md). As regras arquiteturais e decisões do
simulador estão resumidas no [documento técnico](docs/arquitetura.md).
