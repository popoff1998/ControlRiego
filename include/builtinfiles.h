/**
 * @file builtinfiles.h
 * @brief Basado en the WebServer example for the ESP8266WebServer.
 */

// used for $upload.htm
static const char uploadContent[] PROGMEM =
R"==(
<!doctype html>
<html lang='en' style="font-family: Arial, Helvetica, sans-serif;">

<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Upload</title>
    <style>
        .resaltado-rojo {
            color: rgba(255, 0, 0, 0.759) !important;
            font-size: 1.2em; font-weight: bold;
        }
    </style>
</head>

<body style="width:300px">
    <h1 style="color:#135a8a; margin-left:8px;">Upload files</h1>
    <div style="margin-bottom:30px;"><a href="/">Home</a></div>

    <div style="margin-top:10px;">
        <label for="subdir-select" style="font-size:0.9em;">Subdirectorio destino (opcional):</label>
        
        <select id="subdir-select" style="width:100%;box-sizing:border-box;margin:6px 0; padding: 6px;">
            <option value="">(vacío)</option>
            <option value="datos">datos</option>
        </select>
        </div>

    <hr>
    <div id='zone' style='width:16em;height:12em;padding:10px;background-color:#3da3aa;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center;'>
        <div style='color:white;font-size:1.2em;'>Drop files here...</div>
        <div style='color:white;font-size:1.2em;margin-top:6px;'>... or click to select files</div>
    </div>
    <hr>
    
    <div style="margin-top:10px;">
        <input type="file" multiple id="fileInput" style="display:none" />
    </div>
    
    <a style="color:#828282; font-size:0.9em; text-decoration:none;" title="Go to OTA page" href="/$update">OTA</a>
    
    <script>
        function dragHelper(e) {
            e.stopPropagation();
            e.preventDefault();
        }
        function dropped(e) {
            dragHelper(e);
            var fls = e.dataTransfer.files;
            uploadFiles(fls);
        }
        function uploadFiles(fileList) {
            if (!fileList || fileList.length === 0) {
                alert('No file selected.');
                return;
            }
            var formData = new FormData();
            var rawDir = (document.getElementById('subdir-select') || {value:''}).value || '';
            var safeDir = rawDir.replace(/^\/*/, '').replace(/\/*$/, ''); // quita slashes al inicio/fin
            for (var i = 0; i < fileList.length; i++) {
                var f = fileList[i];
                // construir nombre objetivo: "/{safeDir/}filename"
                var targetName = '/' + (safeDir ? (safeDir + '/') : '') + f.name;
                formData.append('file', f, targetName);
            }
            fetch('/', { method: 'POST', body: formData })
                .then(function (resp) {
                    if (!resp.ok) {
                        // obtener texto devuelto por el servidor (ej. "File too large") y mostrarlo
                        resp.text().then(function(body){
                            window.alert('Upload failed: ' + resp.status + ' - ' + body);
                        });
                        return;
                    }
                    // éxito
                    window.alert('done.');
                })
                .catch(function (err) { window.alert('Upload failed (network)'); console.error(err); });
        }
        // cuando cambie el input (selección por diálogo), subir ficheros
        document.getElementById('fileInput').addEventListener('change', function(e) {
            uploadFiles(e.target.files);
        });
        // drag & drop + click handlers
        var z = document.getElementById('zone');
        z.style.cursor = 'pointer';
        z.addEventListener('click', function (e) {
            // abrir diálogo de selección de ficheros
            document.getElementById('fileInput').click();
        }, false);
        z.addEventListener('dragenter', dragHelper, false);
        z.addEventListener('dragover', dragHelper, false);
        z.addEventListener('drop', dropped, false);
        // Lógica para el resaltado del select
        document.addEventListener('DOMContentLoaded', (event) => {
            const selectElement = document.getElementById('subdir-select');
            const manejarCambio = () => {
                        // Aplica la clase siempre que el valor NO esté vacío
                        if (selectElement.value !== "") {
                            selectElement.classList.add('resaltado-rojo');
                        } else {
                            // Remueve la clase solo cuando se selecciona el valor vacío ("")
                            selectElement.classList.remove('resaltado-rojo');
                        }
                    };
            // Ejecuta una vez para establecer el estado inicial (si es necesario)
            manejarCambio();
            // Añadir el escuchador de eventos
            selectElement.addEventListener('change', manejarCambio);
        });
    </script>
</body>
</html>
)==";

// used for $upload.htm
static const char notFoundContent[] PROGMEM = R"==(
<html>
<head>
  <title>Resource not found</title>
</head>
<body>
  <p>The resource was not found.</p>
  <p><a href="/">Start again</a></p>
</body>
</html>
)==";