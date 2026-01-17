/*
 * ============================================================
 * FUNCIONES AUXILIARES PARA INTERFAZ WEB ESP32
 * ============================================================
 * 
 * Este archivo contiene funciones reutilizables para:
 * 1. Gestión de tablas dinámicas (populateTable, createTableRow, createButton)
 * 2. Descarga/eliminación/copia de archivos (downloadFile, handleFileAction, handleFileDelete)
 * 3. Obtención de datos JSON (apiGetJson) - FUNCIÓN PRINCIPAL PARA TODA DATA
 * 4. Formateo de datos (formatDateLocal, formatMinutes, returnFileSize)
 * 5. Validación de tipos de archivo (validFileType)
 * 
 * INCLUIR EN HTML:
 *   <script src="/funciones.js"></script>
 *   (Preferiblemente antes del </body> para no bloquear rendering)
 */

        // Población de tablas
        function populateTable(data, tableId) {
            const tableBody = document.getElementById(tableId);
            tableBody.innerHTML = ""; // Clear existing rows
            data.forEach(file => tableBody.appendChild(createTableRow(file, tableId)));
        }
    
        // Crear filas de tabla dinámicamente
        function createTableRow(file, tableId) {
            const row = document.createElement("tr");
            // Filename (en el JSON viene la ruta completa filepath)
            const filenameCell = document.createElement("td");
            filenameCell.className = file.type == "dir" ? "dirclass" : "filename"; // Add a class for styling (wrap long names if needed)
            filenameCell.setAttribute('data-label','Filename');
            const prefix = file.type == "dir" ? "📁 " : "📄 "; // Definimos el prefijo (Emoji de carpeta o fichero)
            const filenameLink = document.createElement("a");
            if (tableId === "filesTableBody")
                 filenameLink.href = file.type == "dir" ? '/files.htm?dir='+file.name : file.name; 
            else filenameLink.href = file.name; 
            filenameLink.target = "_blank"; // Open in a new tab
            if (tableId === "parmTableBody" || tableId === "backupTableBody")
                 filenameLink.textContent = getFileName(file.name); // show only the file name, not the full path
            else filenameLink.textContent = prefix + file.name; // muestra emoji de tipo
            if (tableId !== "logsTableBody")  // no activa el link directo para los logs
                 filenameCell.appendChild(filenameLink);
            else filenameCell.textContent = "📜 " + getFileName(file.name); // show only the file name, not the full path     
            row.appendChild(filenameCell);

            // Size
            const sizeCell = document.createElement("td");
            sizeCell.textContent = file.type == "dir" ? "directory" : file.size;
            sizeCell.className = file.type == "dir" ? "dirclass" : "fileclass"; // Add a class for styling
            sizeCell.setAttribute('data-label','Size');
            row.appendChild(sizeCell);

            // Timestamp
            const timestampCell = document.createElement("td");
            timestampCell.textContent = new Date(file.time * 1000).toLocaleString();
            timestampCell.setAttribute('data-label','Timestamp');      
            row.appendChild(timestampCell);

            // Actions
            const actionCell = document.createElement("td");
            actionCell.className = "buttoncolumn";
            actionCell.setAttribute('data-label', 'Acciones');
            const buttonContainer = document.createElement("div");
            buttonContainer.className = "button-group";
            if (tableId === "parmTableBody") {
                buttonContainer.appendChild(createButton("Export", () => downloadFile(file.name)));
                buttonContainer.appendChild(createButton("Backup", () => handleFileAction("BACKUP", file.name)));
                buttonContainer.appendChild(createButton("Edit", () => {
                    window.open(`/parmfile_edit.htm?file=${file.name}`, '_self');
                }));
                actionCell.appendChild(buttonContainer);
            } 
            else if (tableId === "backupTableBody") {
                const restoreButton = createButton("Restore", () => {
                    if (confirm("Copiar " + file.name + " a %PARMFILE% ?")) handleFileAction("RESTORE", file.name);
                });
                restoreButton.className = "button-restore";
                actionCell.appendChild(restoreButton); // Aquí lo añades directo a la celda
            } 
            else if (tableId === "filesTableBody" && file.type == "file") {
                buttonContainer.appendChild(createButton("Download", () => downloadFile(file.name)));
                const deleteButton = createButton("Delete", () => {
                    if (confirm("Delete " + file.name + " ?")) handleFileDelete(file.name);
                });
                deleteButton.className = "button-delete";
                buttonContainer.appendChild(deleteButton);
                actionCell.appendChild(buttonContainer);
            } 
            else if (tableId === "logsTableBody" && file.type == "file") {
                buttonContainer.appendChild(createButton("👁️ Ver", () => viewLogFile(file.name)));
                if (file.name === "%ERRORFILE%") {
                    const clearButton = createButton("🧹 Limpiar", () => {
                        if (confirm("¿Vaciar el historial de errores actual?")) handleFileDelete(file.name);
                    });
                    buttonContainer.appendChild(clearButton);
                } else {
                    const deleteButton = createButton("🗑️ Eliminar", () => {
                        if (confirm("¿Eliminar este archivo antiguo?")) handleFileDelete(file.name);
                    });
                    deleteButton.className = "button-delete";
                    buttonContainer.appendChild(deleteButton);
                }
                actionCell.appendChild(buttonContainer);
            }
            row.appendChild(actionCell);
            return row;
        }

        // Crear botones dinámicamente
        function createButton(label, onClick) {
            const button = document.createElement("button");
            button.textContent = label;
            button.addEventListener("click", onClick);
            return button;
        }    

        // Function to handle file download (Export)
        function downloadFile(filename) {
            const url = `/download?file=${encodeURIComponent(filename)}`;
            const a = document.createElement("a");
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            a.remove();
        }

        // Function to view logs files (force reload using Cache Buster)
        function viewLogFile(filename) {
            const cacheBuster = Date.now();
            const path = filename.startsWith('/') ? filename : '/' + filename;
            const url = `${path}?v=${cacheBuster}`;
            window.open(url, '_blank');
        }   

        // Function to handle file copy (Backup, Restore)
        function handleFileAction(action, filename) {
            // call the server copy endpoints as absolute paths (e.g. /BACKUP or /RESTORE)
            fetch('/' + action, { method: 'COPY' })
                .then(response => {
                    if (response.ok) {
                        if (action === "RESTORE") { 
                            if (confirm("Restore OK! ¿Desea reiniciar el sistema para aplicar los cambios?")) {
                                window.location.href = '/api/restart';
                                return; // Importante: detener la ejecución aquí
                            }
                        } else alert(`${action} OK!`);
                        // Si no se reinicia (se pulsa Cancelar o la acción no es RESTORE), recargar la tabla.
                        location.reload(); 
                    } else {
                        response.text().then(text => alert(`Failed to ${action.toLowerCase()} the file. Server error: ${text}`));
                    }
                })
                .catch(error => {
                    console.error(`Error during ${action}:`, error);
                    alert(`Error de red al intentar ${action.toLowerCase()}.`);
                });
        }   

        // Function to handle file deletion
        function handleFileDelete(filename) {
            fetch(filename, { method: 'DELETE' })
            .then(response => {
                if (response.ok) location.reload(); // Reload the page to update the table
                else alert(`Failed to delete the file.`);
            })
            .catch(error => console.error(`Error during delete:`, error));
        }

        // Extrae el nombre de archivo de una ruta completa
        function getFileName(filePath) {
            const lastSlash = filePath.lastIndexOf('/');
            if (lastSlash === -1) return filePath;
            return filePath.substring(lastSlash + 1);
        }

        /**
         * Obtiene JSON desde una ruta o un token.
         * Si el path comienza con '%' (token como %LASTRIEGOS%), solicita al servidor
         * que lo resuelva via /token_file?file=...
         */
        function apiGetJson(path) {
            let fetchUrl = path;
            if (path && path.startsWith('%')) {
                fetchUrl = `/token_file?file=${encodeURIComponent(path)}`;
            }
            return fetch(fetchUrl).then(r => {
                if (!r.ok) throw new Error(`HTTP error! status: ${r.status}`);
                return r.json();
            });
        }

        /**
         * Formatea un timestamp Unix (segundos) a formato local DD/MM/YYYY - HH:MM
         * Usa métodos getUTC* para evitar aplicar la zona horaria, ya que el timestamp ya esta en hora local.
         */
        function formatDateLocal(ts) {
            if (!ts || ts === 0) return "-";
            const d = new Date(ts * 1000);
            const y = d.getUTCFullYear();
            const m = ('0' + (d.getUTCMonth() + 1)).slice(-2);
            const day = ('0' + d.getUTCDate()).slice(-2);
            const h = ('0' + d.getUTCHours()).slice(-2);
            const min = ('0' + d.getUTCMinutes()).slice(-2);
            return `${day}/${m}/${y} - ${h}:${min}`;
        }

        /**
         * Convierte segundos a minutos con formato legible
         * Si < 1 minuto: devuelve con 1 decimal (ej: "0.5 min")
         * Si >= 1 minuto: devuelve redondeado (ej: "5 min")
         */
        function formatMinutes(segundos) {
            const minutos = segundos / 60;
            if (minutos < 1 && segundos > 0) {
                return minutos.toFixed(1);
            }
            return Math.round(minutos);
        }

/**
 * Valida si un fichero tiene una extensión permitida.
 * @param {File} file - El objeto File a validar.
 * @param {string[]} acceptedExtensions - Array de cadenas de extensión permitidas (ej: ['.bin', '.json']).
 * @returns {boolean} True si el archivo tiene una extensión permitida, False en caso contrario.
 */
function validFileType(file, acceptedExtensions) {
    if (!file || !file.name) return false;
    const fileName = file.name.toLowerCase();
    const lastDotIndex = fileName.lastIndexOf('.');
    if (lastDotIndex === -1) return false; 
    const fileExtension = fileName.substring(lastDotIndex);
    return acceptedExtensions.map(ext => ext.toLowerCase()).includes(fileExtension);
}


/** Formatea un tamaño de fichero en bytes a un formato legible (KB, MB). */
function returnFileSize(number) {
    if (number < 1024) {
        return `${number} bytes`;
    } else if (number >= 1024 && number < 1048576) {
        return `${(number / 1024).toFixed(1)} KB`;
    }
    return `${(number / 1048576).toFixed(1)} MB`;
}

/* Intenta parsear la cadena de texto para validar la sintaxis JSON.
 * @returns {object|null} El objeto JSON si la sintaxis es correcta, o null si falla. */
function validarSintaxisJson(jsonString) {
    try {
        return JSON.parse(jsonString);
    } catch (e) {
        console.error("Error de sintaxis JSON:", e);
        throw new Error(`Error de sintaxis JSON: ${e.message}. Por favor, corrige el contenido.`);
    }
}
/* Valida que el objeto JSON tenga la estructura mínima requerida.
 * @param {object} data - El objeto JSON ya parseado. */
function validarEstructura(data) {
    let errores = [];
    // --- Validación de claves principales ---
    if (!data.botones || !Array.isArray(data.botones)) {
        errores.push("Falta la clave 'botones' o no es un array.");
    }
    // --- Validación de contenido mínimo (solo si las claves principales existen) ---
    if (errores.length === 0) {
        // Verificar que 'botones' tenga al menos un elemento con la clave 'zona'
        const tieneBotonValido = data.botones.some(item => 
            typeof item === 'object' && item !== null && 'zona' in item
        );
        if (!tieneBotonValido) {
            errores.push("El array 'botones' debe contener al menos un objeto con la clave 'zona'.");
        }
    }
    if (errores.length > 0) {
        // Usamos throw para que el código llamador se detenga y muestre los errores.
        throw new Error("Errores de Estructura JSON:\n" + errores.join('\n'));
    }
    return true; // La estructura es correcta
}