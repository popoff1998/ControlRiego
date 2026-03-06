/* ============================================================
 * FUNCIONES AUXILIARES PARA INTERFAZ WEB ESP32 (Versión Completa)
 * ============================================================ */

/** Configuración del servidor con caché */
async function getServerConfig(forceRefresh = false) {
    const cached = sessionStorage.getItem('serverConfig');
    if (!forceRefresh && cached) return JSON.parse(cached);
    try {
        const config = await apiGetJson("/api/serverVars");
        sessionStorage.setItem('serverConfig', JSON.stringify(config));
        return config;
    } catch (e) { return {}; }
}

/** Población de tablas dinámicas */
async function populateTable(data, tableId) {
    const tableBody = document.getElementById(tableId);
    if (!tableBody) return;
    const config = await getServerConfig();
    tableBody.innerHTML = "";
    data.forEach(file => tableBody.appendChild(createTableRow(file, tableId, config)));
}

/** Crear filas de tabla con toda la lógica original */
function createTableRow(file, tableId, config) {
    const row = document.createElement("tr");
    const isDir = file.type === "dir";
    const nameOnly = getFileName(file.name);
    
    // 1. Celda Nombre y Enlace
    const nameCell = document.createElement("td");
    nameCell.className = isDir ? "dirclass" : "filename";
    nameCell.dataset.label = 'Filename';

    if (tableId === "logsTableBody") {
        nameCell.textContent = "📜 " + nameOnly;
    } else {
        const link = document.createElement("a");
        link.target = "_blank";
        // Lógica de enlaces original
        if (tableId === "filesTableBody") link.href = isDir ? '/files.htm?dir=' + file.name : file.name;
        else link.href = file.name;
        
        // Lógica de etiquetas original
        if (tableId === "parmTableBody" || tableId === "backupTableBody") link.textContent = nameOnly;
        else link.textContent = (isDir ? "📁 " : "📄 ") + file.name;
        
        nameCell.appendChild(link);
    }
    row.appendChild(nameCell);

    // 2 y 3. Tamaño y Fecha (Uso de template para ahorrar líneas)
    row.innerHTML += `
        <td class="${isDir ? 'dirclass' : 'fileclass'}" data-label="Size">${isDir ? 'directory' : file.size}</td>
        <td data-label="Timestamp">${new Date(file.time * 1000).toLocaleString()}</td>
    `;

    // 4. Celda de Acciones
    const actionCell = document.createElement("td");
    actionCell.className = "buttoncolumn";
    actionCell.dataset.label = 'Acciones';
    const btnGrp = document.createElement("div");
    btnGrp.className = "button-group";

    if (tableId === "parmTableBody") {
        btnGrp.append(
            createButton("Export", () => downloadFile(file.name)),
            createButton("Backup", () => handleFileAction("BACKUP", file.name)),
            createButton("Edit", () => window.open(`/parmfile_edit.htm?file=${file.name}`, '_self'))
        );
    } else if (tableId === "backupTableBody") {
        btnGrp.appendChild(createButton("Restore", () => {
            if (confirm(`¿Restaurar ${nameOnly} sobre ${getFileName(config.parmFile)}?`)) handleFileAction("RESTORE", file.name);
        }, "button-restore"));
    } else if (tableId === "filesTableBody" && !isDir) {
        btnGrp.append(
            createButton("Download", () => downloadFile(file.name)),
            createButton("Delete", () => confirm(`Delete ${file.name}?`) && handleFileDelete(file.name), "button-delete")
        );
    } else if (tableId === "logsTableBody" && !isDir) {
        btnGrp.appendChild(createButton("👁️ Ver", () => viewLogFile(file.name)));
        const isErr = file.name === config.errorFile;
        btnGrp.appendChild(createButton(isErr ? "🧹 Limpiar" : "🗑️ Eliminar", () => {
            if (confirm(isErr ? "¿Vaciar historial?" : "¿Eliminar archivo?")) handleFileDelete(file.name);
        }, isErr ? "" : "button-delete"));
    }

    actionCell.appendChild(btnGrp);
    row.appendChild(actionCell);
    return row;
}

/** Helpers de UI */
function createButton(label, onClick, cls = "") {
    const b = document.createElement("button");
    b.textContent = label;
    if (cls) b.className = cls;
    b.onclick = onClick;
    return b;
}

function loadSimpleTable(data, tableId, isEditable = true) {
    const body = document.getElementById(tableId);
    if (!body) return;
    body.innerHTML = Object.entries(data).map(([k, v]) => `
        <tr><td>${k}</td><td>${isEditable ? `<input type="text" value="${v || ''}" data-field="${k}">` : (v || '')}</td></tr>
    `).join('');
}

/** Operaciones de Archivo */
const getFileName = p => p.includes('/') ? p.substring(p.lastIndexOf('/') + 1) : p;

function downloadFile(f) {
    const a = document.createElement("a");
    a.href = `/api/download?file=${encodeURIComponent(f)}`;
    a.download = f;
    a.click();
}

function viewLogFile(f) {
    const path = f.startsWith('/') ? f : '/' + f;
    window.open(`${path}?v=${Date.now()}`, '_blank');
}

async function handleFileAction(action, f) {
    try {
        const r = await fetch('/' + action, { method: 'COPY' });
        if (!r.ok) throw new Error(await r.text());
        if (action === "RESTORE") {
            await fetch('/api/setrestart');
            if (confirm("¡Restauración completada! ¿Desea reiniciar el sistema ahora?")) {
                sessionStorage.removeItem('serverConfig');
                sessionStorage.removeItem('needsRestart');
                window.location.href = '/api/restart';
            } else {
                sessionStorage.setItem('needsRestart', 'true');
                location.reload(); }
            return;
        }
        alert(`${action} OK!`);
        location.reload();
    } catch (e) { alert(`Error: ${e.message}`); }
}

function handleFileDelete(f) {
    fetch(f, { method: 'DELETE' }).then(r => r.ok ? location.reload() : alert("Error al eliminar"));
}

/** Formateo y Utilidades de Datos */
const apiGetJson = p => fetch(p).then(r => { if (!r.ok) throw new Error(r.status); return r.json(); });

function formatDateLocal(ts) {
    if (!ts) return "-";
    const d = new Date(ts * 1000);
    const f = n => n.toString().padStart(2, '0');
    return `${f(d.getUTCDate())}/${f(d.getUTCMonth() + 1)}/${d.getUTCFullYear()} - ${f(d.getUTCHours())}:${f(d.getUTCMinutes())}`;
}

const formatMinutes = s => s > 0 && s < 60 ? (s / 60).toFixed(1) : Math.round(s / 60);

function returnFileSize(n) {
    if (n < 1024) return n + " bytes";
    return (n / (n < 1048576 ? 1024 : 1048576)).toFixed(1) + (n < 1048576 ? " KB" : " MB");
}

function validFileType(file, extArray) {
    if (!file?.name) return false;
    const name = file.name.toLowerCase();
    return extArray.some(ext => name.endsWith(ext.toLowerCase()));
}

/** Validaciones de Guardado */
function validarSintaxisJson(str) {
    try { return JSON.parse(str); } catch (e) { throw new Error("Error de sintaxis JSON: " + e.message); }
}

/**
 * Unifica la validación de sintaxis y estructura en un solo paso.
 * @param {string} str - El contenido JSON en formato texto.
 */
function validarJsonCompleto(str) {
    const data = validarSintaxisJson(str);
    if (!data.botones || !Array.isArray(data.botones)) throw new Error("Falta clave 'botones'.");
    if (!data.botones.some(item => item && 'zona' in item)) throw new Error("Debe haber al menos una 'zona' en botones.");
    return data; // Devuelve el objeto validado
}

/**
 * Envía la configuración al servidor y gestiona la respuesta.
 * @param {Object|string} data - Datos a guardar.
 * @param {boolean} askRestart - Si se debe preguntar por reiniciar.
 * @param {boolean} isRaw - Si es true, aplica validaciones de estructura JSON.
 */
async function apiSaveConfig(data, askRestart = false, isRaw = false) {
    try {
        let contentToSend = typeof data === 'string' ? data : JSON.stringify(data);
        if (isRaw) {
            validarJsonCompleto(contentToSend);
            console.log("JSON validado correctamente.");
        }
        const response = await fetch('/api/save_config', {
            method: 'POST', headers: { 'Content-Type': 'application/json' }, body: contentToSend
        });
        if (!response.ok) {
            const errorText = await response.text();
            throw new Error(errorText || `Error del servidor: ${response.status}`);
        }
        sessionStorage.removeItem('tempRawData');
        hasChanges = false
        if (askRestart) {
            if (confirm("Cambios guardados. ¿Desea reiniciar para aplicarlos?")) {
                sessionStorage.removeItem('serverConfig');
                sessionStorage.removeItem('needsRestart');
                window.location.href = '/api/restart';
                return;
            }
            sessionStorage.setItem('needsRestart', 'true');
            window.location.href = '/parmfile.htm';
        } else {
            alert("Archivo guardado correctamente.");
            window.location.href = '/parmfile.htm';
        }

    } catch (error) { alert(error.message); throw error; }
}