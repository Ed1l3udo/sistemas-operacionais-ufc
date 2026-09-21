import { spawn } from 'node:child_process';
import { createReadStream } from 'node:fs';
import { access, mkdtemp, rm, writeFile } from 'node:fs/promises';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const SERVER_FILE = fileURLToPath(import.meta.url);
const WEB_DIR = path.dirname(SERVER_FILE);
const PROJECT_DIR = path.dirname(WEB_DIR);
const DEFAULT_BINARY = path.join(PROJECT_DIR, 'build', 'scheduler');
const HOST = '127.0.0.1';
const BODY_LIMIT = 64 * 1024;
const OUTPUT_LIMIT = 12 * 1024 * 1024;
const TIMEOUT_MS = 5000;
const ALGORITHMS = new Set([
  'fcfs', 'sjf', 'srtf', 'priority-np', 'priority-p', 'rr', 'priority-rr',
]);
const STATIC_ROUTES = new Map([
  ['/', ['index.html', 'text/html; charset=utf-8']],
  ['/index.html', ['index.html', 'text/html; charset=utf-8']],
  ['/styles.css', ['styles.css', 'text/css; charset=utf-8']],
  ['/app.js', ['app.js', 'text/javascript; charset=utf-8']],
]);

class ApiError extends Error {
  constructor(status, code, message, details) {
    super(message);
    this.status = status;
    this.code = code;
    this.details = details;
  }
}

function applyHeaders(response) {
  response.setHeader('X-Content-Type-Options', 'nosniff');
  response.setHeader('Referrer-Policy', 'no-referrer');
  response.setHeader(
    'Content-Security-Policy',
    "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'",
  );
}

function sendJson(response, status, payload) {
  const body = JSON.stringify(payload);
  applyHeaders(response);
  response.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(body),
  });
  response.end(body);
}

async function readJson(request) {
  if (!String(request.headers['content-type'] || '').toLowerCase().startsWith('application/json')) {
    throw new ApiError(415, 'UNSUPPORTED_MEDIA_TYPE', 'Use Content-Type: application/json.');
  }
  let size = 0;
  const chunks = [];
  for await (const chunk of request) {
    size += chunk.length;
    if (size > BODY_LIMIT) {
      throw new ApiError(413, 'BODY_TOO_LARGE', `A requisição excede ${BODY_LIMIT} bytes.`);
    }
    chunks.push(chunk);
  }
  try {
    return JSON.parse(Buffer.concat(chunks).toString('utf8'));
  } catch {
    throw new ApiError(400, 'INVALID_JSON', 'O corpo não contém JSON válido.');
  }
}

function integer(value, name, minimum, maximum) {
  if (!Number.isInteger(value) || value < minimum || value > maximum) {
    throw new ApiError(
      400,
      'INVALID_REQUEST',
      `${name} deve ser um inteiro entre ${minimum} e ${maximum}.`,
      { field: name },
    );
  }
  return value;
}

function validatePayload(payload) {
  if (!payload || typeof payload !== 'object' || Array.isArray(payload)) {
    throw new ApiError(400, 'INVALID_REQUEST', 'O corpo deve ser um objeto JSON.');
  }
  if (typeof payload.processes !== 'string' || payload.processes.trim() === '') {
    throw new ApiError(400, 'INVALID_REQUEST', 'processes deve conter a entrada textual.', {
      field: 'processes',
    });
  }
  if (payload.processes.includes('\0')) {
    throw new ApiError(400, 'INVALID_REQUEST', 'processes contém caractere nulo.', {
      field: 'processes',
    });
  }
  const quantum = integer(payload.quantum, 'quantum', 1, 1000000);
  const aging = integer(payload.aging, 'aging', 0, 1000000);
  const seed = integer(payload.seed, 'seed', 0, 0xffffffff);
  if (!Array.isArray(payload.algorithms) || payload.algorithms.length === 0) {
    throw new ApiError(400, 'INVALID_REQUEST', 'Selecione ao menos um algoritmo.', {
      field: 'algorithms',
    });
  }
  const algorithms = [...new Set(payload.algorithms)];
  if (algorithms.some((algorithm) => typeof algorithm !== 'string' || !ALGORITHMS.has(algorithm))) {
    throw new ApiError(400, 'INVALID_REQUEST', 'A lista contém um algoritmo desconhecido.', {
      field: 'algorithms',
    });
  }
  return { processes: payload.processes, quantum, aging, seed, algorithms };
}

function executeScheduler(binaryPath, configPath, payload) {
  return new Promise((resolve, reject) => {
    const child = spawn(binaryPath, [
      '--config', configPath,
      '--algorithm', 'all',
      '--format', 'json',
      '--seed', String(payload.seed),
    ], { shell: false, stdio: ['pipe', 'pipe', 'pipe'] });
    const stdout = [];
    const stderr = [];
    let outputSize = 0;
    let settled = false;

    const finish = (callback) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      callback();
    };
    const timer = setTimeout(() => {
      child.kill('SIGKILL');
      finish(() => reject(new ApiError(504, 'SIMULATION_TIMEOUT', 'A simulação excedeu o limite de tempo.')));
    }, TIMEOUT_MS);

    child.on('error', (error) => {
      finish(() => reject(new ApiError(
        500,
        'BINARY_UNAVAILABLE',
        'Não foi possível iniciar o simulador C.',
        { reason: error.message },
      )));
    });
    child.stdout.on('data', (chunk) => {
      outputSize += chunk.length;
      if (outputSize > OUTPUT_LIMIT) {
        child.kill('SIGKILL');
        finish(() => reject(new ApiError(413, 'OUTPUT_TOO_LARGE', 'A saída da simulação é muito grande.')));
      } else {
        stdout.push(chunk);
      }
    });
    child.stderr.on('data', (chunk) => {
      if (Buffer.concat(stderr).length < 8192) stderr.push(chunk);
    });
    child.on('close', (code) => {
      finish(() => {
        const diagnostic = Buffer.concat(stderr).toString('utf8').trim();
        if (code !== 0) {
          reject(new ApiError(422, 'SIMULATION_ERROR', diagnostic.replace(/^erro:\s*/i, '') || 'Entrada rejeitada pelo simulador.'));
          return;
        }
        try {
          const output = JSON.parse(Buffer.concat(stdout).toString('utf8'));
          output.results = output.results.filter((result) => payload.algorithms.includes(result.algorithm));
          resolve(output);
        } catch {
          reject(new ApiError(500, 'INVALID_BINARY_OUTPUT', 'O simulador produziu uma resposta inválida.'));
        }
      });
    });
    child.stdin.on('error', () => {});
    child.stdin.end(payload.processes);
  });
}

async function simulate(binaryPath, payload) {
  const temporaryDirectory = await mkdtemp(path.join(os.tmpdir(), 'scheduler-'));
  const configPath = path.join(temporaryDirectory, 'config.txt');
  try {
    await writeFile(configPath, `quantum:${payload.quantum}\naging:${payload.aging}\n`, {
      encoding: 'utf8',
      mode: 0o600,
    });
    return await executeScheduler(binaryPath, configPath, payload);
  } finally {
    await rm(temporaryDirectory, { recursive: true, force: true });
  }
}

async function serveStatic(request, response) {
  const pathname = new URL(request.url, `http://${HOST}`).pathname;
  const route = STATIC_ROUTES.get(pathname);
  if (!route || (request.method !== 'GET' && request.method !== 'HEAD')) return false;
  const [filename, contentType] = route;
  const filePath = path.join(WEB_DIR, filename);
  try {
    await access(filePath);
  } catch {
    return false;
  }
  applyHeaders(response);
  response.writeHead(200, { 'Content-Type': contentType });
  if (request.method === 'HEAD') response.end();
  else createReadStream(filePath).pipe(response);
  return true;
}

export function createAppServer({ binaryPath = DEFAULT_BINARY } = {}) {
  return http.createServer(async (request, response) => {
    try {
      const pathname = new URL(request.url, `http://${HOST}`).pathname;
      if (pathname === '/api/simulate') {
        if (request.method !== 'POST') {
          response.setHeader('Allow', 'POST');
          throw new ApiError(405, 'METHOD_NOT_ALLOWED', 'Use POST neste endpoint.');
        }
        const payload = validatePayload(await readJson(request));
        sendJson(response, 200, await simulate(binaryPath, payload));
        return;
      }
      if (await serveStatic(request, response)) return;
      sendJson(response, 404, { error: { code: 'NOT_FOUND', message: 'Recurso não encontrado.' } });
    } catch (error) {
      const apiError = error instanceof ApiError
        ? error
        : new ApiError(500, 'INTERNAL_ERROR', 'Erro interno do servidor.');
      if (!response.headersSent) {
        const payload = { error: { code: apiError.code, message: apiError.message } };
        if (apiError.details) payload.error.details = apiError.details;
        sendJson(response, apiError.status, payload);
      } else {
        response.destroy();
      }
    }
  });
}

if (process.argv[1] && path.resolve(process.argv[1]) === SERVER_FILE) {
  const port = Number.parseInt(process.env.PORT || '3000', 10);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    console.error('PORT deve ser um inteiro entre 1 e 65535.');
    process.exitCode = 1;
  } else {
    const server = createAppServer();
    server.on('error', (error) => {
      console.error(`Falha ao iniciar servidor: ${error.message}`);
      process.exitCode = 1;
    });
    server.listen(port, HOST, () => {
      console.log(`Simulador disponível em http://${HOST}:${port}`);
    });
  }
}
