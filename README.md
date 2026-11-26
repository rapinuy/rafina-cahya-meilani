# Mini IoT & MES System Prototype

Prototype sistem pemantauan line produksi berbasis IoT (ESP32, Node.js, PostgreSQL, dan Live Web Dashboard) untuk memantau status mesin, output, reject, dan suhu motor secara real-time.

- **Simulasi Hardware Wokwi:** https://wokwi.com/projects/481753557877895169
- **Waktu Pengerjaan:** Mulai `18:30 WIB` – Selesai `19:45 WIB`

---

## Cara Menjalankan Sistem (5 Langkah)

1. **Siapkan Database PostgreSQL:** Buat database bernama `mini_mes`, lalu jalankan skema dan data awal:
   ```bash
   psql -U postgres -d mini_mes -f backend/db/schema.sql
   psql -U postgres -d mini_mes -f backend/db/seed.sql
