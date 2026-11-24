const barnNodes = {
    scale: { x: 180, y: 265, eui: '70b3d57ed007985a', color: '#fb923c' },
    vision: { x: 420, y: 147, eui: '44ae5e17e0e18ad8', color: '#34d399' },
    climate: { x: 620, y: 319, eui: '766979864b726282', color: '#60a5fa' },
    gateway: { x: 820, y: 176, color: '#34d399' }
};
let barnZoom = 1;

function drawBarn() {
    const canvas = document.getElementById('barnPlan');
    const ctx = canvas.getContext('2d');
    const layer = document.getElementById('barnScene').dataset.layer;
    const polygon = (points, fill, stroke = '#354357') => {
        ctx.beginPath();
        points.forEach(([x, y], i) => i ? ctx.lineTo(x, y) : ctx.moveTo(x, y));
        ctx.closePath();
        ctx.fillStyle = fill;
        ctx.fill();
        ctx.strokeStyle = stroke;
        ctx.stroke();
    };
    const line = (x, y, endX, endY, color) => {
        ctx.beginPath();
        ctx.moveTo(x, y);
        ctx.lineTo(endX, endY);
        ctx.strokeStyle = color;
        ctx.stroke();
    };

    ctx.fillStyle = '#0C233A';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    for (let x = 20; x < 1000; x += 30) line(x, 0, x, 490, '#143656');
    for (let y = 20; y < 490; y += 30) line(0, y, 1000, y, '#143656');

    polygon([[61, 169], [680, 83], [944, 254], [313, 404]], '#1B466E');
    polygon([[61, 169], [313, 404], [313, 421], [61, 186]], '#143656');
    polygon([[313, 404], [944, 254], [944, 271], [313, 421]], '#173E63');
    polygon([[61, 169], [680, 83], [680, 46], [61, 132]], '#22588A');
    polygon([[680, 83], [944, 254], [944, 217], [680, 46]], '#1A4973');

    for (let i = 0; i <= 8; i++) {
        const t = i / 8;
        line(61 + t * 619, 169 - t * 86, 313 + t * 631, 404 - t * 150, '#26649C');
        line(61 + t * 252, 169 + t * 235, 680 + t * 264, 83 + t * 171, '#26649C');
    }
    polygon([[263, 210], [674, 141], [721, 169], [308, 250]], '#F7941D', '#FBB03B');
    polygon([[360, 293], [776, 206], [824, 235], [405, 334]], '#F7941D', '#FBB03B');
    polygon([[263, 210], [308, 250], [308, 267], [263, 227]], '#C6710F', '#FBB03B');
    polygon([[360, 293], [405, 334], [405, 351], [360, 310]], '#C6710F', '#FBB03B');

    if (layer === 'rf') {
        ctx.setLineDash([8, 6]);
        for (const node of [barnNodes.scale, barnNodes.vision, barnNodes.climate]) {
            if (node.signal) line(node.x, node.y, barnNodes.gateway.x, barnNodes.gateway.y, '#F7941D');
        }
        ctx.setLineDash([]);
    }

    for (const [name, node] of Object.entries(barnNodes)) {
        const rx = name === 'climate' && layer === 'thermal' ? 39 : 22;
        ctx.beginPath();
        ctx.ellipse(node.x, node.y + 27, rx, rx * .45, 0, 0, Math.PI * 2);
        ctx.strokeStyle = node.color;
        ctx.stroke();
    }
}

function setBarnLayer(layer) {
    const scene = document.getElementById('barnScene');
    scene.dataset.layer = layer;
    const tabs = { sensors: 'btnLayerSensors', thermal: 'btnLayerThermal', rf: 'btnLayerRf' };
    for (const [name, id] of Object.entries(tabs)) {
        const button = document.getElementById(id);
        const selected = name === layer;
        button.setAttribute('aria-pressed', String(selected));
        button.classList.toggle('bg-orangebrand', selected);
        button.classList.toggle('text-white', selected);
        button.classList.toggle('bg-[#E2E8F0]', !selected);
        button.classList.toggle('text-[#718096]', !selected);
    }
    document.getElementById('barnLayerStatus').textContent = {
        sensors: 'Selecciona un equipo para ver su detalle.',
        thermal: 'Temperatura y humedad medidas en el sensor ambiental.',
        rf: 'Enlaces de los nodos con mediciones de señal disponibles.'
    }[layer];
    drawBarn();
}

function updateBarnTelemetry(data) {
    const eui = (data.devEui || data.deveui || '').toLowerCase();
    const entry = Object.entries(barnNodes).find(([, node]) => eui && node.eui === eui);
    const metrics = {
        scale: [['anchorWeightVal', data.weightKg, 2, ' kg'], ['anchorHopperVal', data.hopperPercentage, 1, '%']],
        vision: [['anchorEggVal', data.totalEggs, 0, ' huevos']],
        climate: [['anchorClimateVal', data.temperature, 1, ' °C'], ['anchorClimateHum', data.humidity, 1, '%']]
    };
    const timestamp = Date.parse(data.timestamp);
    const groups = entry ? [entry[0]] : eui ? [] : Object.keys(metrics);
    for (const group of groups) {
        const node = barnNodes[group];
        if (Number.isFinite(timestamp) && timestamp < (node.lastReading ?? -Infinity)) continue;
        for (const [id, value, decimals, unit] of metrics[group]) {
            if (Number.isFinite(value) && (entry || !Number.isFinite(node.lastReading))) {
                document.getElementById(id).textContent = value.toFixed(decimals) + unit;
            }
        }
        if (entry && Number.isFinite(timestamp)) node.lastReading = timestamp;
    }

    if (!entry) return;
    const [name, node] = entry;
    if (Number.isFinite(timestamp) && timestamp < (node.lastReading ?? -Infinity)) return;
    const signal = [
        Number.isFinite(data.rssi) && data.rssi !== 0 ? data.rssi + ' dBm' : '',
        Number.isFinite(data.snr) && data.rssi !== 0 ? data.snr + ' dB SNR' : ''
    ].filter(Boolean).join(' · ');
    if (!signal) return;
    node.signal = signal;
    document.querySelector('[data-barn-node="' + name + '"] .barn-rf').textContent = signal;
    if (data.rssi) document.getElementById('anchorGwRssi').textContent = data.rssi + ' dBm';
    if (document.getElementById('barnScene').dataset.layer === 'rf') drawBarn();
}

function zoomBarn(direction) {
    const viewport = document.getElementById('barnViewport');
    const scene = document.getElementById('barnScene');
    const x = (viewport.scrollLeft + viewport.clientWidth / 2) / scene.offsetWidth;
    const y = (viewport.scrollTop + viewport.clientHeight / 2) / scene.offsetHeight;
    barnZoom = Math.max(1, Math.min(2.5, barnZoom + direction * .25));
    scene.style.width = 'max(' + barnZoom * 100 + '%, ' + barnZoom * 550 + 'px)';
    viewport.scrollTo(x * scene.offsetWidth - viewport.clientWidth / 2, y * scene.offsetHeight - viewport.clientHeight / 2);
}

function resetBarnView() {
    barnZoom = 1;
    document.getElementById('barnScene').style.removeProperty('width');
    document.getElementById('barnViewport').scrollTo(0, 0);
}

document.addEventListener('DOMContentLoaded', () => {
    const viewport = document.getElementById('barnViewport');
    for (const button of viewport.querySelectorAll('[data-barn-node]:not([data-barn-node="gateway"])')) {
        const signal = document.createElement('span');
        signal.className = 'barn-rf';
        signal.textContent = 'Sin lectura';
        button.firstElementChild.append(signal);
    }
    setBarnLayer('sensors');

    let drag;
    viewport.addEventListener('pointerdown', event => {
        if (event.pointerType !== 'mouse' || event.button !== 0 || event.target.closest('button')) return;
        drag = { x: event.clientX + viewport.scrollLeft, y: event.clientY + viewport.scrollTop };
        viewport.setPointerCapture(event.pointerId);
        viewport.classList.add('is-dragging');
    });
    viewport.addEventListener('pointermove', event => {
        if (drag) viewport.scrollTo(drag.x - event.clientX, drag.y - event.clientY);
    });
    viewport.addEventListener('lostpointercapture', () => {
        drag = null;
        viewport.classList.remove('is-dragging');
    });
});
