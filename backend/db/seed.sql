INSERT INTO machines (machine_id, name, device_key) VALUES
('MC-01', 'Mesin Produksi 01', 'key-mc01-secret-2026'),
('MC-02', 'Mesin Produksi 02', 'key-mc02-secret-2026'),
('MC-03', 'Mesin Produksi 03', 'key-mc03-secret-2026')
ON CONFLICT (machine_id) DO NOTHING;
