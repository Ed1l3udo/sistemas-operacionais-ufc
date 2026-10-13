# Simulador de Escalonamento de Processos

Projeto da disciplina de **Sistemas Operacionais** que simula, em C17, o
escalonamento de processos em tempo discreto. Ele foi desenvolvido a partir do
enunciado [Tarefa 01 - Escalonamento de Processos](Tarefa%2001%20-%20Escalonamento%20de%20Processos.pdf).

O programa lê processos pela entrada padrão, executa um ou todos os algoritmos
suportados e apresenta métricas e uma linha temporal vertical. O código é
organizado em módulos C e possui testes automatizados.

## Algoritmos

| Algoritmo | Preemptivo? | Opção da CLI |
| --- | --- | --- |
| FCFS | Não | `fcfs` |
| Shortest Job First (SJF) | Não | `sjf` |
| Shortest Remaining Time First (SRTF) | Sim | `srtf` |
| Prioridade sem preempção | Não | `priority-np` |
| Prioridade com preempção | Sim | `priority-p` |
| Round-Robin | Sim, na fronteira do quantum | `rr` |
| Round-Robin com prioridade e aging | Sim, na fronteira do quantum | `priority-rr` |

No Round-Robin com prioridade não há preempção no meio de um quantum: a
prioridade dinâmica é considerada no próximo despacho.

## Funcionalidades

- leitura e validação de processos por `stdin`;
- configuração de `quantum` e `aging` em arquivo de texto;
- execução de um algoritmo ou de todos os sete;
- desempate reproduzível por seed;
- métricas individuais e médias de turnaround, espera e resposta;
- contagem uniforme de trocas de contexto;
- timeline vertical com o estado de cada processo;
- saída JSON estável para integração com outras aplicações;
- testes automatizados para entrada, núcleo, algoritmos, métricas, saída e CLI.

Servidor e interface web não fazem parte da implementação atual. Eles podem
consumir o modo JSON em extensões futuras.

## Estrutura do repositório

```text
.
├── simulator/
│   ├── include/       # interfaces e estruturas públicas em C
│   ├── src/           # implementação do simulador e executável
│   ├── tests/         # suíte automatizada e fixtures
│   └── Makefile       # alvos de compilação, teste e limpeza
├── examples/          # configuração e processos de exemplo
├── docs/              # documento técnico de implementação
└── Tarefa 01 - Escalonamento de Processos.pdf
```

## Requisitos

- compilador compatível com C17;
- GCC ou equivalente;
- `make`, opcionalmente, para usar os alvos fornecidos pelo Makefile.

## Compilação

Entre no diretório `simulator` antes de compilar:

```bash
cd simulator
make
```

O executável gerado é `scheduler` (ou `scheduler.exe` no Windows) dentro de
`simulator/`. Para limpar objetos e executáveis:

```bash
make clean
```

Quando `make` não estiver disponível, o alvo principal do Makefile equivale a:

```bash
gcc -Iinclude -std=c17 -Wall -Wextra -Wpedantic src/*.c -o scheduler
```

No PowerShell, uma forma equivalente é:

```powershell
$sources = Get-ChildItem src\*.c | ForEach-Object FullName
gcc -Iinclude -std=c17 -Wall -Wextra -Wpedantic @sources -o scheduler.exe
```

## Configuração

O arquivo de configuração deve conter exatamente os campos `quantum` e
`aging`, um por linha. O exemplo em `examples/config.txt` é:

```text
quantum:2
aging:1
```

- `quantum` deve ser maior que zero;
- `aging` deve ser maior ou igual a zero;
- a prioridade de um processo deve ser estritamente positiva;
- `1` é a prioridade máxima: quanto menor o número, maior a prioridade.

## Entrada de processos

Cada linha recebida por `stdin` tem o formato:

```text
instante_de_criação duração prioridade
```

Exemplo oficial, também disponível em `examples/processes.txt`:

```text
0 5 2
0 2 3
1 4 1
3 3 4
```

Os IDs (`P1`, `P2` e assim por diante) são atribuídos na ordem original das
linhas. A entrada não precisa estar ordenada pelo instante de chegada. A
criação deve ser maior ou igual a zero, a duração deve ser maior que zero e a
prioridade deve ser maior que zero.

## Execução

Em sistemas com shell compatível com POSIX:

```bash
./scheduler ../examples/config.txt < ../examples/processes.txt
./scheduler ../examples/config.txt --algorithm all --seed 0 < ../examples/processes.txt
./scheduler ../examples/config.txt --algorithm fcfs < ../examples/processes.txt
./scheduler ../examples/config.txt --algorithm sjf < ../examples/processes.txt
./scheduler ../examples/config.txt --algorithm srtf < ../examples/processes.txt
./scheduler ../examples/config.txt --algorithm priority-np < ../examples/processes.txt
./scheduler ../examples/config.txt --algorithm priority-p < ../examples/processes.txt
./scheduler ../examples/config.txt --algorithm rr < ../examples/processes.txt
./scheduler ../examples/config.txt --algorithm priority-rr < ../examples/processes.txt
./scheduler ../examples/config.txt --algorithm all --seed 0 --json < ../examples/processes.txt
./scheduler --help
```

`all` é o padrão. A opção `--seed 0` aceita qualquer inteiro sem sinal de 64
bits e torna os desempates reproduzíveis. Sem `--seed`, o programa cria uma
seed com os relógios portáveis `time()` e `clock()` e a imprime no início da
saída.

No PowerShell, use um pipeline para fornecer a entrada:

```powershell
Get-Content ..\examples\processes.txt | .\scheduler.exe ..\examples\config.txt --algorithm all --seed 0
```

## Saída

Cada seção apresenta:

- **TT médio**: turnaround, ou conclusão menos criação;
- **TW médio**: tempo de espera, ou turnaround menos duração;
- **tempo médio de resposta**: primeira execução menos criação;
- **trocas de contexto** entre processos reais consecutivos diferentes;
- `Quantum` e `Aging` nos algoritmos Round-Robin;
- uma tabela temporal com uma linha por segundo.

Legenda da tabela:

```text
## = executando
-- = pronto e esperando
.. = ainda não chegou
OK = finalizado
IDLE = CPU ociosa
```

Trecho real obtido com os arquivos de exemplo, `--algorithm srtf` e `--seed 0`:

```text
Seed: 0

=== Shortest Remaining Time First (SRTF) ===
Tempo médio de vida (turnaround, tt): 6.75
Tempo médio de espera (waiting, tw): 3.25
Tempo médio de resposta: 3.25
Trocas de contexto: 3
```

### JSON

Use `--json` para escrever exclusivamente um documento JSON em `stdout`:

```bash
./scheduler ../examples/config.txt --algorithm all --seed 0 --json < ../examples/processes.txt
```

O documento contém `schemaVersion: 1`, `seed` como string decimal (para não
perder precisão de `uint64_t` em JavaScript), `config` e um array
`algorithms`. Cada algoritmo inclui `id`, `name`, métricas, processos finais e
uma timeline por segundo. CPU ociosa é representada por `"processId": null`,
não pelo identificador interno zero. O JSON é adequado para consumo futuro por
um servidor ou interface, que continuam fora deste projeto.

## Testes

Com `make`:

```bash
cd simulator
make test
```

Alternativa GCC direta, equivalente ao alvo de testes (exclui apenas
`src/main.c`):

```bash
gcc -Iinclude -std=c17 -Wall -Wextra -Wpedantic \
  $(find src -name '*.c' ! -name main.c) tests/*.c -o scheduler_tests
./scheduler_tests
```

No ambiente avaliado, a suíte cobre parsing, CLI, motor temporal, estruturas
dinâmicas, PRNG, desempates, os sete algoritmos, métricas, saída e integração
da aplicação, inclusive o schema JSON e o modo transacional. O alvo `make sanitize` tenta AddressSanitizer e
UndefinedBehaviorSanitizer; sua disponibilidade depende do toolchain instalado.

## Decisões principais

O simulador representa processos, filas e timeline com estruturas dinâmicas em
C. O motor registra exatamente um intervalo `[t, t + 1)` por segundo e os
algoritmos operam sobre cópias da lista original. O documento técnico detalha
as decisões, limites e complexidade em [docs/implementacao.md](docs/implementacao.md).
