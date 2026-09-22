#!/usr/bin/env python3
"""
Adds CC/CV Dynamic Tapering UI, interactive Canvas chart, and modal configuration
to data/index.html.
"""
import os

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INDEX_HTML = os.path.join(PROJECT_ROOT, "data", "index.html")

def update_html():
    with open(INDEX_HTML, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. CSS Styles
    css_to_add = """
        /* CC/CV Dynamic Tapering & Saturation Governor */
        .badge-cccv {
            background: rgba(139, 92, 246, 0.15);
            border: 1px solid #8b5cf6;
            color: #c4b5fd;
            cursor: pointer;
            transition: all 0.2s ease;
        }
        .badge-cccv:hover {
            background: rgba(139, 92, 246, 0.3);
            box-shadow: 0 0 10px var(--purple-glow);
        }
        .badge-cccv.tapering {
            background: rgba(245, 158, 11, 0.2);
            border-color: #f59e0b;
            color: #fcd34d;
            animation: cccv-pulse 2s infinite;
        }
        .badge-cccv.complete {
            background: rgba(16, 185, 129, 0.2);
            border-color: #10b981;
            color: #6ee7b7;
        }
        .badge-cccv.disabled {
            background: rgba(100, 116, 139, 0.15);
            border-color: #475569;
            color: #94a3b8;
        }
        @keyframes cccv-pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.65; }
        }

        .cccv-card {
            background: linear-gradient(180deg, #131d33 0%, var(--bg-card) 100%);
            border: 1px solid rgba(139, 92, 246, 0.35);
            border-radius: 12px;
            padding: 1.25rem;
            display: flex;
            flex-direction: column;
            gap: 1rem;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.25);
            margin-bottom: 1.25rem;
        }
        .cccv-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 0.75rem;
            padding-bottom: 0.75rem;
            border-bottom: 1px solid rgba(255, 255, 255, 0.08);
        }
        .cccv-title {
            display: flex;
            align-items: center;
            gap: 0.6rem;
            font-size: 1.05rem;
            font-weight: 700;
            color: #e0e7ff;
        }
        .cccv-title span.tag {
            font-size: 0.7rem;
            padding: 0.2rem 0.5rem;
            background: rgba(139, 92, 246, 0.2);
            border: 1px solid #8b5cf6;
            border-radius: 9999px;
            color: #c4b5fd;
            font-weight: 600;
        }
        .cccv-stats-row {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
            gap: 0.75rem;
        }
        .cccv-stat-box {
            background: rgba(15, 23, 42, 0.6);
            border: 1px solid rgba(255, 255, 255, 0.06);
            border-radius: 8px;
            padding: 0.6rem 0.8rem;
            display: flex;
            flex-direction: column;
        }
        .cccv-stat-label {
            font-size: 0.7rem;
            font-weight: 600;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 0.03em;
        }
        .cccv-stat-val {
            font-size: 1.2rem;
            font-weight: 700;
            font-family: var(--font-mono);
            color: #38bdf8;
            display: flex;
            align-items: baseline;
            gap: 0.2rem;
        }
        .cccv-stat-val span {
            font-size: 0.75rem;
            color: var(--text-muted);
            font-weight: normal;
        }
        .cccv-chart-container {
            background: #030712;
            border: 1px solid rgba(255, 255, 255, 0.08);
            border-radius: 8px;
            height: 220px;
            position: relative;
            overflow: hidden;
        }
        .cccv-chart-container canvas {
            width: 100%;
            height: 100%;
        }
        .cccv-actions-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 0.75rem;
        }
"""
    if ".badge-cccv" not in content:
        content = content.replace(".sim-presets-row {", css_to_add + "\n        .sim-presets-row {", 1)

    # 2. Header Badge
    badge_to_add = """            <div class="badge badge-cccv" id="badge-cccv" onclick="openCccvModal()" title="CC/CV Dynamic Tapering Status (Click to configure curve)">
                <span id="cccv-icon">📉</span>
                <span>CC/CV:</span>
                <strong id="cccv-status-label">BULK CC</strong>
            </div>\n"""
    if 'id="badge-cccv"' not in content:
        content = content.replace('<div class="badge badge-governor"', badge_to_add + '            <div class="badge badge-governor"', 1)

    # 3. Main CC/CV Card (between chargers-grid and sim-panel)
    card_to_add = """
        <!-- CC/CV Dynamic Tapering & Saturation Governor Card -->
        <div class="cccv-card" id="cccv-panel">
            <div class="cccv-header">
                <div class="cccv-title">
                    <span>📉 CC/CV Saturation Governor & Dynamic Tapering</span>
                    <span class="tag" id="cccv-profile-tag">Tesla 36S (4.10V / 147.6V)</span>
                </div>
                <div style="display:flex; align-items:center; gap:0.75rem;">
                    <div style="display:flex; align-items:center; gap:0.4rem;">
                        <span style="font-size:0.75rem; font-weight:600; color:#c4b5fd;">CC/CV Governor:</span>
                        <label class="switch" style="margin:0;">
                            <input type="checkbox" id="chk-cccv-toggle" checked onchange="toggleCccvGovernor(this.checked)">
                            <span class="slider-round"></span>
                        </label>
                    </div>
                    <button class="btn-settings" onclick="openCccvModal()">⚙️ Edit Curve Points</button>
                </div>
            </div>

            <!-- Live Stat Boxes -->
            <div class="cccv-stats-row">
                <div class="cccv-stat-box">
                    <div class="cccv-stat-label">Pack Voltage</div>
                    <div class="cccv-stat-val" id="cccv-stat-vpack">0.0<span>V</span></div>
                </div>
                <div class="cccv-stat-box">
                    <div class="cccv-stat-label">Equiv Cell Voltage</div>
                    <div class="cccv-stat-val" id="cccv-stat-vcell" style="color:#a78bfa;">0.00<span>V/cell</span></div>
                </div>
                <div class="cccv-stat-box">
                    <div class="cccv-stat-label">Curve Target Current</div>
                    <div class="cccv-stat-val" id="cccv-stat-itarget" style="color:#34d399;">50.0<span>A</span></div>
                </div>
                <div class="cccv-stat-box">
                    <div class="cccv-stat-label">Total Active Current</div>
                    <div class="cccv-stat-val" id="cccv-stat-iactive">0.0<span>A</span></div>
                </div>
                <div class="cccv-stat-box">
                    <div class="cccv-stat-label">Tapering Phase</div>
                    <div class="cccv-stat-val" id="cccv-stat-phase" style="font-size:0.95rem; color:#f59e0b;">BULK CC</div>
                </div>
            </div>

            <!-- Canvas Curve Chart -->
            <div class="cccv-chart-container">
                <canvas id="chart-cccv"></canvas>
            </div>

            <div class="cccv-actions-row">
                <div style="font-size:0.75rem; color:var(--text-muted);" id="cccv-status-desc">
                    Targeting 4.10V/cell ceiling to protect Model S NCA chemistry, prevent Li-plating, and give 100mA BMS bleeders balance window.
                </div>
                <div style="display:flex; gap:0.4rem;">
                    <button class="btn-preset" onclick="quickApplyPreset('conservative')">4.10V Conservative</button>
                    <button class="btn-preset" onclick="quickApplyPreset('standard')">4.15V Standard</button>
                    <button class="btn-preset" onclick="quickApplyPreset('max_range')">4.20V Max Range</button>
                </div>
            </div>
        </div>
"""
    if 'id="cccv-panel"' not in content:
        content = content.replace('<!-- EVCC Simulation & Fault Injection Panel -->', card_to_add + '\n        <!-- EVCC Simulation & Fault Injection Panel -->', 1)

    # 4. Parameter settings row
    param_row_to_add = """                <div class="param-row" style="margin-top:0.4rem; padding-top:0.4rem; border-top:1px solid rgba(255,255,255,0.07); justify-content:space-between; align-items:center;">
                    <div style="display:flex; flex-direction:column; gap:2px;">
                        <span class="param-label" style="font-weight:600; color:#c4b5fd;">📉 CC/CV Dynamic Taper:</span>
                        <span id="cccv-param-info" style="font-size:0.72rem; color:var(--text-muted);">Active: 50.0A (Bulk CC) | Target: 147.6V</span>
                    </div>
                    <label class="switch" style="margin:0;">
                        <input type="checkbox" id="chk-cccv-param-enable" checked onchange="toggleCccvGovernor(this.checked)">
                        <span class="slider-round"></span>
                    </label>
                </div>\n"""
    if 'id="chk-cccv-param-enable"' not in content:
        content = content.replace('<!-- Raw EVCC Terminal -->', param_row_to_add + '            </div>\n        </div>\n\n        <!-- Raw EVCC Terminal -->', 1)

    # 5. CC/CV Modal Editor
    modal_to_add = """    <!-- CC/CV Curve Settings Modal -->
    <div class="modal-backdrop" id="modal-cccv">
        <div class="modal" style="max-width: 650px;">
            <div class="modal-header">
                <h3>⚙️ CC/CV Charge Curve Configuration</h3>
                <button class="btn-settings" onclick="closeCccvModal()">✕</button>
            </div>
            <div class="modal-body" style="white-space:normal; display:flex; flex-direction:column; gap:1rem;">
                <div style="font-size:0.8rem; color:var(--text-muted);">
                    Configure the multi-step voltage & current derating curve. The gateway dynamically throttles EVCC <code>maxc</code> along this line to protect against cell overvoltage and internal resistance ($IR$) sag.
                </div>

                <!-- Presets selection -->
                <div style="display:flex; gap:0.5rem; flex-wrap:wrap;">
                    <span style="font-size:0.75rem; font-weight:700; color:var(--text-muted); align-self:center;">Presets:</span>
                    <button class="btn-preset" onclick="fillModalPreset('conservative')">Tesla 36S Conservative (4.10V)</button>
                    <button class="btn-preset" onclick="fillModalPreset('standard')">Tesla 36S Standard (4.15V)</button>
                    <button class="btn-preset" onclick="fillModalPreset('max_range')">Tesla 36S Max (4.20V)</button>
                </div>

                <!-- General Pack Options -->
                <div style="display:grid; grid-template-columns:repeat(auto-fit, minmax(160px, 1fr)); gap:0.75rem; background:#0b0f19; padding:0.75rem; border-radius:8px; border:1px solid #1f2937;">
                    <div>
                        <label style="font-size:0.75rem; color:var(--text-muted); display:block; margin-bottom:0.25rem;">Series Cell Count (S):</label>
                        <input type="number" class="param-input" id="modal-cells" value="36" min="1" max="120" style="width:100%;" oninput="recalcModalCellEquiv()">
                    </div>
                    <div>
                        <label style="font-size:0.75rem; color:var(--text-muted); display:block; margin-bottom:0.25rem;">Ramp Mode:</label>
                        <select class="param-input" id="modal-ramp-mode" style="width:100%;">
                            <option value="linear">Smooth Linear Ramp</option>
                            <option value="step">Discrete Steps</option>
                        </select>
                    </div>
                    <div>
                        <label style="font-size:0.75rem; color:var(--text-muted); display:block; margin-bottom:0.25rem;">Cutoff Current (termc):</label>
                        <input type="number" step="0.1" class="param-input" id="modal-term-amps" value="2.5" style="width:100%;">
                    </div>
                </div>

                <!-- 5 Points Configuration Table -->
                <div>
                    <div style="font-size:0.75rem; font-weight:700; color:#93c5fd; margin-bottom:0.4rem; text-transform:uppercase;">Curve Points (Vpack vs Max Current)</div>
                    <table style="width:100%; border-collapse:collapse; font-size:0.8rem; text-align:left;">
                        <thead>
                            <tr style="color:var(--text-muted); border-bottom:1px solid #374151;">
                                <th style="padding:0.35rem 0.5rem;">Step / Phase</th>
                                <th style="padding:0.35rem 0.5rem;">Pack Volts</th>
                                <th style="padding:0.35rem 0.5rem;">Cell Equiv</th>
                                <th style="padding:0.35rem 0.5rem;">Target Amps</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr style="border-bottom:1px solid rgba(255,255,255,0.05);">
                                <td style="padding:0.4rem 0.5rem; color:#93c5fd; font-weight:600;">1. Bulk CC (< 3.98V)</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.1" class="param-input" id="m-v0" value="143.3" style="width:85px;" oninput="recalcModalCellEquiv()"> V</td>
                                <td style="padding:0.4rem 0.5rem; color:#a78bfa;" id="m-c0">3.980 V</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.5" class="param-input" id="m-a0" value="50.0" style="width:80px;"> A</td>
                            </tr>
                            <tr style="border-bottom:1px solid rgba(255,255,255,0.05);">
                                <td style="padding:0.4rem 0.5rem; color:#93c5fd; font-weight:600;">2. Initial Taper</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.1" class="param-input" id="m-v1" value="145.4" style="width:85px;" oninput="recalcModalCellEquiv()"> V</td>
                                <td style="padding:0.4rem 0.5rem; color:#a78bfa;" id="m-c1">4.039 V</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.5" class="param-input" id="m-a1" value="35.0" style="width:80px;"> A</td>
                            </tr>
                            <tr style="border-bottom:1px solid rgba(255,255,255,0.05);">
                                <td style="padding:0.4rem 0.5rem; color:#93c5fd; font-weight:600;">3. Mid Taper</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.1" class="param-input" id="m-v2" value="146.5" style="width:85px;" oninput="recalcModalCellEquiv()"> V</td>
                                <td style="padding:0.4rem 0.5rem; color:#a78bfa;" id="m-c2">4.069 V</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.5" class="param-input" id="m-a2" value="20.0" style="width:80px;"> A</td>
                            </tr>
                            <tr style="border-bottom:1px solid rgba(255,255,255,0.05);">
                                <td style="padding:0.4rem 0.5rem; color:#93c5fd; font-weight:600;">4. Fine Taper</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.1" class="param-input" id="m-v3" value="147.2" style="width:85px;" oninput="recalcModalCellEquiv()"> V</td>
                                <td style="padding:0.4rem 0.5rem; color:#a78bfa;" id="m-c3">4.089 V</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.5" class="param-input" id="m-a3" value="10.0" style="width:80px;"> A</td>
                            </tr>
                            <tr style="border-bottom:1px solid rgba(255,255,255,0.05);">
                                <td style="padding:0.4rem 0.5rem; color:#93c5fd; font-weight:600;">5. Balance / Ceiling</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.1" class="param-input" id="m-v4" value="147.6" style="width:85px;" oninput="recalcModalCellEquiv()"> V</td>
                                <td style="padding:0.4rem 0.5rem; color:#a78bfa;" id="m-c4">4.100 V</td>
                                <td style="padding:0.4rem 0.5rem;"><input type="number" step="0.5" class="param-input" id="m-a4" value="4.0" style="width:80px;"> A</td>
                            </tr>
                        </tbody>
                    </table>
                </div>

                <div style="font-size:0.75rem; color:#f59e0b;">
                    💡 Tip: For 3 parallel chargers (7.5 kW), you can scale Step 1 up to 60.0A. The governor handles multiple chargers automatically.
                </div>
            </div>
            <div class="modal-footer">
                <button class="btn-settings" onclick="closeCccvModal()">Cancel</button>
                <button class="btn-apply" onclick="saveCccvProfileModal()">💾 Save & Apply Profile</button>
            </div>
        </div>
    </div>\n"""
    if 'id="modal-cccv"' not in content:
        content = content.replace('<!-- Settings Modal -->', modal_to_add + '    <!-- Settings Modal -->', 1)

    # 6. JavaScript Functions
    js_to_add = """
        // ====================================================================
        // CC/CV Dynamic Tapering Logic & Canvas Visualization
        // ====================================================================
        let currentCccvData = null;

        function updateCccvUI(cccv) {
            currentCccvData = cccv;
            const badge = document.getElementById('badge-cccv');
            const icon = document.getElementById('cccv-icon');
            const label = document.getElementById('cccv-status-label');
            const chkMain = document.getElementById('chk-cccv-toggle');
            const chkParam = document.getElementById('chk-cccv-param-enable');
            const paramInfo = document.getElementById('cccv-param-info');
            const desc = document.getElementById('cccv-status-desc');

            if (chkMain) chkMain.checked = cccv.enabled;
            if (chkParam) chkParam.checked = cccv.enabled;

            // Live Stat values
            const vPackEl = document.getElementById('cccv-stat-vpack');
            const vCellEl = document.getElementById('cccv-stat-vcell');
            const iTargEl = document.getElementById('cccv-stat-itarget');
            const iActEl = document.getElementById('cccv-stat-iactive');
            const phaseEl = document.getElementById('cccv-stat-phase');

            if (vPackEl) vPackEl.innerHTML = `${cccv.packVoltage.toFixed(1)}<span>V</span>`;
            if (vCellEl) vCellEl.innerHTML = `${cccv.cellVoltage.toFixed(3)}<span>V/cell</span>`;
            if (iTargEl) iTargEl.innerHTML = `${cccv.targetAmps.toFixed(1)}<span>A</span>`;
            if (iActEl) iActEl.innerHTML = `${cccv.activeCurrent.toFixed(1)}<span>A</span>`;
            if (phaseEl) {
                phaseEl.textContent = cccv.phase.replace('_', ' ');
                if (cccv.phase === 'COMPLETE') phaseEl.style.color = '#10b981';
                else if (cccv.isTapering) phaseEl.style.color = '#f59e0b';
                else phaseEl.style.color = '#38bdf8';
            }

            if (!cccv.enabled) {
                if (badge) {
                    badge.className = 'badge badge-cccv disabled';
                    if (icon) icon.textContent = '⚪';
                    if (label) label.textContent = 'DISABLED';
                }
                if (paramInfo) paramInfo.textContent = 'Disabled | Manual EVCC Control';
            } else if (cccv.phase === 'COMPLETE') {
                if (badge) {
                    badge.className = 'badge badge-cccv complete';
                    if (icon) icon.textContent = '✅';
                    if (label) label.textContent = 'COMPLETE';
                }
                if (paramInfo) paramInfo.innerHTML = `<span style="color:#10b981; font-weight:600;">Charge Complete</span> | Cutoff reached`;
            } else if (cccv.isTapering) {
                if (badge) {
                    badge.className = 'badge badge-cccv tapering';
                    if (icon) icon.textContent = '📉';
                    if (label) label.textContent = `${cccv.phase.replace('_', ' ')} (${cccv.targetAmps.toFixed(1)}A)`;
                }
                if (paramInfo) paramInfo.innerHTML = `<span style="color:#f59e0b; font-weight:600;">⚠️ Tapering (${cccv.phase.replace('_', ' ')})</span> | Target: ${cccv.targetAmps.toFixed(1)}A`;
            } else {
                if (badge) {
                    badge.className = 'badge badge-cccv';
                    if (icon) icon.textContent = '⚡';
                    if (label) label.textContent = `BULK CC (${cccv.targetAmps.toFixed(1)}A)`;
                }
                if (paramInfo) paramInfo.textContent = `Active: ${cccv.targetAmps.toFixed(1)}A (Bulk CC) | ${cccv.packVoltage.toFixed(1)}V`;
            }

            if (desc) {
                desc.textContent = cccv.statusText;
            }

            // Draw interactive curve chart
            drawCccvChart('chart-cccv', cccv);
        }

        function drawCccvChart(canvasId, cccv) {
            const canvas = document.getElementById(canvasId);
            if (!canvas || !cccv || !cccv.points || !cccv.points.length) return;
            const ctx = canvas.getContext('2d');
            const width = canvas.width = canvas.parentElement.clientWidth;
            const height = canvas.height = canvas.parentElement.clientHeight;

            ctx.clearRect(0, 0, width, height);

            const padL = 50, padR = 45, padT = 25, padB = 25;
            const plotW = width - padL - padR;
            const plotH = height - padT - padB;

            // X-axis: Pack Voltage (bounds based on profile)
            const minV = Math.floor(cccv.points[0].v - 4.0);
            const maxV = Math.ceil(cccv.points[cccv.points.length - 1].v + 2.0);

            // Y-axis: Amps (0 to max amps + 10%)
            let maxA = 60.0;
            cccv.points.forEach(p => { if (p.a > maxA) maxA = p.a; });
            maxA = Math.ceil(maxA / 10) * 10;
            const minA = 0.0;

            // Draw grid & Y-ticks
            ctx.strokeStyle = '#1e293b';
            ctx.lineWidth = 1;
            for (let i = 0; i <= 4; i++) {
                const frac = 1.0 - (i / 4.0);
                const y = padT + (plotH / 4) * i;
                ctx.beginPath();
                ctx.moveTo(padL, y);
                ctx.lineTo(width - padR, y);
                ctx.stroke();

                ctx.fillStyle = '#64748b';
                ctx.font = '10px monospace';
                ctx.textAlign = 'right';
                const aVal = (minA + (maxA - minA) * frac).toFixed(0);
                ctx.fillText(`${aVal}A`, padL - 6, y + 3);
            }

            // Draw X-ticks
            const numXTicks = 5;
            for (let i = 0; i <= numXTicks; i++) {
                const frac = i / numXTicks;
                const x = padL + plotW * frac;
                const vVal = (minV + (maxV - minV) * frac).toFixed(1);
                const cellV = (vVal / (cccv.cellCount || 36)).toFixed(2);

                ctx.strokeStyle = '#1e293b';
                ctx.beginPath();
                ctx.moveTo(x, padT);
                ctx.lineTo(x, padT + plotH);
                ctx.stroke();

                ctx.fillStyle = '#64748b';
                ctx.font = '10px monospace';
                ctx.textAlign = 'center';
                ctx.fillText(`${vVal}V`, x, padT + plotH + 14);
                ctx.fillStyle = '#8b5cf6';
                ctx.font = '9px monospace';
                ctx.fillText(`${cellV}V/c`, x, padT + plotH + 23);
            }

            // Project point helper
            const toX = (v) => padL + Math.max(0, Math.min(1, (v - minV) / (maxV - minV))) * plotW;
            const toY = (a) => padT + plotH - Math.max(0, Math.min(1, (a - minA) / (maxA - minA))) * plotH;

            // Shaded Area Under Curve
            ctx.beginPath();
            ctx.moveTo(toX(minV), toY(0));
            ctx.lineTo(toX(minV), toY(cccv.points[0].a));
            cccv.points.forEach((p, idx) => {
                if (!cccv.smoothLinear && idx > 0) {
                    ctx.lineTo(toX(p.v), toY(cccv.points[idx - 1].a));
                }
                ctx.lineTo(toX(p.v), toY(p.a));
            });
            ctx.lineTo(toX(maxV), toY(0));
            ctx.closePath();
            const grad = ctx.createLinearGradient(0, padT, 0, padT + plotH);
            grad.addColorStop(0, 'rgba(139, 92, 246, 0.25)');
            grad.addColorStop(1, 'rgba(139, 92, 246, 0.02)');
            ctx.fillStyle = grad;
            ctx.fill();

            // Draw Curve Line
            ctx.beginPath();
            ctx.strokeStyle = '#a78bfa';
            ctx.lineWidth = 2.5;
            ctx.moveTo(toX(minV), toY(cccv.points[0].a));
            cccv.points.forEach((p, idx) => {
                if (!cccv.smoothLinear && idx > 0) {
                    ctx.lineTo(toX(p.v), toY(cccv.points[idx - 1].a));
                }
                ctx.lineTo(toX(p.v), toY(p.a));
            });
            ctx.stroke();

            // Draw Setpoint Dots & Labels
            cccv.points.forEach((p, idx) => {
                const px = toX(p.v);
                const py = toY(p.a);
                ctx.fillStyle = '#8b5cf6';
                ctx.beginPath();
                ctx.arc(px, py, 4.5, 0, Math.PI * 2);
                ctx.fill();
                ctx.strokeStyle = '#ffffff';
                ctx.lineWidth = 1.5;
                ctx.stroke();

                // Point text
                ctx.fillStyle = '#c4b5fd';
                ctx.font = '10px sans-serif';
                ctx.textAlign = 'center';
                ctx.fillText(`P${idx+1}: ${p.a}A`, px, py - 8);
            });

            // Draw Live Operating Point Marker (if pack voltage > 0)
            if (cccv.packVoltage > 0) {
                const liveX = toX(cccv.packVoltage);
                const liveY = toY(cccv.targetAmps);

                // Crosshairs
                ctx.strokeStyle = 'rgba(56, 189, 248, 0.4)';
                ctx.setLineDash([4, 4]);
                ctx.beginPath();
                ctx.moveTo(liveX, padT);
                ctx.lineTo(liveX, padT + plotH);
                ctx.moveTo(padL, liveY);
                ctx.lineTo(width - padR, liveY);
                ctx.stroke();
                ctx.setLineDash([]);

                // Glowing outer ring
                ctx.beginPath();
                ctx.arc(liveX, liveY, 9, 0, Math.PI * 2);
                ctx.fillStyle = 'rgba(56, 189, 248, 0.3)';
                ctx.fill();

                // Live center point
                ctx.beginPath();
                ctx.arc(liveX, liveY, 5, 0, Math.PI * 2);
                ctx.fillStyle = '#38bdf8';
                ctx.fill();
                ctx.strokeStyle = '#ffffff';
                ctx.lineWidth = 2;
                ctx.stroke();

                // Live badge tag on chart
                ctx.fillStyle = '#030712';
                ctx.strokeStyle = '#38bdf8';
                ctx.lineWidth = 1;
                ctx.fillRect(liveX - 40, liveY - 26, 80, 16);
                ctx.strokeRect(liveX - 40, liveY - 26, 80, 16);
                ctx.fillStyle = '#38bdf8';
                ctx.font = 'bold 9px monospace';
                ctx.textAlign = 'center';
                ctx.fillText(`LIVE: ${cccv.targetAmps.toFixed(1)}A`, liveX, liveY - 14);
            }
        }

        function toggleCccvGovernor(enabled) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ action: 'toggle_cccv', enabled: enabled }));
                appendLog(`[CC/CV] Set governor: ${enabled ? 'ENABLED' : 'DISABLED'}\\n`);
            }
        }

        function quickApplyPreset(preset) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ action: 'set_cccv_preset', preset: preset }));
                appendLog(`[CC/CV] Applied preset: ${preset}\\n`);
                const tag = document.getElementById('cccv-profile-tag');
                if (tag) {
                    if (preset === 'conservative') tag.textContent = 'Tesla 36S Conservative (4.10V)';
                    else if (preset === 'standard') tag.textContent = 'Tesla 36S Standard (4.15V)';
                    else if (preset === 'max_range') tag.textContent = 'Tesla 36S Max (4.20V)';
                }
            }
        }

        function openCccvModal() {
            if (currentCccvData && currentCccvData.points) {
                document.getElementById('modal-cells').value = currentCccvData.cellCount || 36;
                document.getElementById('modal-ramp-mode').value = currentCccvData.smoothLinear ? 'linear' : 'step';
                document.getElementById('modal-term-amps').value = currentCccvData.termAmps || 2.5;

                currentCccvData.points.forEach((p, idx) => {
                    const vInp = document.getElementById(`m-v${idx}`);
                    const aInp = document.getElementById(`m-a${idx}`);
                    if (vInp) vInp.value = p.v.toFixed(1);
                    if (aInp) aInp.value = p.a.toFixed(1);
                });
                recalcModalCellEquiv();
            }
            document.getElementById('modal-cccv').style.display = 'flex';
        }

        function closeCccvModal() {
            document.getElementById('modal-cccv').style.display = 'none';
        }

        function fillModalPreset(preset) {
            const cells = 36;
            document.getElementById('modal-cells').value = cells;
            document.getElementById('modal-ramp-mode').value = 'linear';
            document.getElementById('modal-term-amps').value = '2.5';

            if (preset === 'conservative') {
                setModalPoint(0, 143.3, 50.0);
                setModalPoint(1, 145.4, 35.0);
                setModalPoint(2, 146.5, 20.0);
                setModalPoint(3, 147.2, 10.0);
                setModalPoint(4, 147.6, 4.0);
            } else if (preset === 'standard') {
                setModalPoint(0, 144.5, 50.0);
                setModalPoint(1, 146.8, 35.0);
                setModalPoint(2, 148.0, 20.0);
                setModalPoint(3, 148.8, 10.0);
                setModalPoint(4, 149.4, 4.0);
            } else if (preset === 'max_range') {
                setModalPoint(0, 146.0, 50.0);
                setModalPoint(1, 148.2, 35.0);
                setModalPoint(2, 149.6, 20.0);
                setModalPoint(3, 150.5, 10.0);
                setModalPoint(4, 151.2, 4.0);
            }
            recalcModalCellEquiv();
        }

        function setModalPoint(idx, v, a) {
            const vInp = document.getElementById(`m-v${idx}`);
            const aInp = document.getElementById(`m-a${idx}`);
            if (vInp) vInp.value = v.toFixed(1);
            if (aInp) aInp.value = a.toFixed(1);
        }

        function recalcModalCellEquiv() {
            const cells = parseFloat(document.getElementById('modal-cells').value) || 36;
            for (let i = 0; i < 5; i++) {
                const v = parseFloat(document.getElementById(`m-v${i}`).value) || 0;
                const cellV = (v / cells).toFixed(3);
                const el = document.getElementById(`m-c${i}`);
                if (el) el.textContent = `${cellV} V`;
            }
        }

        function saveCccvProfileModal() {
            const cells = parseInt(document.getElementById('modal-cells').value) || 36;
            const smooth = document.getElementById('modal-ramp-mode').value === 'linear';
            const term = parseFloat(document.getElementById('modal-term-amps').value) || 2.5;

            const pts = [];
            for (let i = 0; i < 5; i++) {
                const v = parseFloat(document.getElementById(`m-v${i}`).value) || 0;
                const a = parseFloat(document.getElementById(`m-a${i}`).value) || 0;
                pts.push({ v, a });
            }

            const payload = {
                action: 'set_cccv_profile',
                enabled: true,
                cellCount: cells,
                smoothLinear: smooth,
                termAmps: term,
                points: pts
            };

            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify(payload));
                appendLog('[CC/CV] Profile configuration saved & applied.\\n');
                closeCccvModal();
            }
        }
"""
    if "updateCccvUI" not in content:
        content = content.replace("function updateGovernorUI(gov) {", js_to_add + "\n        function updateGovernorUI(gov) {", 1)

    # 7. Call updateCccvUI in updateTelemetry
    telemetry_hook = """            // CC/CV status update
            if (data.cccv) {
                updateCccvUI(data.cccv);
            }\n"""
    if "data.cccv" not in content:
        content = content.replace("// Governor status update", telemetry_hook + "            // Governor status update", 1)

    with open(INDEX_HTML, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"Updated {INDEX_HTML} successfully!")

if __name__ == "__main__":
    update_html()
