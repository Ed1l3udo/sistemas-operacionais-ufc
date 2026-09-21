import assert from 'node:assert/strict';
import { mkdtemp, rm, writeFile } from 'node:fs/promises';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import test, { after, before } from 'node:test';
import { fileURLToPath } from 'node:url';

import { createAppServer } from '../web/server.mjs';

const TEST_DIR = path.dirname(fileURLToPath(import.meta.url));
const PROJECT_DIR = path.dirname(TEST_DIR);
const BINARY = path.join(PROJECT_DIR, 'build', 'scheduler');
const PROCESS_INPUT = '0 5 2\n0 2 3\n1 4 1\n3 3 4\n';
const ALL_ALGORITHMS = [
  'fcfs', 'sjf', 'srtf', 'priority-np', 'priority-p', 'rr', 'priority-rr',
];

let server;
let port;

function request(method, pathname, body, contentType = 'application/json') {
  return new Promise((resolve, reject) => {
    const serialized = typeof body === 'string' ? body : JSON.stringify(body);
    const clientRequest = http.request({
      host: '127.0.0.1', port, method, path: pathname,
      headers: {
        'Content-Type': contentType,
        'Content-Length': Buffer.byteLength(serialized),
      },
    }, (response) => {
      const chunks = [];
      response.on('data', (chunk) => chunks.push(chunk));
      response.on('end', () => {
        const text = Buffer.concat(chunks).toString('utf8');
        resolve({ status: response.statusCode, body: JSON.parse(text) });
      });
    });
    clientRequest.on('error', reject);
    clientRequest.end(serialized);
  });
}

before(async () => {
  server = createAppServer({ binaryPath: BINARY });
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  port = server.address().port;
});

after(async () => {
  await new Promise((resolve) => server.close(resolve));
});

test('API devolve exatamente o JSON do motor C para todos os algoritmos', async () => {
  const temporaryDirectory = await mkdtemp(path.join(os.tmpdir(), 'scheduler-api-test-'));
  const configPath = path.join(temporaryDirectory, 'config.txt');
  try {
    await writeFile(configPath, 'quantum:2\naging:1\n');
    const direct = spawnSync(BINARY, [
      '--config', configPath, '--algorithm', 'all', '--format', 'json', '--seed', '42',
    ], { input: PROCESS_INPUT, encoding: 'utf8' });
    assert.equal(direct.status, 0, direct.stderr);
    const response = await request('POST', '/api/simulate', {
      processes: PROCESS_INPUT,
      quantum: 2,
      aging: 1,
      seed: 42,
      algorithms: ALL_ALGORITHMS,
    });
    assert.equal(response.status, 200);
    assert.deepEqual(response.body, JSON.parse(direct.stdout));
  } finally {
    await rm(temporaryDirectory, { recursive: true, force: true });
  }
});

test('API filtra os algoritmos selecionados', async () => {
  const response = await request('POST', '/api/simulate', {
    processes: '0 2 1\n', quantum: 2, aging: 0, seed: 7, algorithms: ['srtf', 'rr'],
  });
  assert.equal(response.status, 200);
  assert.deepEqual(response.body.results.map((result) => result.algorithm), ['srtf', 'rr']);
});

test('API estrutura erros de transporte e do motor', async () => {
  const invalidJson = await request('POST', '/api/simulate', '{', 'application/json');
  assert.equal(invalidJson.status, 400);
  assert.equal(invalidJson.body.error.code, 'INVALID_JSON');

  const invalidProcess = await request('POST', '/api/simulate', {
    processes: '0 0 1\n', quantum: 2, aging: 0, seed: 7, algorithms: ['fcfs'],
  });
  assert.equal(invalidProcess.status, 422);
  assert.equal(invalidProcess.body.error.code, 'SIMULATION_ERROR');

  const invalidAlgorithm = await request('POST', '/api/simulate', {
    processes: '0 1 1\n', quantum: 2, aging: 0, seed: 7, algorithms: ['lottery'],
  });
  assert.equal(invalidAlgorithm.status, 400);
  assert.equal(invalidAlgorithm.body.error.details.field, 'algorithms');
});
