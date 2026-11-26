require('dotenv').config();
const express = require('express');
const cors = require('cors');
const path = require('path');
const { Pool } = require('pg');

const app = express();
const pool = new Pool({
  connectionString: process.env.DATABASE_URL
});

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, '../frontend')));

const VALID_STATES = ['RUNNING', 'IDLE', 'DOWN'];

// Endpoint 1: Menerima data telemetri dari ESP32 / Simulator
app.post('/api/ingest', async (req, res) => {
  const deviceKey = req.header('X-Device-Key');
  const {
    machine_id,
    boot_id,
    seq,
    state,
    reason = null,
    output_delta = 0,
    reject_delta = 0,
    temperature_c = null
  } = req.body || {};

  if (!deviceKey) {
    return res.status(401).json({ error: 'Header X-Device-Key wajib disertakan' });
  }

  const isTempValid = temperature_c === null || typeof temperature_c === 'number';
  if (
    typeof machine_id !== 'string' || !machine_id.trim() ||
    typeof boot_id !== 'string' || !boot_id.trim() ||
    typeof seq !== 'number' || !Number.isInteger(seq) || seq < 1 ||
    !VALID_STATES.includes(state) ||
    typeof output_delta !== 'number' || output_delta < 0 ||
    typeof reject_delta !== 'number' || reject_delta < 0 ||
    !isTempValid ||
    (state === 'DOWN' && (typeof reason !== 'string' || !reason.trim()))
  ) {
    return res.status(400).json({ error: 'Payload tidak valid atau reason kosong saat status DOWN' });
  }

  const client = await pool.connect();
  try {
    await client.query('BEGIN');

    const machineRes = await client.query(
      'SELECT machine_id, device_key, current_state FROM machines WHERE machine_id = $1 FOR UPDATE',
      [machine_id]
    );

    if (machineRes.rows.length === 0 || machineRes.rows[0].device_key !== deviceKey) {
      await client.query('ROLLBACK');
      return res.status(401).json({ error: 'Machine ID tidak ditemukan atau Device Key salah' });
    }

    const previousState = machineRes.rows[0].current_state;
    const normalizedReason = state === 'DOWN' ? reason.trim() : null;

    const insertRes = await client.query(
      `INSERT INTO telemetry_logs 
       (machine_id, boot_id, seq, state, reason, output_delta, reject_delta, temperature_c)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
       ON CONFLICT (machine_id, boot_id, seq) DO NOTHING
       RETURNING id`,
      [machine_id, boot_id, seq, state, normalizedReason, output_delta, reject_delta, temperature_c]
    );

    if (insertRes.rowCount === 0) {
      await client.query('COMMIT');
      return res.status(200).json({ status: 'duplicate_ignored' });
    }

    if (previousState !== state) {
      await client.query(
        'INSERT INTO state_history (machine_id, state, reason) VALUES ($1, $2, $3)',
        [machine_id, state, normalizedReason]
      );
    }

    await client.query(
      `UPDATE machines 
       SET current_state = $1,
           current_reason = $2,
           last_temp_c = CASE WHEN $3::numeric IS NOT NULL THEN $3::numeric ELSE last_temp_c END,
           last_seen_at = NOW()
       WHERE machine_id = $4`,
      [state, normalizedReason, temperature_c, machine_id]
    );

    await client.query('COMMIT');
    return res.status(201).json({ status: 'ok' });
  } catch (err) {
    await client.query('ROLLBACK');
    console.error('Error pada /api/ingest:', err);
    return res.status(500).json({ error: 'Terjadi kesalahan pada server' });
  } finally {
    client.release();
  }
});

// Endpoint 2: Mengambil status terkini & akumulasi harian semua mesin
app.get('/api/machines', async (req, res) => {
  try {
    const query = `
      SELECT 
        m.machine_id,
        m.name,
        m.current_state,
        m.current_reason,
        m.last_temp_c,
        m.last_seen_at,
        CASE 
          WHEN m.last_seen_at IS NULL OR EXTRACT(EPOCH FROM (NOW() - m.last_seen_at)) > 30 
          THEN false 
          ELSE true 
        END AS is_online,
        COALESCE(SUM(t.output_delta), 0)::INT AS today_output,
        COALESCE(SUM(t.reject_delta), 0)::INT AS today_reject
      FROM machines m
      LEFT JOIN telemetry_logs t 
        ON m.machine_id = t.machine_id 
        AND t.created_at >= (DATE_TRUNC('day', NOW() AT TIME ZONE 'Asia/Jakarta') AT TIME ZONE 'Asia/Jakarta')
      GROUP BY m.machine_id
      ORDER BY m.machine_id ASC;
    `;

    const { rows } = await pool.query(query);

    const responseData = rows.map(row => ({
      machine_id: row.machine_id,
      name: row.name,
      status: row.is_online ? row.current_state : 'OFFLINE',
      reason: row.is_online && row.current_state === 'DOWN' ? row.current_reason : null,
      is_online: row.is_online,
      temperature_c: row.last_temp_c !== null ? parseFloat(row.last_temp_c) : null,
      today_output: row.today_output,
      today_reject: row.today_reject,
      last_seen_at: row.last_seen_at
    }));

    return res.json(responseData);
  } catch (err) {
    console.error('Error pada /api/machines:', err);
    return res.status(500).json({ error: 'Gagal mengambil data mesin' });
  }
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Server Mini IoT & MES berjalan di http://localhost:${PORT}`);
});
