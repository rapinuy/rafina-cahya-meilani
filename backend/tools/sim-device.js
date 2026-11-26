const API_URL = process.env.API_URL || 'http://localhost:3000/api/ingest';

const devices = [
  { machine_id: 'MC-01', key: 'key-mc01-secret-2026', boot_id: '9f3a61c2', seq: 1, state: 'RUNNING' },
  { machine_id: 'MC-02', key: 'key-mc02-secret-2026', boot_id: '4b8e12d0', seq: 1, state: 'IDLE' },
  { machine_id: 'MC-03', key: 'key-mc03-secret-2026', boot_id: '7c1d90e5', seq: 1, state: 'DOWN' }
];

async function sendTelemetry(dev) {
  if (dev.machine_id === 'MC-02' && dev.seq % 3 === 0) {
    dev.state = dev.state === 'IDLE' ? 'RUNNING' : 'IDLE';
  }
  if (dev.machine_id === 'MC-03' && dev.seq % 4 === 0) {
    dev.state = dev.state === 'DOWN' ? 'RUNNING' : 'DOWN';
  }

  const isRunning = dev.state === 'RUNNING';
  const isDown = dev.state === 'DOWN';

  const payload = {
    machine_id: dev.machine_id,
    boot_id: dev.boot_id,
    seq: dev.seq,
    state: dev.state,
    reason: isDown ? 'OVERHEAT' : null,
    output_delta: isRunning ? Math.floor(Math.random() * 6) + 2 : 0,
    reject_delta: isRunning && Math.random() > 0.75 ? 1 : 0,
    temperature_c: isDown
      ? parseFloat((71 + Math.random() * 5).toFixed(1))
      : parseFloat((42 + Math.random() * 12).toFixed(1))
  };

  try {
    const res = await fetch(API_URL, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'X-Device-Key': dev.key
      },
      body: JSON.stringify(payload)
    });

    const result = await res.json();
    console.log(
      `[${new Date().toLocaleTimeString()}] ${dev.machine_id} | seq=${dev.seq} | state=${dev.state} | HTTP ${res.status}`,
      result
    );

    if (res.ok) {
      dev.seq++;
    }
  } catch (err) {
    console.error(`[${dev.machine_id}] Gagal terhubung ke server:`, err.message);
  }
}

console.log(`Mengirim data simulasi ke ${API_URL} setiap 5 detik...`);
devices.forEach(sendTelemetry);
setInterval(() => {
  devices.forEach(sendTelemetry);
}, 5000);
