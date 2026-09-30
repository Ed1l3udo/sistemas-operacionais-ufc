# Projeto dos indicadores de decisão do escalonador

Status: proposta de interface. Nenhum indicador ou contrato de rastreamento está
implementado nesta etapa.

## Objetivo e posição na tela

Ao percorrer o instante `t`, mostrar por que o processo exibido na timeline ocupa
a CPU em `[t, t+1)`. O painel de decisão ficará no detalhe do algoritmo
selecionado, entre os controles de playback e a timeline. Assim, comparação,
métricas e controles existentes continuam no mesmo lugar.

O painel terá uma estrutura comum: instante, chegadas de `t`, processo em execução,
processos prontos, critério decisivo e uma frase curta de explicação. A área central
mudará de acordo com a política. Cada cartão terá ID e valor numérico legíveis sem
depender de cor. O mesmo segundo selecionado pelo scrubber, pelos botões ou pela
reprodução determina todo o conteúdo; pausar e retroceder devem recuperar a mesma
fotografia da decisão.

| Política | Indicador exclusivo | Leitura esperada no instante `t` |
| --- | --- | --- |
| FCFS | Fila de chegadas | Cartões entram pela chegada e destacam a menor chegada elegível. A CPU mantém o processo em curso até ele terminar. |
| SJF | Comparador de trabalhos curtos | Barras dos tempos restantes dos prontos, com o menor valor marcado quando a CPU fica livre. |
| SRTF | Disputa de tempo restante | Barras dos prontos **e** do ocupante da CPU, atualizadas a cada segundo; uma seta mostra uma preempção. |
| Prioridade não preemptiva | Escada de prioridades | Cartões por prioridade numérica crescente; a escolha ocorre apenas quando a CPU está livre. |
| Prioridade preemptiva | Disputa de prioridades | Comparação entre CPU e prontos a cada segundo; destaca quem entrou com prioridade numericamente menor e provocou troca. |
| Round-Robin | Fila circular e relógio do quantum | Posição dos prontos, fatia usada/restante e retorno do processo ao fim da fila quando a fatia expira. |
| Round-Robin com prioridade e envelhecimento | Fila por prioridade efetiva e medidor de espera | Prioridade estática → efetiva, progresso dos quanta completos de espera e FIFO entre empates, sem interromper a fatia atual. |

## Comportamento específico

### FCFS — fila de chegadas

Mostrar o cartão da CPU separado dos cartões que aguardam. Cada cartão que chega
exibe `P# · chegada t`; os candidatos com a menor chegada ficam destacados.
Chegadas no mesmo instante formam um grupo de empate: o indicador deve mostrar o
desempate real (menor tempo restante e, se necessário, sorteio reproduzível), sem
prometer uma ordem FIFO estrita dentro desse grupo. Exemplo de explicação:
“P2 tem a menor chegada entre os prontos; P1 continua até concluir”. Quando ninguém
está pronto, mostrar “CPU ociosa · próxima chegada em t=…”.

### SJF — comparador de trabalhos curtos

No ponto de despacho, ordenar visualmente os prontos pelo tempo restante, com
barras de escala comum e os valores em segundos. Destacar o escolhido e o eventual
desempate. Enquanto ele executa, manter o cartão da CPU com um marcador “mantém
até concluir”; a chegada de um trabalho mais curto aparece entre os prontos, sem
sugerir preempção. O valor de quem está na CPU diminui no playback, mas a seleção
não é refeita no meio do trabalho.

### SRTF — disputa de tempo restante

Mostrar lado a lado o tempo restante da CPU antes da decisão e o dos processos
prontos após admitir as chegadas de `t`. Atualizar os números a cada segundo.
Quando outro processo vence, destacar “preempção em t” e a comparação numérica;
em empate no critério principal, mostrar que o ocupante atual é mantido. O cartão
despachado sempre corresponde ao processo da timeline em `t`.

### Prioridade não preemptiva — escada de prioridades

Exibir os prontos em níveis, da menor prioridade numérica (mais alta) para a
maior, com o valor explícito em cada cartão. Quando a CPU está livre, destacar o
nível vencedor e o desempate aplicado. Durante a execução, o cartão da CPU fica
“fixo até concluir”; uma nova chegada de prioridade mais alta é exibida na espera,
sem seta de preempção.

### Prioridade preemptiva — disputa de prioridades

Comparar a prioridade estática da CPU e a dos prontos após as chegadas de `t`.
Destacar o menor número. Se houver troca, ligar o cartão vencedor à CPU e mostrar
“P# substitui P# em t”; se as prioridades empatarem, exibir a preferência pelo
ocupante atual antes dos demais critérios de desempate. O indicador refaz essa
leitura a cada segundo.

### Round-Robin — fila circular e relógio do quantum

Mostrar a ordem FIFO dos prontos, a CPU e uma régua `usado/quantum`. Ao terminar
a fatia sem concluir, o cartão executado retorna ao fim da fila; ao concluir, sai
da visualização ativa. No limite da fatia, as chegadas daquele instante entram
antes do processo expirado. Isso precisa ser visível mesmo quando a CPU recebe o
mesmo processo novamente porque não há concorrentes. Não aplicar o desempate dos
algoritmos de seleção à fila.

### Round-Robin com prioridade e envelhecimento — fila e medidor

Cada pronto mostra `prioridade estática → efetiva`, o tempo de espera desde a
chegada ou o retorno à fila e quantos quanta completos já produziram envelhecimento. Mostrar
os cartões ordenados por prioridade efetiva, usando a ordem FIFO de entrada na
fila de prontos para empates. A régua do quantum da CPU permanece travada até a
fatia terminar ou o processo concluir, mesmo que um pronto alcance prioridade
efetiva melhor. No próximo despacho do processo, o medidor zera e a prioridade
volta ao valor estático. A prioridade efetiva nunca cai abaixo de 1.

## Contrato necessário para uma futura implementação

O JSON atual informa processos, configuração, métricas e apenas o processo que
executou em cada segundo. Ele não informa toda a fila nem a razão do desempate.
Os estados de decisão deverão vir do motor em C; o JavaScript só apresentará os
valores recebidos. A fotografia de `t` representa, nesta ordem: admitir chegadas,
recolocar na fila um quantum expirado no instante anterior, escolher ou manter a
CPU, executar `[t, t+1)` e atualizar espera/conclusão. O rastreamento proposto
deve fornecer, para o algoritmo e o instante solicitados:

- `time`, `arrivals` e `cpuBefore`: estado imediatamente **antes** de escolher,
  depois de admitir chegadas e recolocar o processo cujo quantum expirou;
- `ready`: IDs em ordem relevante para a política, com tempo restante,
  prioridade estática/efetiva, tempo de espera para aging e posição FIFO quando
  aplicáveis;
- `selected`, `criterion` e `tieBreak`: decisão efetiva do C, incluindo a
  preferência pela CPU, menor tempo restante ou sorteio quando usados;
- `quantumUsed` e `quantumLimit` quando houver fatia, com convenção explícita
  sobre valores antes da execução de `[t, t+1)`;
- `eventsBefore` e `eventsAfter`: chegada/retorno à fila antes da escolha e
  despacho, continuidade, preempção, expiração, conclusão ou ociosidade conforme
  a fase; pode haver mais de um evento no mesmo instante.

O resultado deve ser determinístico ao voltar no tempo: não se deve inferir a fila
pela animação anterior nem consumir um novo sorteio ao usar o scrubber. Para não
multiplicar o JSON de `--algorithm all` por todas as decisões de todos os processos,
o rastreamento deve ser opcional e limitado ao algoritmo/intervalo de instantes
em exibição. A forma exata da extensão CLI/API fica para a implementação; o
contrato atual permanece válido.

## Regras de apresentação e aceite futuro

- Um único destaque identifica quem recebeu a CPU em `t`; valores e frase curta
  explicam a escolha. A timeline continua sendo a referência temporal.
- Mudanças de cartão acompanham os controles atuais. Animações curtas só ilustram
  a transição; `prefers-reduced-motion` permite vê-la sem movimento.
- Em telas estreitas, cartões passam a uma lista rolável com ordem e números
  preservados. Empate, CPU ociosa e processo único têm textos próprios.
- Com muitos processos, mostrar os candidatos relevantes e uma contagem dos
  demais, com expansão sob demanda; não criar milhares de elementos por segundo.
- Testar cada indicador com uma carga que evidencie sua regra: chegadas
  simultâneas no FCFS; trabalho curto chegando durante SJF e SRTF; chegada mais
  prioritária durante as duas políticas de prioridade; expiração com chegada no
  Round-Robin; e promoção por aging sem preempção no meio da fatia.
- A seleção exibida deve coincidir com `timeline[t].process` e com a decisão do C
  em todos os testes; métricas e resultados já existentes não mudam.
