let sdnHistory = [];
let sdnPreviewRequest = 0;
let sdnPreviewTimer;
let sdnDispatching = false;

function sdnPayload() {
    const value = id => document.getElementById(id).value;
    const numericIds = ['sdnCmdId', 'sdnSfTarget', 'sdnTxPower', 'sdnSampleRate', 'sdnPayloadMode', 'sdnSleepEnable', 'sdnParamValue', 'sdnFPort'];
    if (numericIds.some(id => !value(id) || !document.getElementById(id).checkValidity())) return null;
    const [cmdId, sfTarget, txPower, sampleRate, payloadMode, sleepEnable, paramValue, fPort] = numericIds.map(id => Number(value(id)));
    if (txPower < 0 || txPower > 15) return null;
    return { devEui: value('sdnDevEui').trim().toLowerCase(), cmdId, sfTarget, txPower, sampleRate,
        payloadMode, sleepEnable, paramValue, fport: fPort, confirmed: document.getElementById('sdnConfirmed').checked };
}

async function getSdnPreview(payload) {
    const response = await fetch('/api/sdn/preview', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    });
    if (!response.ok) throw new Error('No se pudo calcular la vista previa.');
    return response.json();
}

function renderSdnPreview(preview, port) {
    const values = { sdnHexDump: preview?.hexDump, sdnBase64: preview?.base64, sdnRawHex: preview?.rawWordHex,
        sdnDecodedCmd: preview?.cmdName, sdnDecodedRate: preview ? preview.sampleRateSeconds + ' s' : null, sdnPreviewPort: port };
    for (const [id, value] of Object.entries(values)) {
        document.getElementById(id).textContent = value ?? '—';
    }
}

function calcSdnPreview() {
    const request = ++sdnPreviewRequest;
    clearTimeout(sdnPreviewTimer);
    const status = document.getElementById('sdnPreviewStatus');
    const payload = sdnPayload();
    if (!payload) {
        status.textContent = 'Revisa los campos numéricos del comando.';
        renderSdnPreview();
        return;
    }
    status.textContent = 'Calculando vista previa…';
    sdnPreviewTimer = setTimeout(async () => {
        try {
            const preview = await getSdnPreview(payload);
            if (request !== sdnPreviewRequest) return;
            renderSdnPreview(preview, payload.fport);
            status.textContent = '';
        } catch (error) {
            if (request !== sdnPreviewRequest) return;
            renderSdnPreview();
            status.textContent = 'No se pudo cargar la vista previa.';
        }
    }, 180);
}

async function sendSdnDownlink() {
    if (currentUser?.role !== 'ROLE_ADMIN' || sdnDispatching) return;
    const status = document.getElementById('sdnSendStatus');
    const payload = sdnPayload();
    if (!payload || !/^[a-f0-9]{16}$/.test(payload.devEui)) {
        status.textContent = 'Selecciona un DevEUI de 16 dígitos hexadecimales y revisa los parámetros.';
        return;
    }

    const button = document.getElementById('btnDispatchSdn');
    const token = currentUser.token;
    sdnDispatching = true;
    button.disabled = true;
    status.textContent = '';
    try {
        const preview = await getSdnPreview(payload);
        if (currentUser?.token !== token) return;
        const summary = [preview.cmdName, 'Nodo ' + payload.devEui, 'Puerto ' + payload.fport,
            preview.hexDump].filter(Boolean).join(' · ');
        if (!confirm('¿Enviar este comando?\n' + summary)) return;

        const response = await sessionFetch('/api/sdn/downlink', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const result = await response.json().catch(() => ({}));
        if (!response.ok) throw new Error(result.error || result.message || 'El servidor rechazó el comando.');
        status.textContent = downlinkStatus(result.status) + ' · ' + payload.devEui;
        loadSdnHistory();
    } catch (error) {
        if (currentUser?.token === token) status.textContent = error.message;
    } finally {
        sdnDispatching = false;
        button.disabled = currentUser?.role !== 'ROLE_ADMIN';
    }
}

function downlinkStatus(status) {
    if (status === 'ENQUEUED_IN_CHIRPSTACK') return 'Publicado para ChirpStack';
    return status || 'Estado sin reportar';
}

async function loadSdnDevices() {
    try {
        const response = await sessionFetch('/api/devices');
        if (!response.ok) return;
        const devices = await response.json();
        const options = devices.map(device => {
            const option = document.createElement('option');
            option.value = device.devEui;
            option.label = device.name || device.devEui;
            return option;
        });
        document.getElementById('sdnDeviceList').replaceChildren(...options);
    } catch (error) {
        // El DevEUI también puede ingresarse manualmente.
    }
}

async function loadSdnHistory() {
    if (!currentUser) return;
    const status = document.getElementById('sdnHistoryStatus');
    status.textContent = 'Cargando historial…';
    try {
        const response = await sessionFetch('/api/sdn/downlinks');
        if (!response.ok) throw new Error();
        sdnHistory = await response.json();
        renderSdnHistory();
    } catch (error) {
        if (currentUser) status.textContent = 'No se pudo cargar el historial.';
    }
}

function renderSdnHistory() {
    const container = document.getElementById('sdnHistoryRows');
    const filterNode = document.getElementById('sdnHistoryNode').value.trim().toLowerCase();
    const filterDate = document.getElementById('sdnHistoryDate').value;
    const rows = sdnHistory.flatMap(event => {
        const device = event.devEui || '';
        const day = new Date(event.timestamp);
        const dateValid = Number.isFinite(day.getTime());
        const localDate = dateValid ? new Date(day - day.getTimezoneOffset() * 60000).toISOString().slice(0, 10) : '';
        if (!device.toLowerCase().includes(filterNode) || (filterDate && localDate !== filterDate)) return [];
        const row = document.createElement('tr');
        for (const value of [dateValid ? day.toLocaleString('es-CO') : '—', device || '—', event.cmdName || event.cmdId, downlinkStatus(event.status)]) {
            const cell = row.insertCell();
            cell.className = 'p-2';
            cell.textContent = value ?? '—';
        }
        return [row];
    });
    container.replaceChildren(...rows);
    document.getElementById('sdnHistoryStatus').textContent = rows.length ? '' : 'No hay comandos para esta consulta.';
}
