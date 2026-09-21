# Guia de desenvolvimento

## Fluxo Git obrigatório

- A branch de integração é `main`.
- Toda feature, correção ou refatoração deve nascer em uma branch própria, criada a
  partir da `main` atualizada. Use os prefixos `feat/`, `fix/`, `refactor/`,
  `test/` ou `docs/`.
- Faça commits pequenos e autocontidos, com mensagens no imperativo e no padrão
  Conventional Commits (`feat:`, `fix:`, `test:`, `docs:`, `refactor:`).
- Antes do merge, execute os testes relacionados e registre no commit apenas os
  arquivos da tarefa em curso.
- O merge para `main` pode ser feito autonomamente, sem pedir autorização. Prefira
  `git merge --no-ff` para preservar a fronteira da feature.
- Nunca reescreva, descarte ou inclua alterações preexistentes do usuário sem
  autorização explícita.

## Decisões do projeto

- O motor de simulação e todos os algoritmos são implementados em C17. A interface
  web não replica nenhuma regra de escalonamento.
- A CLI é o contrato central: lê processos de `stdin`, configuração de arquivo e
  produz texto acadêmico ou JSON em `stdout`; diagnósticos vão para `stderr`.
- A simulação é discreta, com uma decisão por segundo. Chegadas são admitidas antes
  da decisão do instante correspondente.
- IDs (`P1`, `P2`, ...) seguem a ordem original da entrada, mesmo que as chegadas
  estejam fora de ordem.
- Prioridade numérica menor significa prioridade mais alta.
- Seleções empatadas preferem: processo já na CPU, menor tempo restante e sorteio
  pseudoaleatório reproduzível. Round-Robin preserva sua rotação FIFO e não aplica
  a preferência pelo processo atual.
- Troca de contexto só é contada entre dois segundos consecutivos ocupados por
  processos diferentes. Despacho inicial, ociosidade e retomada após ociosidade não
  contam.
- No Round-Robin, chegadas do instante do fim do quantum entram na fila antes do
  processo expirado.
- No Round-Robin com prioridade, não há preempção no meio do quantum. O processo
  pronto melhora a prioridade efetiva por quantum completo de espera, limitada a
  1, e seu envelhecimento reinicia ao voltar à CPU. Empates usam FIFO.
- O servidor usa somente módulos nativos do Node.js, escuta em `127.0.0.1`, chama o
  binário com `spawn` sem shell, limita corpo/tempo e sempre remove temporários.
- A interface usa HTML semântico, CSS responsivo e módulos JavaScript sem framework.

## Organização prevista

- `src/` e `include/`: CLI, parser, motor, algoritmos e serialização.
- `tests/`: testes unitários/integrados e casos calculados manualmente.
- `web/`: servidor local e arquivos estáticos da interface.
- `examples/`: entrada e configuração de demonstração.
- `docs/`: documento técnico em português.

## Backlog de implementação

- [ ] Estruturar build C17, tipos comuns, parser e validações.
- [ ] Implementar os sete algoritmos, métricas e timelines.
- [ ] Implementar saída textual e JSON e cobrir a CLI com testes.
- [ ] Criar API Node.js segura que execute o mesmo binário.
- [ ] Criar interface web responsiva, comparação e playback.
- [ ] Completar testes unitários, integração CLI/API e sanitizadores.
- [ ] Documentar arquitetura, decisões, execução e exemplos.

## Critérios de conclusão

- `make`, `make test` e `make sanitize` passam.
- `make web` inicia o site local usando o binário compilado.
- Saídas JSON da CLI e da API representam os mesmos resultados.
- Os sete algoritmos exibem métricas individuais/globais e timeline por segundo.
- Entradas inválidas terminam com código não zero e mensagem clara, sem contaminar
  `stdout`.
