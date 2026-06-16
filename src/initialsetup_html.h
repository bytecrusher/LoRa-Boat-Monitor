const char initialsetup_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
    <title>Filesystem - %devname%</title>
    <link rel='stylesheet' type='text/css' href='/styles.css'>
    <link rel='stylesheet' type='text/css' href='/common.css'>
    <meta http-equiv='content-type' content='text/html; charset=UTF-8'>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name='csrf-token' content='%csrfToken%'>
    <style type='text/css'>
        input[type="file"] {
            display: none;
        }
    </style>
  </head><body class="file-manager-page">
    <div class="header-layout">
      <div class="header-title-group">
        <h2>%devname%</h2>
      </div>
      <div class="header-status-group">
        <div id='myping' class="led"></div>
        <span class="header-status-text">Device reachability</span>
      </div>
    </div>
    <div class="header-meta">%crights%, %fversion%, CQ: <data id='quality'>%quality%</data>%</div>
    <div class="notice hidden" id="infoNotice"><data class="blink" id='info'></data></div>

    <div id='loader'></div>
    <div style='display:none;' id='myDiv' class='animate-bottom file-manager-shell'>
        <section class="page-section">
            <div class="page-hero">
                <h3>File Manager</h3>
                <p class="muted-text">Manage the local LittleFS storage on the device. Web interface files should match the installed firmware version.</p>
            </div>
            %wificonfig%
            <div class="metric-grid">
                <article class="metric-card">
                    <span class="status-label">Free</span>
                    <strong id="freespiffs">%FREESPIFFS%</strong>
                </article>
                <article class="metric-card">
                    <span class="status-label">Used</span>
                    <strong id="usedspiffs">%USEDSPIFFS%</strong>
                </article>
                <article class="metric-card">
                    <span class="status-label">Total</span>
                    <strong id="totalspiffs">%TOTALSPIFFS%</strong>
                </article>
            </div>
            <div class="section-spacer progress-inline">
                <div class="progress-track">
                    <div class="progress-fill" id="filesystemUsageBar" style="width:0%;"></div>
                </div>
                <div class="progress-text">Used storage: <span id="filesystemUsageText">0%</span></div>
                <progress class="visually-hidden" id="file" value="%USEDSPIFFSvalue%" max="%TOTALSPIFFSvalue%"> %USEDSPIFFSvalue%% </progress>
            </div>
        </section>

        <section class="page-section">
            <div class="table-toolbar">
                <div>
                    <h4>Stored Files</h4>
                    <p class="muted-text">Load the current file list from the device when you need it.</p>
                </div>
                <div class="button-group">
                    <button type='button' class="button-secondary" onclick='getTable()'>Show Files</button>
                </div>
            </div>
            <div id='table' class='file-table-shell'></div>
        </section>

        <section class="page-section toolbar-stack">
            <div class="table-toolbar">
                <div>
                    <h4>Maintenance</h4>
                    <p class="muted-text">Use the update action to download web files that match the installed firmware. Formatting clears the whole file system.</p>
                </div>
            </div>
            <div class="toolbar-row">
                <button type='button' id='updatefilesbutton' class="button-primary" value='Update Files' onclick='UpdateFiles()'>Get Files from Server</button>
                <button type='button' class="button-danger" value='Format FS' onclick='FormatFS()'>Format Filesystem</button>
                <form method='POST' action='/upload' enctype='multipart/form-data' id='upload_form' class="inline-upload-form">
                    <input type='hidden' name='csrf' value='%csrfToken%'>
                    <label for='upload' class='custom-file-upload'>Upload Single File</label>
                    <input type='file' id='upload' name='upload' onchange='uploadConfig();'>
                </form>
                <button type='button' class="button-secondary" onclick="window.open('/', '_self');">Back to Overview</button>
            </div>
            <div id='updatefilesinfo' class="muted-text"></div>
            <div class="progress-inline">
                <div id="updatefilesprogress" style="display:none;">
                    <div class="progress-track">
                        <div id="updatefilesprogressfill" class="progress-fill" style="width:0%;"></div>
                    </div>
                </div>
                <progress id="updatefilesprogressbar" class="visually-hidden" value="0" max="100">0%</progress>
                <span id="updatefilesprogresstext" class="progress-text"></span>
            </div>
        </section>
    </div>
<script>
    function check_ssid(iname) {
        var valuestring = "";
        if (iname == "cssid1") { valuestring = document.wifiform.cssid1.value; }
        if (iname == "cssid2") { valuestring = document.wifiform.cssid2.value; }
        if (iname == "cssid3") { valuestring = document.wifiform.cssid3.value; }
        var reguexp = /[^\x20-\x7E]/;
        if (reguexp.exec(valuestring) || valuestring.length < 1 || valuestring.length > 30) {
            document.getElementById('sub').disabled = true;
            alert("Error!\nUse only printable characters.\nSSID Length 1-30");
        }
        else {
            document.getElementById('sub').disabled = false;
        }
    };
    
    function check_passwd(iname) {
        var valuestring = "";
        if (iname == "cpasswd1") { valuestring = document.wifiform.cpasswd1.value; }
        if (iname == "cpasswd2") { valuestring = document.wifiform.cpasswd2.value; }
        if (iname == "cpasswd3") { valuestring = document.wifiform.cpasswd3.value; }
        var reguexp = /[^\x20-\x7E]/;
        if (reguexp.exec(valuestring) || valuestring.length < 8 || valuestring.length > 30) {
            document.getElementById('sub').disabled = true;
            alert("Error!\nUse only printable characters.\nPassword Length 8-30");
        }
        else {
            document.getElementById('sub').disabled = false;
        }
    };
    
    var meinIntervall = null;
    const Http = new XMLHttpRequest();
    document.addEventListener('DOMContentLoaded', function() {
        setTimeout(function() {
            showPage();
        }, 10)
    });

    function showPage() {
        document.getElementById('loader').style.display = 'none';
        document.getElementById('myDiv').style.display = 'block';
        updateFilesystemUsage();
        refreshUpdateFilesInfo();
    };

    function updateFilesystemUsage() {
        var progress = document.getElementById('file');
        var fill = document.getElementById('filesystemUsageBar');
        var text = document.getElementById('filesystemUsageText');
        if (!progress || !fill || !text) {
            return;
        }

        var used = Number(progress.value || 0);
        var total = Number(progress.max || 0);
        var percent = total > 0 ? Math.min(100, Math.max(0, Math.round((used / total) * 100))) : 0;
        fill.style.width = percent + '%';
        text.textContent = percent + '%';
    }

    function renderUpdateFilesInfo(info) {
        var button = document.getElementById('updatefilesbutton');
        var status = document.getElementById('updatefilesinfo');
        var progressBar = document.getElementById('updatefilesprogressbar');
        var progressText = document.getElementById('updatefilesprogresstext');
        var progressWrap = document.getElementById('updatefilesprogress');
        var progressFill = document.getElementById('updatefilesprogressfill');
        if (!button || !status || !info) {
            return;
        }

        if (info.busy) {
            button.textContent = 'Downloading Files from Server...';
            button.disabled = true;
            status.textContent = 'Web files are currently being downloaded for firmware ' + info.firmwareVersion + '.';
            if (progressBar && progressText && progressWrap && progressFill) {
                progressWrap.style.display = 'block';
                progressBar.value = info.percent || 0;
                progressFill.style.width = (info.percent || 0) + '%';
                progressText.textContent = (info.percent || 0) + '%';
                if (info.currentFile && info.currentFile.length > 0) {
                    progressText.textContent += ' - ' + info.currentFile + ' (' + info.completed + '/' + info.total + ')';
                }
            }
            return;
        }

        button.disabled = false;
        if (progressBar && progressText && progressWrap && progressFill) {
            progressWrap.style.display = 'none';
            progressBar.value = info.percent || 0;
            progressFill.style.width = (info.percent || 0) + '%';
            progressText.textContent = '';
        }
        if (info.upToDate) {
            button.textContent = 'Get Files from Server';
            status.textContent = 'Web files are up to date for installed firmware ' + info.firmwareVersion + '.';
            return;
        }

        button.textContent = 'Get Files from Server';
        if (info.storedWebFilesVersion && info.storedWebFilesVersion.length > 0) {
            status.textContent = 'Installed firmware: ' + info.firmwareVersion + ' | Stored web files: ' + info.storedWebFilesVersion + ' | Download recommended.';
        } else {
            status.textContent = 'No stored web file version found. Download recommended for firmware ' + info.firmwareVersion + '.';
        }
    };

    function csrfToken() {
        var meta = document.querySelector('meta[name="csrf-token"]');
        return meta ? meta.getAttribute('content') : '';
    }

    function addCsrfHeader(http) {
        var token = csrfToken();
        if (token) {
            http.setRequestHeader('X-CSRF-Token', token);
        }
    }

    function refreshUpdateFilesInfo() {
        var http = null;
        if (window.XMLHttpRequest) {
            http = new XMLHttpRequest();
        } else if (window.ActiveXObject) {
            http = new ActiveXObject("Microsoft.XMLHTTP");
        }
        if (http != null) {
            http.open("GET", "/updatefilesprogress", true);
            http.onreadystatechange = function() {
                if (http.readyState == XMLHttpRequest.DONE && http.status == 200) {
                    try {
                        renderUpdateFilesInfo(JSON.parse(http.responseText));
                    } catch (error) {
                        console.log('Unable to parse update file info.', error);
                    }
                }
            };
            http.send(null);
        }
    };

    function startInterval() {
        meinIntervall = setInterval(function() { 
            meineFunktion(); 
        }, 500);
    };

    function meineFunktion() {
        var http = null;
        if (window.XMLHttpRequest) {
            http = new XMLHttpRequest();
        } else if (window.ActiveXObject) {
            http = new ActiveXObject("Microsoft.XMLHTTP");
        }
        if (http != null) {
            refreshUpdateFilesInfo();
            http.open("GET", "/updatefilesstatus", true);
            http.onreadystatechange = meineFunktionAusgeben;
            http.send(null);
        }

        function meineFunktionAusgeben() {
            if (http.readyState == XMLHttpRequest.DONE) {
                if (http.status == 200) {
                    result = http.responseText;
                    if (result == 0) {
                        clearInterval(meinIntervall);
                        refreshUpdateFilesInfo();
                        //document.getElementById('status').innerHTML = ('Status: Updated Files successfull.');
                        alert('Files successfully downloaded.');
                        location.reload();
                    } else if (result == 1) {
                        refreshUpdateFilesInfo();
                    }
                }
                else if (http.status == 400) {
                    alert('There was an error 400');
                    //document.getElementById('status').innerHTML = ('Status: Error while updatefilesstatus GET.');
                    refreshUpdateFilesInfo();
                    alert('Files not successfully downloaded.');
                    }
                else {
                    alert('something else other than 200 was returned');
                    //document.getElementById('status').innerHTML = ('Status: Error while updatefilesstatus GET.');
                    refreshUpdateFilesInfo();
                }
            }
        }
    };

    function FormatFS(){
        const response = confirm("Are you sure you want Format the Filesystem?");
        if (response) {
            var http = null;
            if (window.XMLHttpRequest) {
                http = new XMLHttpRequest();
            } else if (window.ActiveXObject) {
                http = new ActiveXObject("Microsoft.XMLHTTP");
            }
            if (http != null) {
                document.getElementById('loader').style.display = 'block';
                document.getElementById('myDiv').style.display = 'none';
                http.open("POST", "/formatfs", true);
                addCsrfHeader(http);
                http.onreadystatechange = FormatFSAusgeben;
                http.send(null);
            }

            function FormatFSAusgeben() {
                if (http.readyState == XMLHttpRequest.DONE) {
                    if (http.status == 200) {
                        //document.getElementById("myDiv").innerHTML = xmlhttp.responseText;
                        console.log('success!');
                        //document.getElementById('status').innerHTML = ('Status: Formating Filesystem successfull.');
                        document.getElementById('loader').style.display = 'none';
                        document.getElementById('myDiv').style.display = 'block';
                        alert('Formating Filesystem successfull.');
                        location.reload();
                    }
                    else if (http.status == 400) {
                        //alert('There was an error 400');
                        //document.getElementById('status').innerHTML = ('Status: Error while Formating Filesystem.');
                        document.getElementById('loader').style.display = 'none';
                        document.getElementById('myDiv').style.display = 'block';
                        alert('Error while Formating Filesystem. (Error 400)');
                    }
                    else {
                        //alert('something else other than 200 was returned');
                        //document.getElementById('status').innerHTML = ('Status: Error while Formating Filesystem.');
                        alert('Error while Formating Filesystem. (Error 200)');
                    }
                }
            }
        }
    };

    function UpdateFiles(){
        var http = null;
        if (window.XMLHttpRequest) {
            http = new XMLHttpRequest();
        } else if (window.ActiveXObject) {
            http = new ActiveXObject("Microsoft.XMLHTTP");
        }
        if (http != null) {
            renderUpdateFilesInfo({ busy: true, firmwareVersion: '%fversion%', storedWebFilesVersion: '', upToDate: false });
            startInterval();
            http.open("POST", "/updatefiles", true);
            addCsrfHeader(http);
            http.onreadystatechange = UpdateFilesAusgeben;
            http.send(null);
        }

        function UpdateFilesAusgeben() {
            if (http.readyState == XMLHttpRequest.DONE) {                
                if (http.status == 200) {
                    //document.getElementById("myDiv").innerHTML = xmlhttp.responseText;
                    console.log('Update Files requested...');
                    //document.getElementById('status').innerHTML = ('Status: Update Files requested...');
                }
                else if (http.status == 400) {
                    //alert('There was an error 400');
                    //document.getElementById('status').innerHTML = ('Status: Error while Downlaoding Files from server.');
                    refreshUpdateFilesInfo();
                    alert('Error while Downlaoding Files from server. (Error 400)');
                }
                else {
                    alert('something else other than 200 was returned');
                    //document.getElementById('status').innerHTML = ('Status: Error while Downlaoding Files from server.');
                    refreshUpdateFilesInfo();
                    alert('Error while Downlaoding Files from server. (Error 200)');
                }
            }
        }
    };

    function getTable(){
        var http = null;
        if (window.XMLHttpRequest) {
            http = new XMLHttpRequest();
        } else if (window.ActiveXObject) {
            http = new ActiveXObject("Microsoft.XMLHTTP");
        }
        if (http != null) {
            document.getElementById('loader').style.display = 'block';
            document.getElementById('myDiv').style.display = 'none';
            //startInterval();
            http.open("GET", "/gettable", true);
            http.onreadystatechange = getTableAusgeben;
            http.send(null);
        }

        function getTableAusgeben() {
            if (http.readyState == XMLHttpRequest.DONE) {                
                if (http.status == 200) {
                    document.getElementById('loader').style.display = 'none';
                    document.getElementById('myDiv').style.display = 'block';
                    document.getElementById("table").innerHTML = http.responseText;
                    console.log('getTable successfull.');
                    //document.getElementById('status').innerHTML = ('Status: getTable successfull');
                }
                else if (http.status == 400) {
                    //document.getElementById('status').innerHTML = ('Status: Error while getTable.');
                    document.getElementById('loader').style.display = 'none';
                    document.getElementById('myDiv').style.display = 'block';
                    alert('Error while getTable. (Error 400)');
                }
                else {
                    alert('something else other than 200 was returned');
                    //document.getElementById('status').innerHTML = ('Status: Error while getTable.');
                    alert('Error while getTable. (Error 200)');
                }
            }
        }
    };

    var xmlhttpheader = new XMLHttpRequest();
    let offlineTimer = 0;
    xmlhttpheader.onreadystatechange = function () {
        // Turn on the green led.
        if (this.readyState == 4 && this.status == 200) {
            clearTimeout(offlineTimer);
            offlineTimer = 0;
            document.getElementById("myping").classList.add('led-green');
            document.getElementById("myping").classList.remove('led-red');
            document.getElementById("myping").textContent = "";
            // Turn off the green led.
            setTimeout(function () { 
                document.getElementById("myping").classList.remove('led-green');
            }, 400);
        }
    };
    function startping() {
        // avoid more than one instance of "offlineTimer".
        if (offlineTimer == 0) {
            // When after 3 seconds no response.
            offlineTimer = setTimeout(function () { 
            document.getElementById("myping").classList.add('led-red');
            document.getElementById("myping").classList.remove('led-green');
            document.getElementById("myping").textContent = "Offline";
            }, 3000);
        }
        xmlhttpheader.open('GET', '/getdata', true);
        xmlhttpheader.send();
    }
    setInterval(function () { startping(); }, 1000);

    function uploadConfig() {
        document.getElementById('upload_form').submit();
    }
</script>
</body></html>
)rawliteral";
