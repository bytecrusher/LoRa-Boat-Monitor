
#ifndef filesystem_h
#define filesystem_h

/* You only need to format LittleFS the first time you run a
   test or else use the LITTLEFS plugin to create a partition
   https://github.com/lorol/arduino-esp32littlefs-plugin */
   
#define FORMAT_LITTLEFS_IF_FAILED true

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    DebugPrintln(3, "Listing directory: " + String(dirname));

    File root = fs.open(dirname);
    if(!root){
        DebugPrintln(1, "- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        DebugPrintln(1, " - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            DebugPrint(3, "  DIR : " + String(file.name()));
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            DebugPrintln(3, "  LAST WRITE: " + String((tmstruct->tm_year)+1900) + "-" + String(( tmstruct->tm_mon)+1) + "-" + String(tmstruct->tm_mday) + " " + String(tmstruct->tm_hour) + ":" + String(tmstruct->tm_min) + ":" + String(tmstruct->tm_sec));

            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            DebugPrint(3, "  FILE: " + String(file.name()) + "  SIZE: " + String(file.size()));
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            DebugPrintln(3, "  LAST WRITE: " + String((tmstruct->tm_year)+1900) + "-" + String(( tmstruct->tm_mon)+1) + "-" + String(tmstruct->tm_mday) + " " + String(tmstruct->tm_hour) + ":" + String(tmstruct->tm_min) + ":" + String(tmstruct->tm_sec));
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    DebugPrintln(3, "Creating Dir: " + String(path));
    if(fs.mkdir(path)){
        DebugPrintln(3, "Dir created");
    } else {
        DebugPrintln(1, "mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    DebugPrintln(3, "Removing Dir: " + String(path));
    if(fs.rmdir(path)){
        DebugPrintln(3, "Dir removed");
    } else {
        DebugPrintln(1, "rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    DebugPrintln(3, "Reading file: " + String(path));

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        DebugPrintln(1, "- failed to open file for reading");
        return;
    }

    DebugPrintln(3, "- read from file:");
    while(file.available()){
        //Serial.write(file.read());
        DebugPrintln(3, String(file.read()));
    }
    file.close();
}

String readFile2(fs::FS &fs, const char * path){
    String filecontent = "";
    File file = fs.open(path);
    if(!file || file.isDirectory()){
        DebugPrintln(1, "- failed to open file for reading");
        return "- failed to open file for reading";
    }

    //DebugPrint(3, "- read from file (readfile2): ");
    //DebugPrintln(3, path);
    while(file.available()){
        filecontent+=String((char)file.read());
    }
    file.close();
    return filecontent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    //Serial.printf("Writing file: %s\r\n", path);
    DebugPrint(3, "Writing file: " + String(path));

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        //Serial.println("- failed to open file for writing");
        DebugPrint(1, "- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        //Serial.println("- file written");
        DebugPrintln(3, "- file written");
    } else {
        //Serial.println("- write failed");
        DebugPrintln(1, "- write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    //Serial.printf("Appending to file: %s\r\n", path);
    DebugPrint(3, "Appending to file: " + String(path));

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        //Serial.println("- failed to open file for appending");
        DebugPrintln(1, "- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        //Serial.println("- message appended");
        DebugPrintln(3, "- message appended");
    } else {
        //Serial.println("- append failed");
        DebugPrintln(1, "- append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    //Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    DebugPrint(3, "Renaming file " + String(path1) + " to " + String(path2));
    if (fs.rename(path1, path2)) {
        //Serial.println("- file renamed");
        DebugPrintln(3, "- file renamed");
    } else {
        //Serial.println("- rename failed");
        DebugPrintln(1, "- rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    //Serial.printf("Deleting file: %s\r\n", path);
    DebugPrint(3, "Deleting file: " + String(path));
    if(fs.remove(path)){
        //Serial.println("- file deleted");
        DebugPrintln(3, "- file deleted");
    } else {
        //Serial.println("- delete failed");
        DebugPrintln(1, "- delete failed");
    }
}

// SPIFFS-like write and delete file

// See: https://github.com/esp8266/Arduino/blob/master/libraries/LittleFS/src/LittleFS.cpp#L60
void writeFile2(fs::FS &fs, const char * path, const char * message){
    if(!fs.exists(path)){
		if (strchr(path, '/')) {
			//Serial.printf("Create missing folders of: %s\r\n", path);
			DebugPrint(3, "Create missing folders of: " + String(path));
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

    //Serial.printf("Writing file to: %s\r\n", path);
    DebugPrint(3, "Writing file to: " + String(path));
    File file = fs.open(path, FILE_WRITE);
    if(!file){
        //Serial.println("- failed to open file for writing");
        DebugPrintln(1, "- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        //Serial.println("- file written");
        DebugPrintln(3, "- file written");
    } else {
        //Serial.println("- write failed");
        DebugPrintln(1, "- write failed");
    }
    file.close();
}

// See:  https://github.com/esp8266/Arduino/blob/master/libraries/LittleFS/src/LittleFS.h#L149
void deleteFile2(fs::FS &fs, const char * path){
    //Serial.printf("Deleting file and empty folders on path: %s\r\n", path);
    DebugPrint(1, "Deleting file and empty folders on path: " + String(path));

    if(fs.remove(path)){
        //Serial.println("- file deleted");
        DebugPrintln(3, "- file deleted");
    } else {
        //Serial.println("- delete failed");
        DebugPrintln(1, "- delete failed");
    }

    char *pathStr = strdup(path);
    if (pathStr) {
        char *ptr = strrchr(pathStr, '/');
        if (ptr) {
            //Serial.printf("Removing all empty folders on path: %s\r\n", path);
            DebugPrint(3, "Removing all empty folders on path: " + String(path));
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
    //Serial.printf("Testing file I/O with %s\r\n", path);
    DebugPrint(1, "Testing file I/O with " + String(path));

    static uint8_t buf[512];
    size_t len = 0;
    File file = fs.open(path, FILE_WRITE);
    if(!file){
        //Serial.println("- failed to open file for writing");
        DebugPrintln(1, "- failed to open file for writing");
        return;
    }

    size_t i;
    //Serial.print("- writing" );
    DebugPrint(3, "- writing");
    uint32_t start = millis();
    for(i=0; i<2048; i++){
        if ((i & 0x001F) == 0x001F){
          //Serial.print(".");
          DebugPrint(3, ".");
        }
        file.write(buf, 512);
    }
    //Serial.println("");
    DebugPrintln(3, "");
    uint32_t end = millis() - start;
    //Serial.printf(" - %u bytes written in %u ms\r\n", 2048 * 512, end);
    DebugPrint(3, " - " + String(2048 * 512) + " bytes written in " + String(end) + " ms");
    file.close();

    file = fs.open(path);
    start = millis();
    end = start;
    i = 0;
    if(file && !file.isDirectory()){
        len = file.size();
        size_t flen = len;
        start = millis();
        //Serial.print("- reading" );
        DebugPrint(3, "- reading");
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            if ((i++ & 0x001F) == 0x001F){
              //Serial.print(".");
              DebugPrint(3, ".");
            }
            len -= toRead;
        }
        //Serial.println("");
        DebugPrintln(3, "");
        end = millis() - start;
        //Serial.printf("- %u bytes read in %u ms\r\n", flen, end);
        DebugPrint(3, "- " + String(flen) + " bytes read in " + String(end) + " ms");
        file.close();
    } else {
        //Serial.println("- failed to open file for reading");
        DebugPrintln(1, "- failed to open file for reading");
    }
}

/*DynamicJsonDocument getMyDirAsJson(fs::FS &fs, const char * dirname, uint8_t levels){
    //response += "<th>Type</th><th>Name</th><th>Size</th><th>LAST WRITE</th></tr>";
    String response = "";
    DynamicJsonDocument json_Dir(8048);
    json_Dir["Device"]["Type"] = String(actconf.devname);
    json_Dir["Device"]["CopyRights"] = String(actconf.crights);
    //json_Dir.add("Filename") = ;

    File root = fs.open(dirname);
    if(!root){
        //Serial.println("- failed to open directory");
        DebugPrintln(3, "- failed to open directory");
        response += "<br>";
        return "- failed to open directory";
    }
    if(!root.isDirectory()){
        //Serial.println(" - not a directory");
        DebugPrintln(3, " - not a directory");
        response += "<br>";
        return " - not a directory";
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            //Serial.print("  DIR : ");
            DebugPrint(3, "  DIR : ");

            //Serial.print(file.name());
            DebugPrint(3, String(file.name()));
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
            //Serial.print("  FILE: ");
            DebugPrint(3, "  FILE: ");
            //Serial.print(file.name());
            DebugPrint(3, String(file.name()));
            //Serial.print("  SIZE: ");
            DebugPrintln(3, "  SIZE: ");

            Serial.print(file.size());
            DebugPrint(3, String(file.size()));
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
    return json_Dir;
}*/

String getMyDirAsString(fs::FS &fs, const char * dirname, uint8_t levels){
    String response = "";
    //response += "<p>Listing directory: " + (String)dirname + "</p>";
    response += "<table><tr><th>Type</th><th>Name</th><th>Size (KB)</th><th>LAST WRITE</th></tr>";

    File root = fs.open(dirname);
    if(!root){
        DebugPrintln(1, "- failed to open directory");
        response += "<br>";
        return "- failed to open directory";
    }
    if(!root.isDirectory()){
        DebugPrintln(1, " - not a directory");
        response += "<br>";
        return " - not a directory";
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            DebugPrintln(3, "  DIR : ");

            DebugPrintln(3, file.name());
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
            DebugPrint(3, "  FILE: " + String(file.name()) + "  SIZE: " + String(file.size()));

            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);

            DebugPrintln(3, "  LAST WRITE: " + String((tmstruct->tm_year)+1900) + "-" + String(( tmstruct->tm_mon)+1) + "-" + String(tmstruct->tm_mday) + " " + String(tmstruct->tm_hour) + ":" + String(tmstruct->tm_min) + ":" + String(tmstruct->tm_sec));

            String timestamp = String((tmstruct->tm_year)+1900) + String("-") + String(( tmstruct->tm_mon)+1) + String("-") + String(tmstruct->tm_mday) + String(" ") + String(tmstruct->tm_hour) + String(":") + String(tmstruct->tm_min) + String(":") + String(tmstruct->tm_sec);

            response += String("<tr>") +
                        String("<td>FILE</td>") +
                        String("<td><a href='") + String(file.name()) + String("'>") + String(file.name()) + String("</a></td>") +
                        String("<td>") + float(file.size() / 1024.0) + String("</td>") +
                        String("<td>") + timestamp + String("</td>") +
                        String("</tr>");
        }
        file = root.openNextFile();
    }
    response += "</table>";
    return response;
}

uint32_t startTime;

String formatfs(fs::FS &fs) {
    LittleFS.format();
    // falls es nicht klappt, erneut mit Neu-Formatierung versuchen
    if (!LittleFS.begin(true)) {
        DebugPrintln(1, "LittleFS Mount fehlgeschlagen");
        DebugPrintln(1, "Formatierung nicht möglich");
        return "error";
    } else {
        DebugPrintln(3, "Formatierung des Dateisystems erfolgt");
        return "done";
    }
}

#endif