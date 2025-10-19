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
<html lang='en'>

<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Upload</title>
</head>

<body style="width:300px">
  <h1>Upload</h1>
  <div><a href="/">Home</a></div>
  <hr>
  <div id='zone' style='width:16em;height:12em;padding:10px;background-color:#ddd'>Drop files here...</div>

  <!-- Simple file selector added -->
  <div style="margin-top:10px;">
    <input type="file" id="fileInput" />
    <button id="btnUpload">Upload</button>
  </div>

  <script>
    // allow drag&drop of file objects 
    function dragHelper(e) {
      e.stopPropagation();
      e.preventDefault();
    }

    // allow drag&drop of file objects 
    function dropped(e) {
      dragHelper(e);
      var fls = e.dataTransfer.files;
      uploadFiles(fls);
    }

    // Upload helper used by drag&drop and file input
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
        .then(function () { window.alert('done.'); })
        .catch(function (err) { window.alert('Upload failed'); console.error(err); });
    }

    document.getElementById('fileInput').addEventListener('change', function(e) {
      uploadFiles(e.target.files);
    });

    document.getElementById('btnUpload').addEventListener('click', function() {
      var inp = document.getElementById('fileInput');
      if (inp.files.length === 0) {
        alert('Select a file first.');
        return;
      }
      uploadFiles(inp.files);
    });

    var z = document.getElementById('zone');
    z.addEventListener('dragenter', dragHelper, false);
    z.addEventListener('dragover', dragHelper, false);
    z.addEventListener('drop', dropped, false);
  </script>
</body>
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
)==";