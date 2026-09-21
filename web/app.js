const initialProcesses = [
  { arrival: 0, burst: 5, priority: 2 },
  { arrival: 0, burst: 2, priority: 3 },
  { arrival: 1, burst: 4, priority: 1 },
  { arrival: 3, burst: 3, priority: 4 },
];

const state = {
  processes: structuredClone(initialProcesses),
  data: null,
  selectedAlgorithm: null,
  second: 0,
  timer: null,
};

const elements = {
  form: document.querySelector('#simulation-form'),
  tableBody: document.querySelector('#process-table-body'),
  processText: document.querySelector('#process-text'),
  processError: document.querySelector('#process-error'),
  addProcess: document.querySelector('#add-process'),
  status: document.querySelector('#form-status'),
  submit: document.querySelector('#simulate'),
  results: document.querySelector('#results'),
  comparison: document.querySelector('#comparison-body'),
  detailTitle: document.querySelector('#detail-title'),
  timeline: document.querySelector('#timeline'),
  timelineWrap: document.querySelector('#timeline-wrap'),
  metrics: document.querySelector('#metrics-body'),
  currentTime: document.querySelector('#current-time'),
  currentProcess: document.querySelector('#current-process'),
  reset: document.querySelector('#reset'),
  previous: document.querySelector('#previous'),
  play: document.querySelector('#play'),
  next: document.querySelector('#next'),
  speed: document.querySelector('#speed'),
  scrubber: document.querySelector('#scrubber'),
};

function processText(processes) {
  return processes.map(({ arrival, burst, priority }) => `${arrival} ${burst} ${priority}`).join('\n');
}

function parseProcessText(text) {
  const processes = [];
  const lines = text.split(/\r?\n/);
  for (let index = 0; index < lines.length; index += 1) {
    const content = lines[index].trim();
    if (!content || content.startsWith('#')) continue;
    const values = content.split(/\s+/);
    if (values.length !== 3 || values.some((value) => !/^-?\d+$/.test(value))) {
      throw new Error(`Linha ${index + 1}: informe exatamente três números inteiros.`);
    }
    const [arrival, burst, priority] = values.map(Number);
    if (!Number.isSafeInteger(arrival) || !Number.isSafeInteger(burst) || !Number.isSafeInteger(priority)) {
      throw new Error(`Linha ${index + 1}: há um número fora do intervalo permitido.`);
    }
    if (arrival < 0 || burst <= 0 || priority <= 0) {
      throw new Error(`Linha ${index + 1}: chegada ≥ 0, duração > 0 e prioridade > 0.`);
    }
    processes.push({ arrival, burst, priority });
  }
  if (processes.length === 0) throw new Error('Informe ao menos um processo.');
  if (processes.length > 10000) throw new Error('O limite é de 10.000 processos.');
  return processes;
}

function setProcessError(message = '') {
  elements.processError.textContent = message;
  elements.processText.setAttribute('aria-invalid', message ? 'true' : 'false');
}

function renderProcessTable() {
  elements.tableBody.innerHTML = state.processes.map((process, index) => `
    <tr>
      <td>P${index + 1}</td>
      <td><input type="number" min="0" step="1" value="${process.arrival}" data-index="${index}" data-field="arrival" aria-label="Chegada de P${index + 1}"></td>
      <td><input type="number" min="1" step="1" value="${process.burst}" data-index="${index}" data-field="burst" aria-label="Duração de P${index + 1}"></td>
      <td><input type="number" min="1" step="1" value="${process.priority}" data-index="${index}" data-field="priority" aria-label="Prioridade de P${index + 1}"></td>
      <td><button class="remove-process" type="button" data-remove="${index}" aria-label="Remover P${index + 1}" ${state.processes.length === 1 ? 'disabled' : ''}>×</button></td>
    </tr>
  `).join('');
}

function readProcessTable() {
  const processes = structuredClone(state.processes);
  const inputs = elements.tableBody.querySelectorAll('input[data-field]');
  for (const input of inputs) {
    const value = Number(input.value);
    const minimum = input.dataset.field === 'arrival' ? 0 : 1;
    if (!Number.isInteger(value) || value < minimum) {
      throw new Error(`${input.getAttribute('aria-label')}: valor inteiro mínimo ${minimum}.`);
    }
    processes[Number(input.dataset.index)][input.dataset.field] = value;
  }
  return processes;
}

elements.processText.addEventListener('input', () => {
  try {
    const parsed = parseProcessText(elements.processText.value);
    state.processes = parsed;
    renderProcessTable();
    setProcessError();
  } catch (error) {
    setProcessError(error.message);
  }
});

elements.tableBody.addEventListener('input', () => {
  try {
    state.processes = readProcessTable();
    elements.processText.value = processText(state.processes);
    setProcessError();
  } catch (error) {
    setProcessError(error.message);
  }
});

elements.tableBody.addEventListener('click', (event) => {
  const button = event.target.closest('[data-remove]');
  if (!button || state.processes.length === 1) return;
  state.processes.splice(Number(button.dataset.remove), 1);
  elements.processText.value = processText(state.processes);
  setProcessError();
  renderProcessTable();
});

elements.addProcess.addEventListener('click', () => {
  const last = state.processes.at(-1);
  state.processes.push({ arrival: last ? last.arrival : 0, burst: 1, priority: 1 });
  elements.processText.value = processText(state.processes);
  setProcessError();
  renderProcessTable();
  elements.tableBody.querySelector('tr:last-child input')?.focus();
});

function selectedAlgorithms() {
  return [...document.querySelectorAll('#algorithm-options input:checked')].map((input) => input.value);
}

function setStatus(message, isError = false) {
  elements.status.textContent = message;
  elements.status.classList.toggle('error', isError);
}

function numberFromInput(selector, name, minimum, maximum = Number.MAX_SAFE_INTEGER) {
  const input = document.querySelector(selector);
  const value = Number(input.value);
  if (!Number.isInteger(value) || value < minimum || value > maximum) {
    input.focus();
    throw new Error(`${name} deve ser um inteiro entre ${minimum} e ${maximum}.`);
  }
  return value;
}

elements.form.addEventListener('submit', async (event) => {
  event.preventDefault();
  stopPlayback();
  try {
    const processes = parseProcessText(elements.processText.value);
    const algorithms = selectedAlgorithms();
    if (algorithms.length === 0) throw new Error('Selecione ao menos um algoritmo.');
    const payload = {
      processes: processText(processes),
      quantum: numberFromInput('#quantum', 'Quantum', 1, 1000000),
      aging: numberFromInput('#aging', 'Aging', 0, 1000000),
      seed: numberFromInput('#seed', 'Semente', 0, 0xffffffff),
      algorithms,
    };
    elements.submit.disabled = true;
    setStatus('Executando o motor C…');
    const response = await fetch('/api/simulate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
    const body = await response.json().catch(() => null);
    if (!response.ok) throw new Error(body?.error?.message || 'Não foi possível concluir a simulação.');
    state.processes = processes;
    state.data = body;
    state.selectedAlgorithm = body.results[0]?.algorithm ?? null;
    state.second = 0;
    renderResults();
    setStatus(`${body.results.length} algoritmo(s) calculado(s) pelo motor C.`);
    elements.results.hidden = false;
    elements.results.scrollIntoView({ behavior: 'smooth', block: 'start' });
  } catch (error) {
    setStatus(error.message, true);
  } finally {
    elements.submit.disabled = false;
  }
});

function formatAverage(value) {
  return `${value.toFixed(2)} s`;
}

function renderResults() {
  elements.comparison.innerHTML = state.data.results.map((result) => `
    <tr role="button" tabindex="0" data-algorithm="${result.algorithm}" class="${result.algorithm === state.selectedAlgorithm ? 'selected' : ''}" aria-pressed="${result.algorithm === state.selectedAlgorithm}">
      <td class="algorithm-name">${result.label}</td>
      <td>${formatAverage(result.averageTurnaround)}</td>
      <td>${formatAverage(result.averageWaiting)}</td>
      <td>${formatAverage(result.averageResponse)}</td>
      <td>${result.contextSwitches}</td>
    </tr>
  `).join('');
  renderDetail();
}

function selectComparisonRow(row) {
  if (!row) return;
  stopPlayback();
  state.selectedAlgorithm = row.dataset.algorithm;
  state.second = 0;
  for (const candidate of elements.comparison.querySelectorAll('tr')) {
    const selected = candidate === row;
    candidate.classList.toggle('selected', selected);
    candidate.setAttribute('aria-pressed', String(selected));
  }
  renderDetail();
}

elements.comparison.addEventListener('click', (event) => selectComparisonRow(event.target.closest('[data-algorithm]')));
elements.comparison.addEventListener('keydown', (event) => {
  if (event.key === 'Enter' || event.key === ' ') {
    event.preventDefault();
    selectComparisonRow(event.target.closest('[data-algorithm]'));
  }
});

function selectedResult() {
  return state.data?.results.find((result) => result.algorithm === state.selectedAlgorithm);
}

function renderDetail() {
  const result = selectedResult();
  if (!result) return;
  elements.detailTitle.textContent = result.label;
  elements.timeline.innerHTML = `
    <thead><tr>${result.timeline.map((entry) => `<th scope="col">${entry.time}–${entry.time + 1}</th>`).join('')}</tr></thead>
    <tbody><tr>${result.timeline.map((entry) => `<td data-time="${entry.time}" class="${entry.process ? '' : 'idle'}">${entry.process ?? 'ociosa'}</td>`).join('')}</tr></tbody>
  `;
  elements.metrics.innerHTML = result.metrics.map((metric) => `
    <tr data-process="${metric.id}">
      <td><strong>${metric.id}</strong></td>
      <td>${metric.firstDispatch} s</td>
      <td>${metric.completion} s</td>
      <td>${metric.turnaround} s</td>
      <td>${metric.waiting} s</td>
      <td>${metric.response} s</td>
    </tr>
  `).join('');
  elements.scrubber.max = String(Math.max(0, result.timeline.length - 1));
  elements.scrubber.value = '0';
  updatePlayback(false);
}

function updatePlayback(scroll = true) {
  const result = selectedResult();
  if (!result?.timeline.length) return;
  state.second = Math.max(0, Math.min(state.second, result.timeline.length - 1));
  const entry = result.timeline[state.second];
  elements.currentTime.textContent = `t = ${entry.time} s`;
  elements.currentProcess.textContent = entry.process ? `${entry.process} em execução` : 'CPU ociosa';
  elements.scrubber.value = String(state.second);
  for (const cell of elements.timeline.querySelectorAll('[data-time]')) {
    cell.classList.toggle('current', Number(cell.dataset.time) === state.second);
  }
  for (const row of elements.metrics.querySelectorAll('[data-process]')) {
    const metric = result.metrics.find((item) => item.id === row.dataset.process);
    row.classList.toggle('active', row.dataset.process === entry.process);
    row.classList.toggle('done', state.second >= metric.completion);
    row.classList.toggle(
      'waiting',
      state.second >= metric.arrival && state.second < metric.completion && row.dataset.process !== entry.process,
    );
  }
  if (scroll) {
    elements.timeline.querySelector(`[data-time="${state.second}"]`)?.scrollIntoView({
      behavior: 'smooth', block: 'nearest', inline: 'center',
    });
  }
}

function stopPlayback() {
  if (state.timer) clearInterval(state.timer);
  state.timer = null;
  elements.play.textContent = '▶';
  elements.play.setAttribute('aria-label', 'Reproduzir');
}

function startPlayback() {
  const result = selectedResult();
  if (!result) return;
  if (state.second >= result.timeline.length - 1) state.second = 0;
  stopPlayback();
  elements.play.textContent = 'Ⅱ';
  elements.play.setAttribute('aria-label', 'Pausar');
  state.timer = setInterval(() => {
    if (state.second >= result.timeline.length - 1) {
      stopPlayback();
      return;
    }
    state.second += 1;
    updatePlayback();
  }, Number(elements.speed.value));
  updatePlayback();
}

elements.play.addEventListener('click', () => (state.timer ? stopPlayback() : startPlayback()));
elements.reset.addEventListener('click', () => { stopPlayback(); state.second = 0; updatePlayback(); });
elements.previous.addEventListener('click', () => { stopPlayback(); state.second -= 1; updatePlayback(); });
elements.next.addEventListener('click', () => { stopPlayback(); state.second += 1; updatePlayback(); });
elements.scrubber.addEventListener('input', () => { stopPlayback(); state.second = Number(elements.scrubber.value); updatePlayback(); });
elements.speed.addEventListener('change', () => { if (state.timer) startPlayback(); });

renderProcessTable();
