/* FUNCIONES AUXILIARES INTERFAZ WEB ESP32 */

/** Constantes de firma del fichero de parametros */
const CLIENT_FILE_TYPE = "CCR_config";
const CLIENT_VERSION   = 1;
const CLIENT_SCD_TYPE  = "DOMOTICZ";

/** Obtencion de variables del servidor con caché y verificacion reinicio pendiente sincronizado */
async function getServerConfig(forceRefresh = false) {
    const cached = sessionStorage.getItem('serverConfig');
    const keyRestart = 'needsRestart';
    try {
        const status = await fetch('/api/status').then(r => r.text());
        const serverStatusStr = (status.trim() === '1') ? 'true' : 'false';
        // Si la caché coincide con el servidor, la servimos de inmediato
        if (!forceRefresh && cached && serverStatusStr === sessionStorage.getItem(keyRestart)) {
            return JSON.parse(cached);
        }
        sessionStorage.setItem(keyRestart, serverStatusStr);
        // Si no coincide o se fuerza, descargamos el JSON completo
        const config = await apiGetJson("/api/serverVars");
        if (!config || !Object.keys(config).length) throw 'vacio';
        sessionStorage.setItem('serverConfig', JSON.stringify(config));
        return config;
    } catch (e) { console.error("Err config servidor:", e); return {}; }
}

function renderTableMessage(tableId, message, colspan) {
    const body = document.getElementById(tableId);
    if (body) body.innerHTML = `<tr><td colspan="${colspan}" style="text-align:center;">${message}</td></tr>`;
}

async function fetchTableData(apiUrl, tableId, colspan = 2, errorMsg = "Error cargando datos", emptyMsg = "-- Sin datos --") {
    try {
        const data = await apiGetJson(apiUrl);
        if (!data || (Array.isArray(data) && data.length === 0) || (!Array.isArray(data) && Object.keys(data).length === 0)) {
            renderTableMessage(tableId, emptyMsg, colspan); return null;
        }
        return data;
    } catch (error) {
        console.error(`Err API ${apiUrl}:`, error);
        renderTableMessage(tableId, errorMsg, colspan); return null;
    }
}

async function populateTable(data, tableId) {
    const tableBody = document.getElementById(tableId);
    if (!tableBody) return;
    const config = await getServerConfig();
    tableBody.innerHTML = "";
    data.forEach(file => tableBody.appendChild(createTableRow(file, tableId, config)));
}

/** Crear filas de tabla preparando el html de cada celda según el tipo de tabla y archivo */
function createTableRow(file, tableId, config) {
    const row = document.createElement("tr"), isDir = file.type === "dir", nameOnly = getFileName(file.name), esEng = tableId === "filesTableBody";
    let nameHtml = "", btnsHtml = "";
    // html Celda 1: Nombre y Enlace
    if (tableId === "logsTableBody") {
        nameHtml = "📜 " + nameOnly;
    } else {
        const href = (esEng && isDir) ? 'files.htm?dir=' + file.name : file.name;
        const text = (tableId === "parmTableBody" || tableId === "backupTableBody") ? nameOnly : (isDir ? "📁 " : "📄 ") + file.name;
        nameHtml = `<a href="${href}" target="_blank">${text}</a>`;
    }
    // html Celda 4: Acciones
    if (tableId === "parmTableBody") {
        btnsHtml = `
            <button onclick="downloadFile('${file.name}')">Export</button>
            <button onclick="handleFileAction('BACKUP','${file.name}')">Backup</button>
            <button onclick="window.open('parmfile_edit.htm?file=${file.name}','_self')">Edit</button>
        `;
    } else if (tableId === "backupTableBody") {
        btnsHtml = `
            <button class="button-restore" onclick="restaurarBackup('${file.name}', '${nameOnly}', '${getFileName(config.parmFile)}')">Restore</button>
        `;
    } else if (tableId === "filesTableBody" && !isDir) {
        btnsHtml = `
            <button onclick="downloadFile('${file.name}')">Download</button>
            <button class="button-delete" onclick="confirmarYBorrar('${file.name}', 'Delete ${file.name}?')">Delete</button>
        `;
    } else if (tableId === "logsTableBody" && !isDir) {
        const isPrim = file.name === config.errorFile;
        const msg = isPrim ? '¿Vaciar historial?' : '¿Eliminar archivo?';
        btnsHtml = `
            <button onclick="viewLogFile('${file.name}')">👁️ Ver</button>
            <button class="button-delete" onclick="confirmarYBorrar('${file.name}', '${msg}')">${isPrim ? '🧹 Limpiar' : '🗑️ Eliminar'}</button>
        `;
    }    
    // las celdas 2 (tamaño) y 3 (fecha) se rellenan directamente en el innerHTML
    row.innerHTML = `
        <td class="filename ${isDir?'dirclass':''}" data-label="${esEng?'Filepath':'Nombre'}">${nameHtml}</td>
        <td class="col-size ${isDir?'dirclass':''}" data-label="${esEng?'Size':'Tamaño'}">${isDir?'directory':file.size}</td>
        <td class="col-time" data-label="${esEng?'Timestamp':'Fecha'}">${formatDateLocal(file.time, true)}</td>
        <td class="col-actions" data-label="Acciones"><div class="button-group">${btnsHtml}</div></td>`;
    return row;
}

function confirmarYBorrar(file, mensaje) {
    if (confirm(mensaje)) handleFileDelete(file);
}

function restaurarBackup(file, nameOnly, parmFile) {
    if (confirm(`¿Restaurar ${nameOnly} sobre ${parmFile}?`)) handleFileAction("RESTORE", file);
}

async function loadSimpleTable(data, tableId, isEditable = false, templateSection = {}) {
    const body = document.getElementById(tableId);
    if (!body) return;
    if (!isEditable) {
        body.innerHTML = Object.entries(data).map(([k, v]) => `<tr><td>${k}</td><td>${v || ''}</td></tr>`).join('');
        return;
    }
    // Solo si es editable cargamos reglas y preparamos los inputs
    const config = await getServerConfig(), rules = config.rules || {};
    body.innerHTML = Object.entries(data).map(([key, value]) => {
        const rk = Object.keys(rules).find(rk => key.toLowerCase().includes(rk)) || "";
        const vAttrs = rk ? rules[rk] : "";
        const type = vAttrs.includes("type='number'") || vAttrs.includes('type="number"') ? "number" : "text";
        return `<tr><td>${key}</td><td><input type="${type}" value="${value || ''}" data-field="${key}" placeholder="${templateSection[key] || ''}" ${vAttrs}></td></tr>`;
    }).join('');
}

const getFileName = p => p.includes('/') ? p.substring(p.lastIndexOf('/') + 1) : p;

function downloadFile(f) {
    const a = document.createElement("a");
    a.href = `/api/download?file=${encodeURIComponent(f)}`;
    a.download = f; a.click();
}

function viewLogFile(f) {
    const p = `${f.startsWith('/')?'':'/'}${f}?v=${Date.now()}`;
    fetch(p).then(r => { if (!r.ok) throw 0; return r.text(); }).then(t => {
        const rows = t.split('\n').map(line => {
            if (!line.trim()) return '';
            let c = '';
            if (line.toUpperCase().includes('CCR STARTED')) c = 'background:#dcfce7;color:#166534;font-weight:bold';
            else if (line.includes('[ERROR]')) c = 'background:#fee2e2;color:#991b1b;font-weight:bold';
            else if (line.includes('[WARN]')) c = 'background:#ffedd5;color:#9a3412';
            return `<tr${c?` style="${c}"`:''}><td>${line.replace(/</g,'&lt;').replace(/>/g,'&gt;')}</td></tr>`;
        }).join('');
        const h = `<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>${f}</title>
        <style>body{font:14px/1.4 monospace;padding:8px;margin:0}
        table{width:100%;max-width:1200px;margin:0 auto;border-collapse:collapse}
        td{padding:4px 8px;border-bottom:1px solid #eee;overflow-wrap:break-word}</style>
        </head><body><table>${rows}</table></body></html>`;
        window.open(URL.createObjectURL(new Blob([h], {type:'text/html;charset=utf-8'})), '_blank');
    }).catch(() => window.open(p, '_blank'));
}

async function handleFileAction(action, f) {
    try {
        const r = await fetch('/' + action, { method: 'COPY' });
        if (!r.ok) throw new Error(await r.text());
        if (action === "RESTORE") {
            await fetch('/api/setrestart');
            dispositivoRestart(true, "¡Restauración completada! ¿Desea reiniciar el sistema ahora?");
        } else alert(`${action} OK!`); 
        location.reload();
    } catch (e) { alert(`Error: ${e.message}`); }
}

function handleFileDelete(f) {
    fetch(f, { method: 'DELETE' }).then(r => r.ok ? location.reload() : alert("Error al eliminar"));
}

const apiGetJson = p => fetch(p).then(r => { if (!r.ok) throw new Error(r.status); return r.json(); });

// isUtc=true: ts en UTC, mostrar en hora local. isUtc=false: ts ya en local, no reconvertir.
function formatDateLocal(ts, isUtc = false) {
    if (!ts) return "-";
    const d = new Date(ts * 1000), f = n => n.toString().padStart(2, '0'), u = isUtc ? 'get' : 'getUTC';
    return `${f(d[u+'Date']())}/${f(d[u+'Month']()+1)}/${d[u+'FullYear']()}\u2003${f(d[u+'Hours']())}:${f(d[u+'Minutes']())}`;
}

const formatMinutes = s => s > 0 && s < 60 ? (s / 60).toFixed(1) : Math.round(s / 60);

function returnFileSize(n) {
    if (n < 1024) return n + " bytes";
    return (n / (n < 1048576 ? 1024 : 1048576)).toFixed(1) + (n < 1048576 ? " KB" : " MB");
}

const validFileType = (file, extArray) => file?.name ? extArray.some(ext => file.name.toLowerCase().endsWith(ext.toLowerCase())) : false;

/**
 * Validación de sintaxis y estructura del JSON.
 * @param {string} str - El contenido JSON en formato texto.
 */
function validarJsonCompleto(str) {
    const data = JSON.parse(str);
    if (!data.botones || !Array.isArray(data.botones)) throw new Error("Falta clave 'botones'.");
    if (!data.botones.some(item => item && 'zona' in item)) throw new Error("Debe haber al menos una 'zona' en botones.");
    return data; // Devuelve el objeto validado
}

async function apiSaveConfig(data, askRestart = false) {
    try {
        const response = await fetch('/api/save_config', {
            method: 'POST', headers: { 'Content-Type': 'application/json' }, body: typeof data === 'string' ? data : JSON.stringify(data, null, 2)
        });
        if (!response.ok) throw new Error(await response.text() || `Error: ${response.status}`);
        sessionStorage.removeItem('tempRawData'); hasChanges = false;
        if (askRestart) {
            const resultado = dispositivoRestart();
            if (resultado === "cancelado") window.location.href = 'parmfile.htm'; 
            return;
        }
        alert("Archivo guardado correctamente.");
        window.location.href = 'parmfile.htm';
    } catch (error) { alert(error.message); throw error; }
}

function UI_actualizarEspacioLibre(config, id, isIcon = false) {
    const el = document.getElementById(id);
    if (!el || config.freeFS === undefined) return;
    const esBajo = config.freeFS < (config.maxFS * 0.1);
    if (isIcon) { el.style.display = esBajo ? "inline" : "none"; } 
    else {
        el.textContent = returnFileSize(config.freeFS);
        el.style.color = esBajo ? "#d9534f" : "var(--theme-dark)";
    }
}

// Función única y centralizada para confirmar e invocar el reinicio del ESP32
function dispositivoRestart(activarFlagAlCancelar = true, preguntar = "¿Desea reiniciar el sistema ahora?") {
    if (preguntar && !confirm(preguntar)) {
        if (activarFlagAlCancelar) sessionStorage.setItem('needsRestart', 'true');
        return "cancelado";
    }
    sessionStorage.removeItem('serverConfig');
    sessionStorage.removeItem('needsRestart');
    window.location.href = '/api/restart';
    return "reiniciando";
}

const MSG_ERR_CONFIG = "Faltan parámetros de configuración para esta página.";