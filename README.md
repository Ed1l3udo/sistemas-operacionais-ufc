# Process Lab — Simulador de Escalonamento

Simulador acadêmico dos sete algoritmos pedidos na atividade de Sistemas
Operacionais. O motor, as decisões de escalonamento e as métricas são implementados
em C17; a interface web apenas envia a entrada ao mesmo executável e apresenta seu
JSON.

## Requisitos

- GCC com suporte a C17;
- GNU Make;
- Node.js 18 ou mais recente para a interface e os testes da API.

Não há dependências externas para instalar.

## Uso rápido

Compile e execute todos os algoritmos com o exemplo do enunciado:

```sh
make
./build/scheduler \
  --config examples/config.txt \
  --algorithm all \
  --format text \
  --seed 42 < examples/processes.txt
```

Para receber o contrato usado pela interface web:

```sh
./build/scheduler \
  --config examples/config.txt \
  --algorithm all \
  --format json \
  --seed 42 < examples/processes.txt
```

Os valores aceitos por `--algorithm` são `all`, `fcfs`, `sjf`, `srtf`,
`priority-np`, `priority-p`, `rr` e `priority-rr`. O formato pode ser `text` ou
`json`. A semente é opcional e vale `42` por padrão.

Cada linha de `stdin` contém três inteiros:

```text
chegada duração prioridade
```

A chegada deve ser não negativa; duração e prioridade devem ser positivas. Os IDs
seguem a ordem das linhas, mesmo quando as chegadas estão fora de ordem. O arquivo
de configuração tem este formato:

```text
quantum:2
aging:1
```

Resultados são escritos somente em `stdout`. Diagnósticos são escritos em
`stderr`, e qualquer entrada inválida produz código de saída diferente de zero.

## Interface web

```sh
make web
```

Acesse <http://127.0.0.1:3000>. Para escolher outra porta:

```sh
PORT=8080 make web
```

A tela mantém a tabela editável sincronizada com a entrada textual, compara os
algoritmos selecionados e oferece controles para reproduzir, pausar, avançar,
retroceder e reiniciar a timeline. Texto inválido não substitui a última tabela
válida.

## Testes

```sh
make test
make sanitize
```

`make test` executa timelines calculadas manualmente, validações da CLI e testes da
API que comparam sua resposta com o JSON direto do binário C. O teste HTTP abre
somente uma porta efêmera em `127.0.0.1`.

`make sanitize` recompila os testes com AddressSanitizer e
UndefinedBehaviorSanitizer. A detecção de leaks do ASan fica desabilitada para
compatibilidade com ambientes executados sob `ptrace`.

## Estrutura

```text
include/      contrato público do motor C
src/          parser, algoritmos, CLI e serialização
tests/        testes C, CLI e API
web/          servidor Node.js e interface estática
examples/     entrada e configuração do enunciado
docs/         documentação técnica
```

As decisões de implementação e regras detalhadas estão em
[`docs/arquitetura.md`](docs/arquitetura.md). O fluxo de contribuição do repositório
está em [`AGENTS.md`](AGENTS.md).

