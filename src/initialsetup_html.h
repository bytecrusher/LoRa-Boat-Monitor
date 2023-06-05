const char initialsetup_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
    <title>Filesystem - %devname%</title>
    <link rel='stylesheet' type='text/css' href='/styles.css'>
    <meta http-equiv='content-type' content='text/html; charset=UTF-8'>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style type='text/css'>
        /* General text definitions */
        body {
            color: rgb(200, 200, 200);
            font-family: arial;
            /*padding: 50px;
            background-color: #222;*/
        }

        /* Background definitions */
        body {
            background-color: rgb(32, 32, 32);
            background-image: linear-gradient(45deg, black 25%, transparent 25%, transparent 75%, black 75%, black),
                linear-gradient(45deg, black 25%, transparent 25%, transparent 75%, black 75%, black),
                linear-gradient(to bottom, rgb(8, 8, 8), rgb(32, 32, 32));
            background-size: 10px 10px, 10px 10px, 10px 5px;
            background-position: 0px 0px, 5px 5px, 0px 0px;
        }

        /* Text color definitions for head letters */
        a {
            color: rgb(255, 255, 255);
        }

        h1 {
            color: rgb(255, 255, 255);
        }

        h2 {
            color: rgb(255, 255, 255);
        }

        h3 {
            color: rgb(255, 255, 255);
        }

        h4 {
            color: rgb(255, 255, 255);
        }

        h5 {
            color: rgb(255, 255, 255);
        }

        h6 {
            color: rgb(255, 255, 255);
        }

        h7 {
            color: rgb(255, 255, 255);
        }

        table {
            font-family: arial, sans-serif;
            border-collapse: collapse;
            width: 100%;
        }
            td, th {
            border: 1px solid #dddddd;
            text-align: left;
            /*padding: 8px;*/
        }

        /* Center the loader */
        #loader {
            position: absolute;
            left: 50%;
            top: 50%;
            z-index: 1;
            width: 120px;
            height: 120px;
            margin: -76px 0 0 -76px;
            border: 16px solid #f3f3f3;
            border-radius: 50%;
            border-top: 16px solid #3498db;
            -webkit-animation: spin 2s linear infinite;
            animation: spin 2s linear infinite;
        }
        
        @-webkit-keyframes spin {
            0% { -webkit-transform: rotate(0deg); }
            100% { -webkit-transform: rotate(360deg); }
        }
        
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        
        /* Add animation to 'page content' */
        .animate-bottom {
            position: relative;
            -webkit-animation-name: animatebottom;
            -webkit-animation-duration: 1s;
            animation-name: animatebottom;
            animation-duration: 1s
        }
        
        @-webkit-keyframes animatebottom {
            from { bottom:-100px; opacity:0 } 
            to { bottom:0px; opacity:1 }
        }
        
        @keyframes animatebottom { 
            from{ bottom:-100px; opacity:0 } 
            to{ bottom:0; opacity:1 }
        }
        
        #myDiv {
            display: none;
            /*text-align: center;*/
        }

        select {
            width: 150px;
        }

        input {
            width: 145px;
        }

        .led {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background-color: rgba(255, 255, 255, 0.25);
            box-shadow: #000 0 -1px 6px 1px;
        }
        .led-red {
            background-color: #F00;
            box-shadow: #000 0 -1px 6px 1px, inset #600 0 -1px 8px, #F00 0 3px 11px;
        }

        .led-green {
            background-color: #80FF00;
            box-shadow: #000 0 -1px 6px 1px, inset #460 0 -1px 8px, #80FF00 0 3px 11px;
        }

        .box { float: left; }
        .box:last-child { margin-right: 0; }
    </style>
  </head><body>
  <div style="float: left; width: 100%">
    <div class="box"><h2>%devname%</h2></div>
    <div class="box" style="margin-top: 30px; margin-left: 20px;">
        <div id='myping' class="led"></div>
    </div>
</div>
<div>%crights%, %fversion%, CQ: <data id='quality'>%quality%</data>%</div>
  <hr>
  <h3><data class="blink" id='info'></data></h3>

    <div id='loader'></div>
    <div style='display:none;' id='myDiv' class='animate-bottom'>
        %wificonfig%
        <p><progress id="file" value="%USEDSPIFFSvalue%" max="%TOTALSPIFFSvalue%" style="margin-right: 10px"> %USEDSPIFFSvalue%% </progress> Free: <span id="freespiffs">%FREESPIFFS%</span> | Used: <span id="usedspiffs">%USEDSPIFFS%</span> | Total: <span id="totalspiffs">%TOTALSPIFFS%</span></p>
        <div id='table'></div>
        <button type='button' style='margin: 20px' value='Format FS' onclick='FormatFS()'>Format Filesystem</button>
        <button type='button' value='Update Files' onclick='UpdateFiles()'>Get Files from Server</button>
        <button type='button' style='margin: 20px' value='Format FS' onclick='getTable()'>Show Files</button>
        <div id='status' style="border: 1px solid white; padding: 5px;">Status: </div>
        <br>
        <div style="border: 1px solid white; padding: 5px;">
            Upload single file:
            <form method='POST' action='/upload' enctype='multipart/form-data' id='upload_form'>
                <input type='file' name='upload' style='width: 300px'>
                <button type='submit' value='Upload'>Upload</button>
            </form>
        </div>
        <br>
        <form action='/'><button type='submit'>Back</button></form>
    </div>
<script>
    function check_ssid(iname) {
        var valuestring = "";
        if (iname == "cssid") { valuestring = document.wifiform.cssid.value; }
        var reguexp = /[^A-z0-9_-]/;
        if (reguexp.exec(valuestring) || valuestring.length < 1 || valuestring.length > 20) {
            document.getElementById('sub').disabled = true;
            alert('Error!\\nUse only a-z, A-Z, 0-9, _-\\nSSID Length 1-20');
        }
        else {
            document.getElementById('sub').disabled = false;
        }
    };
    
    function check_passwd(iname) {
        var valuestring = "";
        if (iname == "cpasswd") { valuestring = document.wifiform.cpasswd.value; }
        var reguexp = /[^A-z0-9\-]/;
        if (reguexp.exec(valuestring) || valuestring.length < 8 || valuestring.length > 20) {
            document.getElementById('sub').disabled = true;
            alert('Error!\\nUse only a-z, A-Z, 0-9, ' - '\\nPassword Length 8-20');
        }
        else {
            document.getElementById('sub').disabled = false;
        }
    };
    
    var myVar;
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
            document.getElementById('loader').style.display = 'block';
            document.getElementById('myDiv').style.display = 'none';
            http.open("GET", "/updatefilesstatus", true);
            http.onreadystatechange = ausgeben;
            http.send(null);
        }

        function ausgeben() {
            if (http.readyState == XMLHttpRequest.DONE) {
                if (http.status == 200) {
                    result = http.responseText;
                    if (result == 0) {
                        clearInterval(meinIntervall);
                        document.getElementById('loader').style.display = 'none';
                        document.getElementById('myDiv').style.display = 'block';
                        document.getElementById('status').innerHTML = ('Status: Updated Files successfull.');
                        alert('Files successfully downloaded.');
                        location.reload();
                    } else if (result == 1) {
                        document.getElementById('loader').style.display = 'block';
                        document.getElementById('myDiv').style.display = 'none';
                    }
                }
                else if (http.status == 400) {
                    alert('There was an error 400');
                    document.getElementById('status').innerHTML = ('Status: Error while updatefilesstatus GET.');
                    document.getElementById('loader').style.display = 'none';
                    document.getElementById('myDiv').style.display = 'block';
                    }
                else {
                    alert('something else other than 200 was returned');
                    document.getElementById('status').innerHTML = ('Status: Error while updatefilesstatus GET.');
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
                http.onreadystatechange = ausgeben;
                http.send(null);
            }

            function ausgeben() {
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
                        alert('There was an error 400');
                        document.getElementById('status').innerHTML = ('Status: Error while Formating Filesystem.');
                        document.getElementById('loader').style.display = 'none';
                        document.getElementById('myDiv').style.display = 'block';
                    }
                    else {
                        alert('something else other than 200 was returned');
                        document.getElementById('status').innerHTML = ('Status: Error while Formating Filesystem.');
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
            document.getElementById('loader').style.display = 'block';
            document.getElementById('myDiv').style.display = 'none';
            startInterval();
            http.open("POST", "/updatefiles", true);
            http.onreadystatechange = ausgeben;
            http.send(null);
        }

        function ausgeben() {
            if (http.readyState == XMLHttpRequest.DONE) {                
                if (http.status == 200) {
                    //document.getElementById("myDiv").innerHTML = xmlhttp.responseText;
                    console.log('Update Files requested...');
                    document.getElementById('status').innerHTML = ('Status: Update Files requested...');
                }
                else if (http.status == 400) {
                    alert('There was an error 400');
                    document.getElementById('status').innerHTML = ('Status: Error while Downlaoding Files from server.');
                    document.getElementById('loader').style.display = 'none';
                    document.getElementById('myDiv').style.display = 'block';
                }
                else {
                    alert('something else other than 200 was returned');
                    document.getElementById('status').innerHTML = ('Status: Error while Downlaoding Files from server.');
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
            http.onreadystatechange = ausgeben;
            http.send(null);
        }

        function ausgeben() {
            if (http.readyState == XMLHttpRequest.DONE) {                
                if (http.status == 200) {
                    document.getElementById('loader').style.display = 'none';
                    document.getElementById('myDiv').style.display = 'block';
                    document.getElementById("table").innerHTML = http.responseText;
                    console.log('getTable successfull.');
                    document.getElementById('status').innerHTML = ('Status: getTable successfull');
                }
                else if (http.status == 400) {
                    alert('There was an error 400');
                    document.getElementById('status').innerHTML = ('Status: Error while getTable.');
                    document.getElementById('loader').style.display = 'none';
                    document.getElementById('myDiv').style.display = 'block';
                }
                else {
                    alert('something else other than 200 was returned');
                    document.getElementById('status').innerHTML = ('Status: Error while getTable.');
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
            document.getElementById("myping").innerHTML = "";
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
            document.getElementById("myping").innerHTML = "<div style='margin-left:20px;'>Offline</div>";
            }, 3000);
        }
        xmlhttpheader.open('GET', '/getdata', true);
        xmlhttpheader.send();
    }
    setInterval(function () { startping(); }, 1000);
</script>
</body></html>
)rawliteral";
