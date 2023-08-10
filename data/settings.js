var myVar;
document.addEventListener("DOMContentLoaded", function() {
    setTimeout(function() {
        //document.querySelector("body").classList.add("loaded");
        showPage();
    }, 10)
    // Get the form and file field
    //let myform = document.querySelector('#upload');
    let file = document.querySelector('#file');

    // Listen for submit events
    //myform.addEventListener('submit', handleSubmit);
});

function showPage() {
    document.getElementById("loader").style.display = "none";
    document.getElementById("myDiv").style.display = "block";
}

function ToggleSection(ele, target) {
    if (document.getElementById(target).style.display == "none") {
        document.getElementById(target).style.display = "block";
        document.getElementById(ele.id).value = '-';
    } else {
        document.getElementById(target).style.display = "none";
        document.getElementById(ele.id).value = '+';
    }
}

function ToggleAllSections() {
    const nodes = document.getElementsByClassName("collapsible");
    if (nodes[0].style.display == "none") {
        for (let i = 0; i < nodes.length; i++) {
            nodes[i].style.display = "block";
            document.getElementById("btAllSettingsPage").value = '-';
        }
    } else {
        for (let i = 0; i < nodes.length; i++) {
            nodes[i].style.display = "none";
            document.getElementById("btAllSettingsPage").value = '+';
            }
    }
    const nodes2 = document.getElementsByClassName("myToggleButton");
    if (nodes[0].style.display == "none") {
        for (let i = 0; i < nodes2.length; i++) {
            nodes2[i].value = '+';
        }
    } else {
        for (let i = 0; i < nodes2.length; i++) {
            nodes2[i].value = '-';
        }
    }
}

function check_devaddr(iname) {
    var valuestring = "";
    if (iname == "devaddr") {
        valuestring = document.SetForm.devaddr.value;
    }
    var reguexp = /[^A-Z0-9]/;
    if (reguexp.exec(valuestring) || valuestring.length !== 8) {
        document.getElementById('sub').disabled = true;
        alert('Error!\\nUse only A-Z, 0-9, \\nAddress Length not 8');
    }
    else {
        document.getElementById('sub').disabled = false;
    }
}

function check_key(iname) {
    var valuestring = "";
    if (iname == "nskey") { valuestring = document.SetForm.nskey.value; }
    if (iname == "appkey") { valuestring = document.SetForm.appkey.value; }
    var reguexp = /[^A-Z0-9]/;
    console.log(valuestring.length);
    if (reguexp.exec(valuestring) || valuestring.length !== 32) {
        document.getElementById('sub').disabled = true;
        alert('Error!\\nUse only A-Z, 0-9, \\nKey Length not 32');
    }
    else {
        document.getElementById('sub').disabled = false;
    }
}

function check_ssid(iname) {
    var valuestring = "";
    if (iname == "cssid1") { valuestring = document.SetForm.cssid1.value; }
    if (iname == "cssid2") { valuestring = document.SetForm.cssid2.value; }
    if (iname == "cssid3") { valuestring = document.SetForm.cssid3.value; }
    if (iname == "sssid") { valuestring = document.SetForm.sssid.value; }
    var reguexp = /[^A-z0-9_-]/;
    if (reguexp.exec(valuestring) || valuestring.length < 1 || valuestring.length > 20) {
        document.getElementById('sub').disabled = true;
        alert('Error!\nUse only a-z, A-Z, 0-9, _-\nSSID Length 1-20');
    }
    else {
        document.getElementById('sub').disabled = false;
    }
}

function check_tinterval(iname) {
    var valuestring = "";
    if (iname == "tinterval") { valuestring = document.SetForm.tinterval.value; }
    var reguexp = /[^0-9]/;
    if (reguexp.exec(valuestring) || valuestring < 1 || valuestring > 255) {
        document.getElementById('sub').disabled = true;
        alert('Error!\nUse only 0-9, \nValues 1...255');
    }
    else {
        document.getElementById('sub').disabled = false;
    }
}

function check_username(iname) {
    var valuestring = "";
    if (iname == "username") { valuestring = document.SetForm.username.value; }
    var reguexp = /[^A-z0-9\-]/;
    if (reguexp.exec(valuestring) || valuestring < 4 || valuestring > 20) {
        document.getElementById('sub').disabled = true;
        alert('Error!\nUse only a-z, A-Z, 0-9, ' - '\nPassword Length 4-20');
    }
    else {
        document.getElementById('sub').disabled = false;
    }
}

function check_passwd(iname) {
    var valuestring = "";
    if (iname == "pagepasswd") { valuestring = document.SetForm.sidepasswd.value; }
    if (iname == "cpasswd1") { valuestring = document.SetForm.cpasswd1.value; }
    if (iname == "cpasswd2") { valuestring = document.SetForm.cpasswd2.value; }
    if (iname == "cpasswd3") { valuestring = document.SetForm.cpasswd3.value; }
    if (iname == "spasswd") { valuestring = document.SetForm.spasswd.value; }
    var reguexp = /[^A-z0-9\-]/;
    if (reguexp.exec(valuestring) || valuestring.length < 8 || valuestring.length > 20) {
        document.getElementById('sub').disabled = true;
        alert('Error!\nUse only a-z, A-Z, 0-9, ' - '\nPassword Length 8-20');
    }
    else {
        document.getElementById('sub').disabled = false;
    }
}

function check_alarmState(event) {
    var selectElement = event.target;
    var value = selectElement.value;
    if (value == "On") {
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function () {
            if (this.readyState == 4 && this.status == 200) {
                //console.log(xhr.response);
                if (xhr.response == "0") {
                    alert("Please check wiring and make sure, Batterie switch is on.");
                    selectElement.value = "Off";
                }
            }
        };
        //xhr.open("GET", "/getdata?alarmState="+element.id+"&state=1", true);
        xhr.open("GET", "/getdata", true);
        xhr.send();
    }
}

function downloadConfigAsJson() {
    const formData = new FormData(form1);
    var myjsonString = JSON.stringify(Object.fromEntries(formData));
    var boardname = "boatmonitor-" + document.getElementById('deviceid').value;
    download(myjsonString, boardname + '.json', 'text/plain');
}

function uploadConfig() {
    // If there's no file, do nothing
    if (!file.value.length) return;
    // Create a new FileReader() object
    let reader = new FileReader();
    // Setup the callback event to run when the file is read
    reader.onload = logFile;
    // Read the file
    reader.readAsText(file.files[0]);
    //window.open('/savesettings', '_self');
}

/**
 * Handle submit events
 * @param  {Event} event The event object
 */
function handleSubmit (event) {
    // Stop the form from reloading the page
    event.preventDefault();
    // If there's no file, do nothing
    if (!file.value.length) return;
    // Create a new FileReader() object
    let reader = new FileReader();
    // Setup the callback event to run when the file is read
    reader.onload = logFile;
    // Read the file
    reader.readAsText(file.files[0]);
}

/**
 * Log the uploaded file to the console
 * @param {event} Event The file loaded event
 */
function logFile (event) {
    let str = event.target.result;
    let json = JSON.parse(str);
    Object.entries(json).forEach((entry) => {
        const [key, value] = entry;
        //console.log(`${key}: ${value}`);
        document.getElementsByName(`${key}`)[0].value = value;
    });
    //console.log('string', str);
    //console.log('json', json);
    //window.open('/savesettings', '_self');
    document.getElementById("form1").submit();
}

function download(content, fileName, contentType) {
    var a = document.createElement("a");
    var file = new Blob([content], {type: contentType});
    a.href = URL.createObjectURL(file);
    a.download = fileName;
    a.click();
}

function logoutButton() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "logout", true);
    xhr.send();
    setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}