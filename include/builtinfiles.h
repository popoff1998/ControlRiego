/**
 * @file builtinfiles.h
 * @brief Basado en the WebServer example for the ESP8266WebServer.
 */

// used for $upload
static const char uploadContent[] PROGMEM =
R"==(
<!doctype html>
<html lang='es'>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Upload Fallback</title>
    <style>
        body{font-family:sans-serif;max-width:320px;margin:10px auto;padding:0 12px;color:#333}
        h1{color:#135a8a;margin:0 0 10px}
        a{text-decoration:none;font-size:14px;color:#135a8a}
        select, #z{width:100%;box-sizing:border-box;border-radius:5px;transition:.2s}
        select{padding:8px;margin:5px 0 15px;border:1px solid #ccc}
        .r{border-color:#d9534f;color:#d9534f;font-weight:700}
        #z{height:300px;border:2px dashed #3da3aa;background:#f9f9f9;display:flex;flex-direction:column;align-items:center;justify-content:center;cursor:pointer;text-align:center}
        #z.v{background:#3da3aa;color:#fff}
        .f{margin-top:15px;padding-top:10px;border-top:1px solid #eee}
        .l{color:#d9534f;font-weight:700;margin-left:5px}
    </style>
</head>
<body>
    <h1>Upload Files</h1>
    <a href="/">← Home</a>
    <div style="margin-top:15px">
        <label style="font-size:13px">Destino (opcional):</label>
        <select id="s"><option value="">(raíz)</option><option value="datos">datos</option></select>
    </div>
    <div id="z"><p>Arrastra archivos aquí<br>o haz clic</p></div>
    <input type="file" multiple id="i" style="display:none">
    <div class="f">
        <a href="/$update">OTA</a> | <a href="/datos/logError.txt" class="l">LOG</a>
    </div>
    <script>
        const z=document.getElementById('z'), i=document.getElementById('i'), s=document.getElementById('s');
        s.onchange=()=>{s.className=s.value?"r":""};
        ['dragenter','dragover'].forEach(e=>z.addEventListener(e,x=>{x.preventDefault();z.className='v'}));
        ['dragleave','drop'].forEach(e=>z.addEventListener(e,x=>{x.preventDefault();z.className=''}));
        z.onclick=()=>i.click();
        i.onchange=()=>up(i.files);
        z.ondrop=(e)=>up(e.dataTransfer.files);
        function up(f){
            if(!f.length)return;
            let d=new FormData(), p=s.value.replace(/^\/+|\/+$/g,'');
            for(let x of f) d.append('file',x,'/'+(p?p+'/':'')+x.name);
            z.innerHTML="<p>Subiendo...</p>";
            fetch('/',{method:'POST',body:d})
                .then(async r=>{alert(r.ok?'OK':await r.text());location.reload()})
                .catch(()=>alert('Error Red'));
        }
    </script>
</body>
</html>
)==";

// used for $upload
static const char notFoundContent[] PROGMEM = R"==(
<html>
<head>
  <title>Resource not found</title>
</head>
<body>
  <p>The resource was not found.</p>
  <p><a title="Go to INDEX page" href="/">Start again</a></p>
  <a style="color:#828282; font-size:0.9em; text-decoration:none;" title="Go to UPLOAD/OTA page" href="/$upload">or go to UPLOAD page</a>
</body>
</html>
)==";