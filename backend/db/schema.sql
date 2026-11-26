CREATE TABLE IF NOT EXISTS machines (
    machine_id VARCHAR(20) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    device_key VARCHAR(100) NOT NULL UNIQUE,
    current_state VARCHAR(20) DEFAULT 'OFFLINE',
    current_reason VARCHAR(50),
    last_temp_c NUMERIC(5,2),
    last_seen_at TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS telemetry_logs (
    id SERIAL PRIMARY KEY,
    machine_id VARCHAR(20) REFERENCES machines(machine_id) ON DELETE CASCADE,
    boot_id VARCHAR(16) NOT NULL,
    seq INTEGER NOT NULL,
    state VARCHAR(20) NOT NULL,
    reason VARCHAR(50),
    output_delta INTEGER NOT NULL DEFAULT 0,
    reject_delta INTEGER NOT NULL DEFAULT 0,
    temperature_c NUMERIC(5,2),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    CONSTRAINT unique_device_message UNIQUE (machine_id, boot_id, seq)
);

CREATE TABLE IF NOT EXISTS state_history (
    id SERIAL PRIMARY KEY,
    machine_id VARCHAR(20) REFERENCES machines(machine_id) ON DELETE CASCADE,
    state VARCHAR(20) NOT NULL,
    reason VARCHAR(50),
    started_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_telemetry_machine_date ON telemetry_logs(machine_id, created_at);
CREATE INDEX IF NOT EXISTS idx_state_history_machine_time ON state_history(machine_id, started_at);
