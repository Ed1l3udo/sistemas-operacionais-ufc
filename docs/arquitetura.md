# Documento técnico do simulador de escalonamento

## 1. Objetivo e contrato

O projeto simula FCFS, SJF, SRTF, prioridade não preemptiva, prioridade preemptiva,
Round-Robin e Round-Robin com prioridade e envelhecimento. Há uma única fonte para
as decisões: o executável C. Tanto a entrega textual quanto o site partem do mesmo
resultado de simulação.

A execução avança em unidades inteiras de um segundo. Em cada instante `t`, o motor
primeiro admite todos os processos cuja chegada é menor ou igual a `t`, decide quem
ocupará a CPU e registra a execução no intervalo `[t, t+1)`. Um valor `-1` na
timeline representa CPU ociosa.

## 2. Módulos e estruturas

O cabeçalho `include/scheduler.h` é o contrato entre os módulos:

- `ProcessSpec`: ID, chegada, duração e prioridade estática, imutáveis durante a
  simulação;
- `SchedulerConfig`: quantum, taxa de envelhecimento e semente pseudoaleatória;
- `ProcessMetrics`: primeiro despacho, conclusão, turnaround, espera e resposta;
- `SimulationResult`: algoritmo, métricas individuais, timeline, médias e trocas de
  contexto.

`src/input.c` valida a configuração e as linhas de processos com detecção de
overflow. `src/scheduler.c` mantém os estados internos `NEW`, `READY`, `RUNNING` e
`DONE`, executa os algoritmos e calcula métricas. `src/output.c` contém apenas as
representações textual e JSON. `src/main.c` interpreta as opções, coordena os
módulos e garante que erros não sejam escritos em `stdout`.

Cada chamada de `simulate` cria uma cópia de estado (`Runtime`) para todos os
processos. Assim, a execução de um algoritmo não pode contaminar a seguinte e todos
partem exatamente da mesma carga.

## 3. Regras dos algoritmos

### FCFS

Seleciona a menor chegada disponível e mantém o processo até a conclusão. Entre
processos com a mesma chegada, aplica o desempate comum descrito na seção 4.

### SJF não preemptivo

Entre os processos prontos, seleciona aquele com menor duração restante. Uma vez
despachado, permanece na CPU até terminar.

### SRTF

Repete a seleção do menor tempo restante a cada segundo. Uma chegada com tempo
restante estritamente menor interrompe a execução no instante em que é admitida.

### Prioridade não preemptiva

Seleciona a menor prioridade numérica disponível e executa até a conclusão.

### Prioridade preemptiva

Refaz a seleção a cada segundo. Um processo recém-chegado com prioridade numérica
menor pode ocupar a CPU imediatamente; prioridades iguais usam o desempate comum.

### Round-Robin

Usa uma fila circular FIFO. Um processo executa até concluir ou consumir o quantum.
No limite de um quantum, chegadas daquele instante são enfileiradas antes do
processo expirado. Isso impede que a rotação seja anulada quando há concorrentes.

### Round-Robin com prioridade e envelhecimento

O processo escolhido mantém a CPU até terminar ou esgotar seu quantum: uma chegada
mais prioritária não causa preempção no meio da fatia. Entre os prontos, vence a
menor prioridade efetiva; empates mantêm a ordem FIFO de entrada na fila de prontos.

Para um processo pronto, a prioridade efetiva é:

```text
max(1, prioridade_estática - aging × floor(tempo_pronto / quantum))
```

Portanto, a melhora ocorre somente depois de períodos completos de espera com o
tamanho do quantum. No despacho, o contador de espera para envelhecimento é zerado
e a prioridade volta ao valor estático. O tempo de espera usado nas métricas não é
zerado: ele é derivado do turnaround e continua representando toda a execução.

## 4. Desempate e reprodutibilidade

Nos algoritmos baseados em seleção, depois do critério principal, a ordem é:

1. manter o processo que já ocupa a CPU;
2. escolher o menor tempo restante;
3. sortear entre os candidatos restantes.

O sorteio usa um gerador `xorshift32` inicializado pela semente da configuração. O
mesmo conjunto de processos, algoritmo e semente sempre produz a mesma timeline.
Cada algoritmo reinicia o gerador, evitando que selecionar mais algoritmos na CLI
altere o resultado individual. Nos dois Round-Robin, FIFO substitui esse desempate,
como exige a semântica da rotação.

## 5. Métricas

Para cada processo:

```text
turnaround = conclusão - chegada
espera     = turnaround - duração
resposta   = primeiro_despacho - chegada
```

As médias são a soma da métrica dividida pelo número de processos. Uma troca de
contexto é contada somente quando duas entradas consecutivas da timeline contêm
processos diferentes. O primeiro despacho, intervalos ociosos e a retomada depois
de ociosidade não são contabilizados.

A saída textual inclui uma tabela vertical por segundo. `##` representa execução,
`--` representa um processo que já chegou e ainda não concluiu, e a célula vazia
representa o período anterior à chegada ou posterior à conclusão.

## 6. Comunicação entre C e Node.js

`web/server.mjs` usa apenas módulos nativos do Node.js e escuta explicitamente em
`127.0.0.1`. `POST /api/simulate` recebe processos, quantum, aging, semente e a lista
de algoritmos. O servidor:

1. valida tipo, limites e tamanho máximo de 64 KiB;
2. cria um diretório temporário privado e escreve a configuração;
3. inicia `build/scheduler` com `spawn`, argumentos separados e `shell: false`;
4. envia os processos pelo `stdin` do filho;
5. limita execução a cinco segundos e saída a 12 MiB;
6. interpreta o JSON, filtra os algoritmos solicitados e remove o temporário em um
   bloco `finally`.

Erros têm a forma `{"error":{"code":"...","message":"..."}}` e status HTTP
compatível. O servidor também aplica CSP, `nosniff` e política de referrer aos
arquivos estáticos.

## 7. Interface

`web/index.html` usa elementos semânticos, rótulos e regiões vivas. `web/styles.css`
fornece layout responsivo e rolagem horizontal para tabelas e timeline.
`web/app.js` mantém como estado a última lista válida de processos: editar uma
entrada textual inválida exibe o erro, mas não modifica a tabela.

Depois da simulação, a comparação permite escolher um algoritmo por mouse ou
teclado. O detalhe mostra a timeline, métricas individuais e o estado atual. O
playback possui reinício, passo anterior, reprodução/pausa, passo seguinte,
scrubber e três velocidades. A única animação temporal da aplicação ocorre nesses
controles.

## 8. Testes e limites

Os testes C cobrem processo único, ociosidade, chegadas desordenadas e simultâneas,
preempção, término anterior ao quantum, múltiplos quanta, envelhecimento, métricas e
reprodutibilidade. O script da CLI cobre JSON/texto e entradas ou configurações
inválidas. Os testes Node iniciam o servidor numa porta efêmera e confirmam que a
resposta da API é igual ao JSON produzido diretamente pelo executável.

Para limitar uso acidental de memória, uma simulação aceita até 10.000 processos e
uma timeline de até 10.000.000 de segundos. Na API, os limites adicionais de corpo,
tempo e saída protegem o servidor local.

