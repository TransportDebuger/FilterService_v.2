#!/bin/bash

sudo apt update

# Install needed packages
sudo apt install -y net-tools inotify-tools python3-venv python3-pip python3-dev build-essential

# Create folders for incoming files flow (inotify testing purposes)
mkdir -p /var/xmlfilter/incoming
sudo chown -R vagrant:vagrant /var/xmlfilter

# Sync incoming files script 
sudo tee /usr/local/bin/xmlfilter-sync-incoming.sh >/dev/null <<'EOF'
#!/usr/bin/env bash
set -e

SRC="/host/incoming"
DST="/var/xmlfilter/incoming"

mkdir -p "$SRC" "$DST"

while true; do
  # Копируем всю структуру каталогов и файлы рекурсивно,
  # сохраняя относительные пути (bypass, ship, auto, rail, avia)
  rsync -a --remove-source-files "$SRC"/ "$DST"/ 2>/dev/null || true
  sleep 1
done
EOF

sudo tee /etc/systemd/system/xmlfilter-sync-incoming.service >/dev/null <<'EOF'
[Unit]
Description=Sync incoming test files from /host/incoming to /var/xmlfilter/incoming

[Service]
Type=simple
User=vagrant
ExecStart=/usr/local/bin/xmlfilter-sync-incoming.sh
Restart=always

[Install]
WantedBy=multi-user.target
EOF

chmod +x /usr/local/bin/xmlfilter-sync-incoming.sh

# Web config app
sudo tee /etc/systemd/system/xmlfilter-config-app.service >/dev/null <<'EOF'
[Unit]
Description=XMLFilter config web app

[Service]
Type=simple
User=vagrant
WorkingDirectory=/opt/xmlfilter/config_app
ExecStart=/usr/bin/bash -lc './run.sh'
Restart=always

[Install]
WantedBy=multi-user.target
EOF

chmod +x /opt/xmlfilter/config_app/run.sh

sudo tee /etc/systemd/system/xmlfilter.service >/dev/null <<'EOF'
[Unit]
Description=XMLFilter main service
After=network.target

[Service]
Type=simple
User=vagrant
WorkingDirectory=/opt/xmlfilter
# Environment=LD_LIBRARY_PATH=/opt/xmlfilter/lib
ExecStart=/usr/bin/bash -lc './xmlfilter # --config /etc/stc/xmlfilter/config.json'
Restart=always
RestartSec=5

# Всё, что пишет процесс в stdout/stderr, пойдёт в journal
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

chmod +x /opt/xmlfilter/xmlfilter

sudo systemctl daemon-reload
sudo systemctl enable --now xmlfilter-config-app.service
sudo systemctl enable --now xmlfilter-sync-incoming.service
sudo systemctl enable --now xmlfilter.service