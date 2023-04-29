const char initialsetup_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
    <title>LoRa Boat Monitor</title>
    <link rel='stylesheet' type='text/css' href='/styles.css'>
    <meta http-equiv='content-type' content='text/html; charset=UTF-8'>
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <style type='text/css'>
                    table {
                font-family: arial, sans-serif;
                border-collapse: collapse;
                width: 100%;
                }
                td, th {
                border: 1px solid #dddddd;
                text-align: left;
                    //padding: 8px;
                }
                tr:nth-child(even) {
                /*background-color: #dddddd;*/
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
            /*width: 300px;*/
        }

        input {
            width: 145px;
            /*width: 292px;*/
        }
    </style>
    
  </head><body>
  <h2>LoRa Boat Monitor</h2>
  %crights%, %fversion%, CQ: <data id='quality'></data> %
  <hr>
  <h3><data class="blink" id='info'></data></h3>

  <div id='loader'></div>
    <div style='display:none;' id='myDiv' class='animate-bottom'>
%wificonfig%
    %tabelle%
</br></br> Upload file:
<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>
<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>
<input type='file' name='upload'>
<input type='submit' value='Upload'>
</form>
<div id='prg'>progress: 0%</div>
<input type='submit' style='margin: 20px' value='Format FS' onclick='FormatFS()'>
<input type='submit' style='margin: 20px' value='Update Files' onclick='UpdateFiles()'>
<div id='status'>Status: </div>
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
        }
    
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
        }
    
    var myVar;
    var meinIntervall = null;
    const Http = new XMLHttpRequest();
        document.addEventListener('DOMContentLoaded', function() {
        setTimeout(function() {
            ////document.querySelector('body').classList.add('loaded');
            showPage();
        }, 10)
        });

        function showPage() {
            document.getElementById('loader').style.display = 'none';
            document.getElementById('myDiv').style.display = 'block';
        }

        function startInterval() {
            meinIntervall = setInterval(function() { 
                meineFunktion(); 
            }, 500);
        }

        function meineFunktion() { 
             $.ajax({
            url: '/updatefilesstatus',
            type: 'GET',               
            success:function(result) {    
                if (result == 0) {
                    clearInterval(meinIntervall);
                    document.getElementById('loader').style.display = 'none';
                    document.getElementById('myDiv').style.display = 'block';
                    //console.log('response = 0');
                    location.reload();
                } else if (result == 1) {
                    document.getElementById('loader').style.display = 'block';
                    document.getElementById('myDiv').style.display = 'none';
                    //console.log('response = 1');
                }
           },
            error: function (a, b, c) {
                $('#status').html('Status: Error while updatefilesstatus GET.');
                document.getElementById('loader').style.display = 'none';
                document.getElementById('myDiv').style.display = 'block';
            }
          });
        } 

/*$('form').submit(function(e){
    e.preventDefault();
      var form = $('#upload_form')[0];
      var data = new FormData(form);
       $.ajax({
            url: '/upload',
            type: 'POST',               
            data: data,
            contentType: false,                  
            processData:false,  
            xhr: function() {
                var xhr = new window.XMLHttpRequest();
                document.getElementById('loader').style.display = 'block';
                document.getElementById('myDiv').style.display = 'none';
                xhr.upload.addEventListener('progress', function(evt) {
                    if (evt.lengthComputable) {
                        var per = evt.loaded / evt.total;
                        $('#prg').html('progress: ' + Math.round(per*100) + '%');
                    }
               }, false);
               return xhr;
            },                                
            success:function(d, s) {    
                console.log('successully uploaded File!');
                document.getElementById('loader').style.display = 'none';
                document.getElementById('myDiv').style.display = 'block';
           },
            error: function (a, b, c) {
                $('#status').html('Status: Error while Uploading File.');
                document.getElementById('loader').style.display = 'none';
                document.getElementById('myDiv').style.display = 'block';
            }
          });
});*/
function FormatFS(){
      var form = $('#upload_form')[0];
      var data = new FormData(form);
       $.ajax({
            url: '/formatfs',
            type: 'POST',               
            data: data,
            contentType: false,                  
            processData:false,  
            xhr: function() {
                var xhr = new window.XMLHttpRequest();
                document.getElementById('loader').style.display = 'block';
                document.getElementById('myDiv').style.display = 'none';
                xhr.upload.addEventListener('progress', function(evt) {
                    if (evt.lengthComputable) {
                        var per = evt.loaded / evt.total;
                        $('#prg').html('progress: ' + Math.round(per*100) + '%');
                    }
               }, false);
               return xhr;
            },                                
            success:function(d, s) {    
                console.log('success!');
                $('#status').html('Status: Formating Filesystem successfull.');
                document.getElementById('loader').style.display = 'none';
                document.getElementById('myDiv').style.display = 'block';
                location.reload();
           },
            error: function (a, b, c) {
                $('#status').html('Status: Error while formating Filesystem.');
                document.getElementById('loader').style.display = 'none';
                document.getElementById('myDiv').style.display = 'block';
            }
          });
};
function UpdateFiles(){
      var form = $('#upload_form')[0];
      var data = new FormData(form);
       $.ajax({
            url: '/updatefiles',
            type: 'POST',               
            data: data,
            contentType: false,                  
            processData:false,  
            xhr: function() {
                var xhr = new window.XMLHttpRequest();
                document.getElementById('loader').style.display = 'block';
                document.getElementById('myDiv').style.display = 'none';
                startInterval();
                xhr.upload.addEventListener('progress', function(evt) {
                    if (evt.lengthComputable) {
                        var per = evt.loaded / evt.total;
                        $('#prg').html('progress: ' + Math.round(per*100) + '%');
                    }
               }, false);
               return xhr;
            },                                
            success:function(d, s) {    
                console.log('successully send POST to updatefiles!');
                //document.getElementById('loader').style.display = 'none';
                //document.getElementById('myDiv').style.display = 'block';
           },
            error: function (a, b, c) {
                $('#status').html('Status: Error while Downlaoding Files from server.');
                document.getElementById('loader').style.display = 'none';
                document.getElementById('myDiv').style.display = 'block';
            }
          });
};
</script>
</body></html>
)rawliteral";
