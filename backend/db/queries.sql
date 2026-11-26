-- Menghitung durasi (dalam menit) tiap status MC-01 pada 24 Nov 2026 pukul 08:00 - 12:00 WIB
WITH window_bounds AS (
    SELECT 
        '2026-11-24 08:00:00+07:00'::TIMESTAMPTZ AS win_start,
        '2026-11-24 12:00:00+07:00'::TIMESTAMPTZ AS win_end
),
state_intervals AS (
    SELECT 
        machine_id,
        state,
        started_at AS period_start,
        COALESCE(
            LEAD(started_at) OVER (PARTITION BY machine_id ORDER BY started_at),
            NOW()
        ) AS period_end
    FROM state_history
    WHERE machine_id = 'MC-01'
),
clamped_intervals AS (
    SELECT 
        s.state,
        GREATEST(s.period_start, w.win_start) AS effective_start,
        LEAST(s.period_end, w.win_end) AS effective_end
    FROM state_intervals s
    CROSS JOIN window_bounds w
    WHERE s.period_end > w.win_start 
      AND s.period_start < w.win_end
)
SELECT 
    state,
    ROUND(SUM(EXTRACT(EPOCH FROM (effective_end - effective_start)) / 60.0), 2) AS total_minutes
FROM clamped_intervals
WHERE effective_end > effective_start
GROUP BY state
ORDER BY total_minutes DESC;
