#pragma once

#include <Arduino.h>

static const char OTA_PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>EVCC Gateway - OTA Firmware Update</title>
    <style>
        :root {
            --bg-body: #0a0f1d;
            --bg-card: #131d33;
            --bg-card-hover: #1b2845;
            --border: #243553;
            --text-main: #f1f5f9;
            --text-muted: #94a3b8;
            --primary: #06b6d4;
            --primary-hover: #0891b2;
            --primary-glow: rgba(6, 182, 212, 0.4);
            --accent-green: #10b981;
            --danger: #ef4444;
            --font-main: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            --font-mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            background-color: var(--bg-body);
            color: var(--text-main);
            font-family: var(--font-main);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            padding: 1.5rem;
        }
        .container {
            width: 100%;
            max-width: 580px;
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: 14px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.5);
            padding: 2rem;
        }
        .header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 1.5rem;
            border-bottom: 1px solid var(--border);
            padding-bottom: 1rem;
        }
        .brand {
            display: flex;
            align-items: center;
            gap: 0.75rem;
        }
        .brand-icon {
            font-size: 1.8rem;
            line-height: 1;
        }
        .brand h1 {
            font-size: 1.25rem;
            font-weight: 700;
            color: #fff;
        }
        .brand p {
            font-size: 0.8rem;
            color: var(--text-muted);
        }
        .back-link {
            font-size: 0.8rem;
            color: var(--primary);
            text-decoration: none;
            display: flex;
            align-items: center;
            gap: 0.3rem;
            transition: color 0.2s;
        }
        .back-link:hover {
            color: #38bdf8;
            text-decoration: underline;
        }
        .info-box {
            background: rgba(15, 23, 42, 0.6);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 0.85rem 1rem;
            margin-bottom: 1.5rem;
            font-size: 0.8rem;
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 0.5rem;
        }
        .info-item span:first-child {
            color: var(--text-muted);
        }
        .info-item span:last-child {
            font-weight: 600;
            color: #38bdf8;
            font-family: var(--font-mono);
        }
        .type-selector {
            display: flex;
            gap: 1rem;
            margin-bottom: 1.25rem;
        }
        .type-option {
            flex: 1;
            display: flex;
            align-items: center;
            gap: 0.5rem;
            background: rgba(15, 23, 42, 0.6);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 0.75rem;
            cursor: pointer;
            font-size: 0.85rem;
            font-weight: 600;
            transition: all 0.2s;
        }
        .type-option:hover {
            border-color: var(--primary);
        }
        .type-option.active {
            border-color: var(--primary);
            background: rgba(6, 182, 212, 0.12);
            color: #38bdf8;
        }
        .type-option input {
            accent-color: var(--primary);
        }
        .dropzone {
            border: 2px dashed var(--border);
            border-radius: 10px;
            padding: 2rem 1.5rem;
            text-align: center;
            cursor: pointer;
            transition: all 0.2s;
            margin-bottom: 1.5rem;
            background: rgba(15, 23, 42, 0.4);
        }
        .dropzone.dragover, .dropzone:hover {
            border-color: var(--primary);
            background: rgba(6, 182, 212, 0.05);
        }
        .dropzone-icon {
            font-size: 2.5rem;
            margin-bottom: 0.5rem;
            opacity: 0.8;
        }
        .dropzone-text {
            font-size: 0.9rem;
            color: #e2e8f0;
            margin-bottom: 0.25rem;
        }
        .dropzone-hint {
            font-size: 0.75rem;
            color: var(--text-muted);
        }
        .file-selected {
            display: none;
            background: rgba(16, 185, 129, 0.1);
            border: 1px solid rgba(16, 185, 129, 0.3);
            border-radius: 8px;
            padding: 0.75rem 1rem;
            margin-bottom: 1.5rem;
            font-size: 0.85rem;
            color: #6ee7b7;
            align-items: center;
            justify-content: space-between;
        }
        .btn-upload {
            width: 100%;
            background: var(--primary);
            color: #041019;
            font-weight: 700;
            font-size: 0.95rem;
            border: none;
            border-radius: 8px;
            padding: 0.85rem;
            cursor: pointer;
            transition: all 0.2s;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 0.5rem;
        }
        .btn-upload:hover:not(:disabled) {
            background: #38bdf8;
            box-shadow: 0 0 15px var(--primary-glow);
        }
        .btn-upload:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        .progress-section {
            display: none;
            margin-top: 1.5rem;
        }
        .progress-bar-bg {
            background: #1e293b;
            border-radius: 9999px;
            height: 12px;
            overflow: hidden;
            border: 1px solid var(--border);
            margin-bottom: 0.5rem;
        }
        .progress-bar-fill {
            background: linear-gradient(90deg, var(--primary) 0%, var(--accent-green) 100%);
            height: 100%;
            width: 0%;
            transition: width 0.15s ease;
        }
        .progress-labels {
            display: flex;
            justify-content: space-between;
            font-size: 0.75rem;
            font-family: var(--font-mono);
            color: var(--text-muted);
        }
        .status-msg {
            margin-top: 1rem;
            padding: 0.75rem;
            border-radius: 8px;
            font-size: 0.85rem;
            display: none;
            text-align: center;
            font-weight: 600;
        }
        .status-msg.success {
            background: rgba(16, 185, 129, 0.15);
            border: 1px solid var(--accent-green);
            color: #6ee7b7;
            display: block;
        }
        .status-msg.error {
            background: rgba(239, 68, 68, 0.15);
            border: 1px solid var(--danger);
            color: #f87171;
            display: block;
        }
        .status-msg.flashing {
            background: rgba(6, 182, 212, 0.15);
            border: 1px solid var(--primary);
            color: #38bdf8;
            display: block;
        }
    </style>
</head>
<body>

    <div class="container">
        <div class="header">
            <div class="brand">
                <div class="brand-icon">⚡</div>
                <div>
                    <h1>Wireless OTA Update</h1>
                    <p>Thunderstruck EVCC Serial-WiFi Gateway</p>
                </div>
            </div>
            <a href="/" class="back-link">← Dashboard</a>
        </div>

        <div class="info-box">
            <div class="info-item"><span>Target MCU:</span> <span>ESP32-S3</span></div>
            <div class="info-item"><span>Flash Size:</span> <span>16 MB</span></div>
            <div class="info-item"><span>Partition Scheme:</span> <span>Dual OTA (A/B)</span></div>
            <div class="info-item"><span>OTA Status:</span> <span style="color:#10b981;">Ready</span></div>
        </div>

        <div class="type-selector">
            <label class="type-option active" id="opt-firmware">
                <input type="radio" name="ota-type" value="firmware" checked onchange="onTypeChanged()">
                <span>Firmware (Application)</span>
            </label>
            <label class="type-option" id="opt-filesystem">
                <input type="radio" name="ota-type" value="fs" onchange="onTypeChanged()">
                <span>Filesystem (LittleFS)</span>
            </label>
        </div>

        <div class="dropzone" id="dropzone" onclick="document.getElementById('file-input').click()">
            <div class="dropzone-icon">📥</div>
            <div class="dropzone-text" id="dropzone-label">Click or Drag & Drop binary (.bin) file here</div>
            <div class="dropzone-hint" id="dropzone-hint">Select compiled firmware.bin (.pio/build/esp32-s3-devkitc-1/firmware.bin)</div>
            <input type="file" id="file-input" accept=".bin" style="display: none;" onchange="onFileSelected(this)">
        </div>

        <div class="file-selected" id="file-selected-box">
            <div>
                <strong id="selected-name">firmware.bin</strong>
                <span id="selected-size" style="font-size:0.75rem; opacity:0.8; margin-left:0.5rem;">(0 KB)</span>
            </div>
            <span style="cursor:pointer; font-weight:bold;" onclick="clearFile(event)">✕</span>
        </div>

        <button class="btn-upload" id="btn-upload" disabled onclick="startUpload()">
            <span>🚀 Flash & Update Device</span>
        </button>

        <div class="progress-section" id="progress-section">
            <div class="progress-bar-bg">
                <div class="progress-bar-fill" id="progress-fill"></div>
            </div>
            <div class="progress-labels">
                <span id="lbl-status">Uploading...</span>
                <span id="lbl-percent">0%</span>
            </div>
        </div>

        <div class="status-msg" id="status-box"></div>
    </div>

    <script>
        let selectedFile = null;

        function onTypeChanged() {
            const isFs = document.querySelector('input[name="ota-type"]:checked').value === 'fs';
            document.getElementById('opt-firmware').classList.toggle('active', !isFs);
            document.getElementById('opt-filesystem').classList.toggle('active', isFs);
            document.getElementById('dropzone-hint').textContent = isFs 
                ? 'Select LittleFS filesystem image (.bin)' 
                : 'Select compiled firmware.bin (.pio/build/esp32-s3-devkitc-1/firmware.bin)';
        }

        const dropzone = document.getElementById('dropzone');
        ['dragenter', 'dragover'].forEach(name => {
            dropzone.addEventListener(name, (e) => {
                e.preventDefault();
                dropzone.classList.add('dragover');
            });
        });
        ['dragleave', 'drop'].forEach(name => {
            dropzone.addEventListener(name, (e) => {
                e.preventDefault();
                dropzone.classList.remove('dragover');
            });
        });
        dropzone.addEventListener('drop', (e) => {
            const files = e.dataTransfer.files;
            if (files && files.length > 0) {
                setFile(files[0]);
            }
        });

        function onFileSelected(input) {
            if (input.files && input.files[0]) {
                setFile(input.files[0]);
            }
        }

        function setFile(file) {
            if (!file.name.toLowerCase().endsWith('.bin')) {
                showStatus('Please select a valid .bin binary file.', 'error');
                return;
            }
            selectedFile = file;
            document.getElementById('selected-name').textContent = file.name;
            document.getElementById('selected-size').textContent = `(${(file.size / 1024).toFixed(1)} KB)`;
            document.getElementById('file-selected-box').style.display = 'flex';
            document.getElementById('dropzone').style.display = 'none';
            document.getElementById('btn-upload').disabled = false;
            hideStatus();
        }

        function clearFile(e) {
            if (e) e.stopPropagation();
            selectedFile = null;
            document.getElementById('file-input').value = '';
            document.getElementById('file-selected-box').style.display = 'none';
            document.getElementById('dropzone').style.display = 'block';
            document.getElementById('btn-upload').disabled = true;
            hideStatus();
        }

        function showStatus(text, type) {
            const box = document.getElementById('status-box');
            box.className = 'status-msg ' + type;
            box.innerHTML = text;
        }

        function hideStatus() {
            const box = document.getElementById('status-box');
            box.className = 'status-msg';
            box.style.display = 'none';
        }

        function startUpload() {
            if (!selectedFile) return;

            const type = document.querySelector('input[name="ota-type"]:checked').value;
            const btn = document.getElementById('btn-upload');
            btn.disabled = true;
            btn.innerHTML = '<span>⏳ Uploading...</span>';

            const progressSection = document.getElementById('progress-section');
            const progressFill = document.getElementById('progress-fill');
            const lblStatus = document.getElementById('lbl-status');
            const lblPercent = document.getElementById('lbl-percent');

            progressSection.style.display = 'block';
            progressFill.style.width = '0%';
            lblStatus.textContent = 'Uploading binary stream...';
            lblPercent.textContent = '0%';
            hideStatus();

            const xhr = new XMLHttpRequest();
            const url = '/update?type=' + encodeURIComponent(type);
            xhr.open('POST', url, true);

            xhr.upload.onprogress = (e) => {
                if (e.lengthComputable) {
                    const percent = Math.round((e.loaded / e.total) * 100);
                    progressFill.style.width = percent + '%';
                    lblPercent.textContent = percent + '%';
                    lblStatus.textContent = `Uploaded ${(e.loaded / (1024 * 1024)).toFixed(2)} MB of ${(e.total / (1024 * 1024)).toFixed(2)} MB`;
                }
            };

            xhr.onload = () => {
                if (xhr.status === 200) {
                    progressFill.style.width = '100%';
                    lblPercent.textContent = '100%';
                    lblStatus.textContent = 'Flash written and verified!';
                    showStatus('✅ Update successful! Device is rebooting into the new firmware...<br><br>Reconnecting in <span id="countdown">10</span> seconds...', 'success');
                    
                    let count = 10;
                    const timer = setInterval(() => {
                        count--;
                        const elem = document.getElementById('countdown');
                        if (elem) elem.textContent = count;
                        if (count <= 0) {
                            clearInterval(timer);
                            window.location.href = '/';
                        }
                    }, 1000);
                } else {
                    btn.disabled = false;
                    btn.innerHTML = '<span>🚀 Retry Flash & Update</span>';
                    showStatus('❌ Update failed (HTTP ' + xhr.status + '): ' + (xhr.responseText || 'Verification error'), 'error');
                }
            };

            xhr.onerror = () => {
                btn.disabled = false;
                btn.innerHTML = '<span>🚀 Retry Flash & Update</span>';
                showStatus('❌ Network error during firmware upload. Check connection to gateway.', 'error');
            };

            const formData = new FormData();
            formData.append('update', selectedFile, selectedFile.name);
            xhr.send(formData);
        }
    </script>
</body>
</html>
)rawliteral";
