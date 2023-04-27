
#ifndef filesystem_h
#define filesystem_h

/* You only need to format LittleFS the first time you run a
   test or else use the LITTLEFS plugin to create a partition
   https://github.com/lorol/arduino-esp32littlefs-plugin */
   
#define FORMAT_LITTLEFS_IF_FAILED true

String serverIndex = String("<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>") +
String("<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>") +
    String("<input type='file' name='upload'>") +
    String("<input type='submit' value='Upload'>") +
"</form>"
"<div id='prg'>progress: 0%</div>"
"<input type='submit' style='margin: 20px' value='Format FS' onclick='FormatFS()'>"
"<input type='submit' style='margin: 20px' value='Update Files' onclick='UpdateFiles()'>"
"<div id='status'>Status: </div>"
"<script>"
    "var myVar;"
    "var meinIntervall = null;"
    "const Http = new XMLHttpRequest();"
        "document.addEventListener('DOMContentLoaded', function() {"
        "setTimeout(function() {"
            //"//document.querySelector('body').classList.add('loaded');"
            "showPage();"
        "}, 10)"
        "});"

        "function showPage() {"
            "document.getElementById('loader').style.display = 'none';"
            "document.getElementById('myDiv').style.display = 'block';"
        "}"

        "function startInterval() {"
            "meinIntervall = setInterval(function() { "
                "meineFunktion(); "
            "}, 500);"
        "}"

        "function meineFunktion() { "
            " $.ajax({"
            "url: '/updatefilesstatus',"
            "type: 'GET',"               
            "success:function(result) {"    
                "if (result == 0) {"
                    "clearInterval(meinIntervall);"
                    "document.getElementById('loader').style.display = 'none';"
                    "document.getElementById('myDiv').style.display = 'block';"
                    "console.log('response = 0');"
                "} else if (result == 1) {"
                    "document.getElementById('loader').style.display = 'block';"
                    "document.getElementById('myDiv').style.display = 'none';"
                    "console.log('response = 1');"
                "}"
           "},"
            "error: function (a, b, c) {"
                "$('#status').html('Status: Error while updatefilesstatus GET.');"
                "document.getElementById('loader').style.display = 'none';"
                "document.getElementById('myDiv').style.display = 'block';"
            "}"
          "});"
        "} "

"$('form').submit(function(e){"
    "e.preventDefault();"
      "var form = $('#upload_form')[0];"
      "var data = new FormData(form);"
      " $.ajax({"
            "url: '/upload',"
            "type: 'POST',"               
            "data: data,"
            "contentType: false,"                  
            "processData:false,"  
            "xhr: function() {"
                "var xhr = new window.XMLHttpRequest();"
                "document.getElementById('loader').style.display = 'block';"
                "document.getElementById('myDiv').style.display = 'none';"
                "xhr.upload.addEventListener('progress', function(evt) {"
                    "if (evt.lengthComputable) {"
                        "var per = evt.loaded / evt.total;"
                        "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
                    "}"
               "}, false);"
               "return xhr;"
            "},"                                
            "success:function(d, s) {"    
                "console.log('successully uploaded File!');"
                "document.getElementById('loader').style.display = 'none';"
                "document.getElementById('myDiv').style.display = 'block';"
           "},"
            "error: function (a, b, c) {"
                "$('#status').html('Status: Error while Uploading File.');"
                "document.getElementById('loader').style.display = 'none';"
                "document.getElementById('myDiv').style.display = 'block';"
            "}"
          "});"
"});"
"function FormatFS(){"
      "var form = $('#upload_form')[0];"
      "var data = new FormData(form);"
      " $.ajax({"
            "url: '/formatfs',"
            "type: 'POST',"               
            "data: data,"
            "contentType: false,"                  
            "processData:false,"  
            "xhr: function() {"
                "var xhr = new window.XMLHttpRequest();"
                "document.getElementById('loader').style.display = 'block';"
                "document.getElementById('myDiv').style.display = 'none';"
                "xhr.upload.addEventListener('progress', function(evt) {"
                    "if (evt.lengthComputable) {"
                        "var per = evt.loaded / evt.total;"
                        "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
                    "}"
               "}, false);"
               "return xhr;"
            "},"                                
            "success:function(d, s) {"    
                "console.log('success!');"
                "$('#status').html('Status: Formating Filesystem successfull.');"
                "document.getElementById('loader').style.display = 'none';"
                "document.getElementById('myDiv').style.display = 'block';"
           "},"
            "error: function (a, b, c) {"
                "$('#status').html('Status: Error while formating Filesystem.');"
                "document.getElementById('loader').style.display = 'none';"
                "document.getElementById('myDiv').style.display = 'block';"
            "}"
          "});"
"};"
"function UpdateFiles(){"
      "var form = $('#upload_form')[0];"
      "var data = new FormData(form);"
      " $.ajax({"
            "url: '/updatefiles',"
            "type: 'POST',"               
            "data: data,"
            "contentType: false,"                  
            "processData:false,"  
            "xhr: function() {"
                "var xhr = new window.XMLHttpRequest();"
                "document.getElementById('loader').style.display = 'block';"
                "document.getElementById('myDiv').style.display = 'none';"
                "startInterval();"
                "xhr.upload.addEventListener('progress', function(evt) {"
                    "if (evt.lengthComputable) {"
                        "var per = evt.loaded / evt.total;"
                        "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
                    "}"
               "}, false);"
               "return xhr;"
            "},"                                
            "success:function(d, s) {"    
                "console.log('successully send POST to updatefiles!');"
                //"document.getElementById('loader').style.display = 'none';"
                //"document.getElementById('myDiv').style.display = 'block';"
           "},"
            "error: function (a, b, c) {"
                "$('#status').html('Status: Error while Downlaoding Files from server.');"
                "document.getElementById('loader').style.display = 'none';"
                "document.getElementById('myDiv').style.display = 'block';"
            "}"
          "});"
"};"
"</script>"
"<form action='/'><button type='submit'>Back</button></form>";

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");

            Serial.print(file.name());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);

            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");

            Serial.print(file.size());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

String readFile2(fs::FS &fs, const char * path){
    //Serial.printf("Reading file: %s\r\n", path);
    String filecontent = "";
    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return "- failed to open file for reading";
    }

    Serial.print("- read from file (readfile2): ");
    Serial.println(path);
    while(file.available()){
        //filecontent = file.read();
        filecontent+=String((char)file.read());
        //Serial.write(file.read());
    }
    file.close();
    return filecontent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("- message appended");
    } else {
        Serial.println("- append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("- file renamed");
    } else {
        Serial.println("- rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}

// SPIFFS-like write and delete file

// See: https://github.com/esp8266/Arduino/blob/master/libraries/LittleFS/src/LittleFS.cpp#L60
void writeFile2(fs::FS &fs, const char * path, const char * message){
    if(!fs.exists(path)){
		if (strchr(path, '/')) {
            Serial.printf("Create missing folders of: %s\r\n", path);
			char *pathStr = strdup(path);
			if (pathStr) {
				char *ptr = strchr(pathStr, '/');
				while (ptr) {
					*ptr = 0;
					fs.mkdir(pathStr);
					*ptr = '/';
					ptr = strchr(ptr+1, '/');
				}
			}
			free(pathStr);
		}
    }

    Serial.printf("Writing file to: %s\r\n", path);
    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}

// See:  https://github.com/esp8266/Arduino/blob/master/libraries/LittleFS/src/LittleFS.h#L149
void deleteFile2(fs::FS &fs, const char * path){
    Serial.printf("Deleting file and empty folders on path: %s\r\n", path);

    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }

    char *pathStr = strdup(path);
    if (pathStr) {
        char *ptr = strrchr(pathStr, '/');
        if (ptr) {
            Serial.printf("Removing all empty folders on path: %s\r\n", path);
        }
        while (ptr) {
            *ptr = 0;
            fs.rmdir(pathStr);
            ptr = strrchr(pathStr, '/');
        }
        free(pathStr);
    }
}

void testFileIO(fs::FS &fs, const char * path){
    Serial.printf("Testing file I/O with %s\r\n", path);

    static uint8_t buf[512];
    size_t len = 0;
    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }

    size_t i;
    Serial.print("- writing" );
    uint32_t start = millis();
    for(i=0; i<2048; i++){
        if ((i & 0x001F) == 0x001F){
          Serial.print(".");
        }
        file.write(buf, 512);
    }
    Serial.println("");
    uint32_t end = millis() - start;
    Serial.printf(" - %u bytes written in %u ms\r\n", 2048 * 512, end);
    file.close();

    file = fs.open(path);
    start = millis();
    end = start;
    i = 0;
    if(file && !file.isDirectory()){
        len = file.size();
        size_t flen = len;
        start = millis();
        Serial.print("- reading" );
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            if ((i++ & 0x001F) == 0x001F){
              Serial.print(".");
            }
            len -= toRead;
        }
        Serial.println("");
        end = millis() - start;
        Serial.printf("- %u bytes read in %u ms\r\n", flen, end);
        file.close();
    } else {
        Serial.println("- failed to open file for reading");
    }
}

//String printDirectory(File dir, int numTabs) {
/*String printDirectory(fs::FS &fs, int numTabs) {
  String response = "";
  String dirname = "/";
  File dir = fs.open(dirname);
  while(true) {
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');   // we'll have a nice indentation
     }
     // Recurse for directories, otherwise print the file size
     if (entry.isDirectory()) {
       printDirectory(fs, numTabs+1);
     } else {
       response += String("<a href='") + String(entry.name()) + String("'>") + String(entry.name()) + String("</a>") + String("</br>");
     }
     entry.close();
   }
   return String("List files:</br>") + response + String("</br></br> Upload file:") + serverIndex;
}*/

String handleRoot(fs::FS &fs, const char * dirname, uint8_t levels){
    String response = String("<!DOCTYPE html>") +
    String("<html>") +
    String("<head>") +
    String("<title>LoRa Boat Monitor</title>") +
    String("<link rel='stylesheet' type='text/css' href='/css'>") +
    String("<meta http-equiv='content-type' content='text/html; charset=UTF-8'>") +
    String("<meta name=viewport content='width=device-width, initial-scale=1'>");
  
    //response += "<link rel='stylesheet' type='text/css' href='/css'>";
    response += String("<style>") +
                    String("table {") +
                    String("font-family: arial, sans-serif;") +
                    String("border-collapse: collapse;") +
                    String("width: 100%;") +
                    String("}") +
                    String("td, th {") +
                    String("border: 1px solid #dddddd;") +
                    String("text-align: left;") +
                    //String("padding: 8px;") +
                    String("}") +
                    String("tr:nth-child(even) {") +
                    String("/*background-color: #dddddd;*/") +
                    String("}") +

                    String("/* Center the loader */") +
String("#loader {") +
    String("position: absolute;") +
    String("left: 50%;") +
    String("top: 50%;") +
    String("z-index: 1;") +
    String("width: 120px;") +
    String("height: 120px;") +
    String("margin: -76px 0 0 -76px;") +
    String("border: 16px solid #f3f3f3;") +
    String("border-radius: 50%;") +
    String("border-top: 16px solid #3498db;") +
    String("-webkit-animation: spin 2s linear infinite;") +
    String("animation: spin 2s linear infinite;") +
  String("}") +
  
  String("@-webkit-keyframes spin {") +
    String("0% { -webkit-transform: rotate(0deg); }") +
    String("100% { -webkit-transform: rotate(360deg); }") +
  String("}") +
  
  String("@keyframes spin {") +
    String("0% { transform: rotate(0deg); }") +
    String("100% { transform: rotate(360deg); }") +
  String("}") +
  
  String("/* Add animation to 'page content' */") +
  String(".animate-bottom {") +
    String("position: relative;") +
    String("-webkit-animation-name: animatebottom;") +
    String("-webkit-animation-duration: 1s;") +
    String("animation-name: animatebottom;") +
    String("animation-duration: 1s") +
  String("}") +
  
  String("@-webkit-keyframes animatebottom {") +
    String("from { bottom:-100px; opacity:0 } ") +
    String("to { bottom:0px; opacity:1 }") +
  String("}") +
  
  String("@keyframes animatebottom { ") +
    String("from{ bottom:-100px; opacity:0 } ") +
    String("to{ bottom:0; opacity:1 }") +
  String("}") +
  
  String("#myDiv {") +
    String("display: none;") +
    String("/*text-align: center;*/") +
  String("}") +
                String("</style>");
    response += String("</head><body>");

    response += String("<h2>LoRa Boat Monitor</h2>");
    response += String(String(actconf.crights) + ", " + String(actconf.fversion) + ", CQ: " + String(wlanquality()) + "<data id='quality'></data> %");
    response += String("<hr align='left'>");
    response += String("<h3>");
    response += String("<blink>");
    response += String("<data id='info'></data>");
    response += String("</blink>");
    response += String("</h3>");
    response += String("<div id='loader'></div>");
    response += String("<div style='display:none;' id='myDiv' class='animate-bottom'>");
    //Serial.printf("Listing directory: %s\r\n", dirname);
    response += "Listing directory: ";
    response += dirname;
    response += "<br><br>";
    response += "<table><tr>";
    response += "<th>Type</th><th>Name</th><th>Size</th><th>LAST WRITE</th></tr>";
    
    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        response += "<br>";
        return "- failed to open directory";
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        response += "<br>";
        return " - not a directory";
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");

            Serial.print(file.name());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);

            String timestamp = String((tmstruct->tm_year)+1900) + String("-") + String(( tmstruct->tm_mon)+1) + String("-") + String(tmstruct->tm_mday) + String(" ") + String(tmstruct->tm_hour) + String(":") + String(tmstruct->tm_min) + String(":") + String(tmstruct->tm_sec);

            response += String("<tr>") +
                        String("<td>Dir</td>") +
                        String("<td>") + String(file.name()) + String("</td>") +
                        String("<td>""</td>") +
                        String("<td>") + timestamp + String("</td>") +
                        String("</tr>");

            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");

            Serial.print(file.size());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);

            String timestamp = String((tmstruct->tm_year)+1900) + String("-") + String(( tmstruct->tm_mon)+1) + String("-") + String(tmstruct->tm_mday) + String(" ") + String(tmstruct->tm_hour) + String(":") + String(tmstruct->tm_min) + String(":") + String(tmstruct->tm_sec);

            response += String("<tr>") +
                        String("<td>FILE</td>") +
                        String("<td><a href='") + String(file.name()) + String("'>") + String(file.name()) + String("</a></td>") +
                        String("<td>") + file.size() + String("</td>") +
                        String("<td>") + timestamp + String("</td>") +
                        String("</tr>");
        }
        file = root.openNextFile();
    }
    response += "</table>";
    return response + String("</br></br> Upload file:") + serverIndex + "</body></html>";
}

uint32_t startTime;

String formatfs(fs::FS &fs) {
  //startTime = millis();
    //LittleFS.format();
    //Serial.printf_P(PSTR("FS format done, took %lu ms!\n"), millis() - startTime);

    LittleFS.format();
    // falls es nicht klappt, erneut mit Neu-Formatierung versuchen
    if (!LittleFS.begin(true)) {
      Serial.println("LittleFS Mount fehlgeschlagen");
      Serial.println("Formatierung nicht möglich");
      return "error";
    } else {
      Serial.println("Formatierung des Dateisystems erfolgt");
      return "done";
    }

    // versuchen, ein vorhandenes Dateisystem einzubinden
  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS Mount fehlgeschlagen");
    Serial.println("Kein Dateisystemsystem gefunden; wird formatiert");
    LittleFS.format();
    // falls es nicht klappt, erneut mit Neu-Formatierung versuchen
    if (!LittleFS.begin(true)) {
      Serial.println("LittleFS Mount fehlgeschlagen");
      Serial.println("Formatierung nicht möglich");
      return "error";
    } else {
      Serial.println("Formatierung des Dateisystems erfolgt");
      return "done";
    }
  }
  return "???";
}

#endif