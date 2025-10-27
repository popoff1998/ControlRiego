/**
 * @file builtinfiles.h
 * @brief This file is part of the WebServer example for the ESP8266WebServer.
 *  
 * This file contains long, multiline text variables for  all builtin resources.
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
</head>

<body style="width:300px">
  <h1 style="color:#135a8a; margin-left:8px;">Upload files</h1>
  <div style="margin-bottom:30px;"><a href="/">Home</a></div>

  <hr>
  <div id='zone' style='width:16em;height:12em;padding:10px;background-color:#3da3aa;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center;'>
    <div style='color:white;font-size:1.2em;'>Drop files here...</div>
    <div style='color:white;font-size:1.2em;margin-top:6px;'>... or click to select files</div>
  </div>
  <hr>
  
  <div style="margin-top:10px;">
  <!-- input invisible: se abrirá al hacer click en el área de drop -->
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
      for (var i = 0; i < fileList.length; i++) {
        formData.append('file', fileList[i], '/' + fileList[i].name);
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