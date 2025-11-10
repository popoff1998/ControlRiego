/*
    * Funciones para manejar la interfaz de usuario y la interacción con el servidor usadas en varias paginas.
    * Estas funciones se encargan de cargar datos, crear tablas y manejar eventos de usuario.
    * Se utilizan para mostrar información sobre archivos de configuración y respaldos en la aplicación web.
    * 
    * Para poder llamarlas desde una pagina HTML, se debe incluir este archivo antes de cualquier script que las use
    * preferentemente antes del cierre del </body>, de la siguiente manera:
    *                       <script src="/funciones.js"></script>
    * 
    * Funciones:
    * - populateTable: Llena una tabla con datos de archivos.
    * - createTableRow: Crea una fila de tabla con información de un archivo.
    * - createButton: Crea un botón con un evento de clic.
    * - downloadFile: Maneja la descarga de archivos.
    * - handleFileAction: Maneja acciones de copia de archivos (respaldo y restauración).
    * - handleFileDelete: Maneja la eliminación de archivos.
    * - apiGetJson: Helper para realizar solicitudes GET y obtener JSON.
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
            const filenameLink = document.createElement("a");
            if (tableId === "filesTableBody")
                 filenameLink.href = file.type == "dir" ? '/files.htm?dir='+file.name : file.name; 
            else filenameLink.href = file.name; 
            filenameLink.target = "_blank"; // Open in a new tab
            if (tableId === "parmTableBody" || tableId === "backupTableBody")
                 filenameLink.textContent = getFileName(file.name); // show only the file name, not the full path
            else filenameLink.textContent = file.name;
            filenameCell.appendChild(filenameLink);
            row.appendChild(filenameCell);

            // Size
            const sizeCell = document.createElement("td");
            sizeCell.textContent = file.type == "dir" ? "directory" : file.size;
            sizeCell.className = file.type == "dir" ? "dirclass" : "fileclass"; // Add a class for styling
            sizeCell.style.width = "15%";
            sizeCell.setAttribute('data-label','Size');
            row.appendChild(sizeCell);

            // Timestamp
            const timestampCell = document.createElement("td");
            timestampCell.textContent = new Date(file.time * 1000).toLocaleString();
            timestampCell.style.textAlign = "center";
            timestampCell.style.width = "20%";
            timestampCell.setAttribute('data-label','Timestamp');      
            row.appendChild(timestampCell);

            // Actions
            const actionCell = document.createElement("td");
            actionCell.className = "buttoncolumn"; // Add a class for styling
            actionCell.style.textAlign = "center";
            actionCell.style.width = "30%";
            actionCell.style.whiteSpace = "nowrap"; // Prevent wrapping
            actionCell.style.padding = "8px 3px"; // Add some padding for better spacing
            actionCell.setAttribute('data-label','Acciones');
            if (tableId === "parmTableBody") {
                const buttonContainer = document.createElement("div");
                buttonContainer.style.display = "flex";
                buttonContainer.style.flexWrap = "wrap";
                buttonContainer.style.justifyContent = "center";
                buttonContainer.style.gap = "20px"; // Add spacing between buttons

                buttonContainer.appendChild(createButton("Export", () => downloadFile(file.name)));
                buttonContainer.appendChild(createButton("Backup", () => handleFileAction("BACKUP", file.name)));
                buttonContainer.appendChild(createButton("Edit", () => {
                    window.open(`/parmfile_edit.htm?file=${file.name}`, '_self');  // Open in a the same tab
                    //window.location.href = `/parmfile_edit.htm?file=${file.name}`;  // Open in the same tab
                    //window.open(`/parmfile_edit.htm?file=${file.name}`, '_blank');  // Open in a new tab
                }));

                actionCell.appendChild(buttonContainer);
            } else if (tableId === "backupTableBody") {
                const restoreButton = createButton("Restore", () => {
                    // if (confirm("Recuperar parametros desde " + file.name + " ?")) handleFileAction("RESTORE", file.name);
                    if (confirm("Copiar " + file.name + " a %PARMFILE% ?")) handleFileAction("RESTORE", file.name);
                });
                restoreButton.className = "button-restore"; // Add a class for styling (color teja)
                actionCell.appendChild(restoreButton);
            } else if (tableId === "filesTableBody" && file.type == "file") {
                // Only show download and delete buttons for files
                const buttonContainer = document.createElement("div");
                buttonContainer.style.display = "flex";
                buttonContainer.style.flexWrap = "wrap";
                buttonContainer.style.justifyContent = "center";
                buttonContainer.style.gap = "15px"; // Add spacing between buttons

                buttonContainer.appendChild(createButton("Download", () => downloadFile(file.name)));
                const deleteButton = createButton("Delete", () => {
                    if (confirm("Delete " + file.name + " ?")) handleFileDelete(file.name);
                });
                deleteButton.className = "button-delete"; // Add a class for styling (color rojo)
                buttonContainer.appendChild(deleteButton);

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

        // Function to handle file copy (Backup, Restore)
        function handleFileAction(action, filename) {
            // call the server copy endpoints as absolute paths (e.g. /BACKUP or /RESTORE)
            fetch('/' + action, { method: 'COPY' })
                .then(response => {
                    if (response.ok) {
                        if (action === "RESTORE") { 
                            if (confirm("Restore OK! ¿Desea reiniciar el sistema para aplicar los cambios?")) {
                                return fetch('/api/restart').then(() => {
                                    alert('System restarting...');
                                });                                 
                            }
                        } else alert(`${action} OK!`);
                        location.reload(); // Reload the page to update the table
                    } else {
                        alert(`Failed to ${action.toLowerCase()} the file.`);
                    }
                })
                .catch(error => console.error(`Error during ${action}:`, error));
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

        // función auxiliar (helper) para GET JSON
        function apiGetJson(path) {
            return fetch(path).then(r => {
                if (!r.ok) throw new Error(`HTTP error! status: ${r.status}`);
                return r.json();
            });
        }
