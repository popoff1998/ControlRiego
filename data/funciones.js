/*
    * Funciones para manejar la interfaz de usuario y la interacción con el servidor usadas en varias paginas.
    * Estas funciones se encargan de cargar datos, crear tablas y manejar eventos de usuario.
    * Se utilizan para mostrar información sobre archivos de configuración y respaldos en la aplicación web.
    * 
    * Para poder llamarlas desde una pagina HTML, se debe incluir este archivo en el HEAD de la siguiente manera:
    *                       <script src="/funciones.js"></script>
    * 
    * Funciones:
    * - populateTable: Llena una tabla con datos de archivos.
    * - createTableRow: Crea una fila de tabla con información de un archivo.
    * - createButton: Crea un botón con un evento de clic.
    * - downloadFile: Maneja la descarga de archivos.
    * - handleFileAction: Maneja acciones de copia de archivos (respaldo y restauración).
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

            // Filename
            const filenameCell = document.createElement("td");
            const filenameLink = document.createElement("a");
            filenameLink.href = '/' + file.name; // Assuming the file can be accessed directly via this URL
            filenameLink.target = "_blank"; // Open in a new tab
            filenameLink.textContent = file.name;
            filenameCell.appendChild(filenameLink);
            row.appendChild(filenameCell);

            // Size
            const sizeCell = document.createElement("td");
            sizeCell.textContent = file.size;
            sizeCell.style.textAlign = "center";
            sizeCell.style.width = "15%";
            row.appendChild(sizeCell);

            // Timestamp
            const timestampCell = document.createElement("td");
            timestampCell.textContent = new Date(file.time * 1000).toLocaleString();
            timestampCell.style.textAlign = "center";
            timestampCell.style.width = "20%";      
            row.appendChild(timestampCell);

            // Actions
            const actionCell = document.createElement("td");
            actionCell.style.textAlign = "center";
            actionCell.style.width = "30%";
            actionCell.style.whiteSpace = "nowrap"; // Prevent wrapping
            actionCell.style.padding = "5px 3px"; // Add some padding for better spacing
            if (tableId === "parmTableBody") {
                const buttonContainer = document.createElement("div");
                buttonContainer.style.display = "flex";
                buttonContainer.style.flexWrap = "wrap";
                buttonContainer.style.justifyContent = "center";
                buttonContainer.style.gap = "10px"; // Add spacing between buttons

                buttonContainer.appendChild(createButton("Export", () => downloadFile(file.name)));
                buttonContainer.appendChild(createButton("Backup", () => handleFileAction("BACKUP", file.name)));
                buttonContainer.appendChild(createButton("Edit", () => {
                    window.open(`/parmfile_edit.htm?file=${file.name}`, '_blank');  // Open in a new tab
                }));

                actionCell.appendChild(buttonContainer);
            } else if (tableId === "backupTableBody") {
                const restoreButton = createButton("Restore", () => {
                    if (confirm("Copiar " + file.name + " a config_parm.json ?")) {
                        handleFileAction("RESTORE", file.name);
                    }
                });
                restoreButton.className = "button-restore"; // Add a class for styling (color teja)
                actionCell.appendChild(restoreButton);
            } else if (tableId === "filesTableBody") {
                const buttonContainer = document.createElement("div");
                buttonContainer.style.display = "flex";
                buttonContainer.style.flexWrap = "wrap";
                buttonContainer.style.justifyContent = "center";
                buttonContainer.style.gap = "10px"; // Add spacing between buttons

                buttonContainer.appendChild(createButton("Download", () => downloadFile(file.name)));
                const deleteButton = createButton("Delete", () => {
                    if (confirm("Delete " + file.name + " ?")) {
                        fetch(file.name, { method: 'DELETE' }) .then (response => location.reload()); // Reload the page to update the table
                    }
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
            fetch(action, { method: 'COPY' })
                .then(response => {
                    if (response.ok) {
                        alert(`${action} OK!`);
                        location.reload();
                    } else {
                        alert(`Failed to ${action.toLowerCase()} the file.`);
                    }
                })
                .catch(error => console.error(`Error during ${action}:`, error));
        }

