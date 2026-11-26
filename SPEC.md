# System Specification: Line Production IoT & Mini MES (12 Mesin)

## 1. Kebutuhan Sistem (System Requirements)
1. **Akuisisi Status Mesin Real-Time:** Sistem wajib mendeteksi status `RUNNING`, `IDLE`, dan `DOWN` secara otomatis dari sinyal mesin (sensor produk & tombol fault) tanpa bergantung pada pencatatan manual operator.
2. **Pencatatan Output & Reject Sederhana:** Antarmuka input di mesin menggunakan tombol fisik industri (*push button* besar) dengan indikator LED/buzzer sehingga operator tidak perlu mengoperasikan komputer.
3. **Proteksi & Peringatan Dini Suhu Motor:** Sensor suhu membaca temperatur motor secara berkala dengan ambang batas peringatan (*overheat* ≥ 70°C) yang memicu alarm lokal (andon/buzzer) dan notifikasi langsung ke kantor sebelum motor terbakar.
4. **Dashboard Kantor & Perhitungan Downtime:** Aplikasi web menampilkan status 12 mesin secara *live*, mencatat waktu mulai-berakhir setiap kejadian `DOWN`, serta mengakumulasi total durasi *downtime* per mesin/shift.
5. **Toleransi Gangguan Jaringan (Store-and-Forward):** Perangkat ESP32 menyimpan sementara data penghitungan dan log status saat WiFi terputus, lalu mengirim ulang secara otomatis dengan ID unik (*idempotent*) saat koneksi pulih.

## 2. Pertanyaan Klarifikasi untuk Kepala Produksi
1. **Sumber Sinyal Produksi & Mesin:** Apakah mesin-mesin yang ada sudah memiliki panel PLC/kontaktor dengan sinyal kering (*dry contact* / 24V) yang bisa di-*tap* untuk menghitung output dan status jalan, atau harus dipasang sensor eksternal (photoelectric/proximity)?
2. **Definisi "Mesin Berhenti" (Idle vs Down):** Berapa detik jeda tanpa produk keluar hingga mesin dianggap berhenti/idle, dan apakah kategori penyebab *down* (misal: material habis, setting/changeover, kerusakan mekanik) perlu dibedakan tombolnya?
3. **Proses Bisnis Pencatatan Reject:** Di titik mana produk *reject* ditemukan (langsung di mesin oleh operator atau di meja QC akhir), dan apakah ada lebih dari satu jenis *reject* yang perlu dibedakan?
4. **Target Alarm & Eskalasi:** Selain tampil di layar kantor, apakah peringatan *overheat* atau *down* di atas durasi tertentu (misal > 10 menit) perlu dikirim ke grup WhatsApp/Telegram tim Maintenance?
5. **Infrastruktur Listrik & Jaringan:** Apakah tersedia sumber listrik 24V DC / 220V AC yang stabil di tiap panel mesin, dan apakah memungkinkan menarik kabel LAN (*Ethernet*) atau memasang industrial Access Point tambahan untuk mengatasi area *blank spot* WiFi?

## 3. Risiko Pemasangan di Area Produksi & Mitigasi
* **Interferensi Elektromagnetik (EMI) & Lonjakan Tegangan:** Start-stop motor 3-fasa dan kontaktor dapat memicu pembacaan palsu (*false trigger*) atau merusak mikrokontroler. **Mitigasi:** Gunakan isolasi optocoupler pada seluruh input sensor, pasang *flyback diode* / RC *snubber* pada beban induktif, gunakan kabel *shielded twisted pair* (STP), dan pisahkan jalur kabel sinyal dari kabel daya tegangan tinggi.
* **Koneksi WiFi Tidak Stabil / Putus-Putus:** Interferensi logam dan mesin berat menyebabkan paket data hilang. **Mitigasi:** Terapkan mekanisme *local buffering* di memori ESP32 dengan nomor urut (`seq`) agar tidak ada hitungan output/downtime yang hilang, serta pertimbangkan modul ESP32 dengan antena eksternal atau koneksi kabel Ethernet.
* **Lingkungan Ekstrem (Debu, Getaran, Panas, Cipratan Cairan):** Dapat memperpendek umur komponen elektronik. **Mitigasi:** Gunakan enclosure box standar minimal **IP65**, pasang *cable gland* karet, gunakan terminal blok sekrup (bukan kabel jumper), dan tempelkan sensor suhu menggunakan *thermal epoxy* atau *mounting bracket* logam pada bodi motor.
