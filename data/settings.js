let fileInput = null;

document.addEventListener("DOMContentLoaded", function () {
    fileInput = byId("file");
    applySettingsSelections();
    bindSettingsPageEvents();
    setTimeout(showPage, 10);
});

function bindSettingsPageEvents() {
    var toggleButtons = document.querySelectorAll(".section-toggle");
    for (var i = 0; i < toggleButtons.length; i += 1) {
        toggleButtons[i].addEventListener("click", function (event) {
            var button = event.currentTarget;
            ToggleSection(button, button.dataset.target);
        });
    }

    var toggleAllButton = byId("btAllSettingsPage");
    if (toggleAllButton) {
        toggleAllButton.addEventListener("click", ToggleAllSections);
    }

    var backButton = byId("backToOverviewButton");
    if (backButton) {
        backButton.addEventListener("click", function () {
            window.location.assign("/index.html");
        });
    }

    var logoutButtonElement = byId("logoutBtn");
    if (logoutButtonElement) {
        logoutButtonElement.addEventListener("click", logoutButton);
    }

    var downloadButton = byId("downloadConfigButton");
    if (downloadButton) {
        downloadButton.addEventListener("click", downloadConfigAsJson);
    }

    var restoreButton = byId("restoreConfigBackupButton");
    if (restoreButton) {
        restoreButton.addEventListener("click", restoreConfigBackup);
    }

    if (fileInput) {
        fileInput.addEventListener("change", uploadConfig);
    }

    var settingsForm = byId("form1");
    if (settingsForm) {
        settingsForm.addEventListener("submit", saveSettings);
    }
}

function applySettingsSelections() {
    var selections = window.settingsSelections || {};
    var form = document.forms.SetForm;
    if (!form) return;

    Object.keys(selections).forEach(function (fieldName) {
        var field = form[fieldName];
        if (!field) return;

        if (field.tagName === 'SELECT') {
            field.selectedIndex = selections[fieldName];
        } else {
            field.value = selections[fieldName];
        }
    });

    if (selections.crypt == 1) {
        byId("logoutBtn").style.display = "block";
    }
}

function showPage() {
    byId("loader").style.display = "none";
    byId("myDiv").style.display = "block";
}

function settingsCsrfToken(form) {
    if (typeof csrfToken === "function") {
        var token = csrfToken();
        if (token) {
            return token;
        }
    }

    var csrfField = form ? form.querySelector("input[name='csrf']") : null;
    return csrfField ? csrfField.value : "";
}

async function saveSettings(event) {
    event.preventDefault();

    var form = event.currentTarget;
    var token = settingsCsrfToken(form);
    var formData = new FormData(form);
    if (token) {
        formData.set("csrf", token);
    }
    var body = new URLSearchParams(formData);

    setBusyState("Saving settings...");

    try {
        var response = await fetch("/savesettings", {
            method: "POST",
            headers: token ? {
                "Content-Type": "application/x-www-form-urlencoded",
                "X-CSRF-Token": token
            } : {
                "Content-Type": "application/x-www-form-urlencoded"
            },
            body: body.toString(),
            credentials: "same-origin",
            redirect: "follow"
        });

        if (!response.ok) {
            var message = "Settings could not be saved.";
            try {
                var json = await response.json();
                if (json && json.message) {
                    message = json.message;
                }
            } catch (error) {
            }
            throw new Error(message);
        }

        window.location.assign("/settings.html");
    } catch (error) {
        clearBusyState();
        alert(error && error.message ? error.message : "Settings could not be saved.");
    }
}

function ToggleSection(ele, target) {
    var targetElement = byId(target);
    var isHidden = targetElement.style.display === "none" || targetElement.style.display === "";
    if (isHidden) {
        targetElement.style.display = "block";
        ele.value = '-';
    } else {
        targetElement.style.display = "none";
        ele.value = '+';
    }
}

function ToggleAllSections() {
    const nodes = document.getElementsByClassName("collapsible");
    const shouldOpen = !nodes.length || nodes[0].style.display == "none" || nodes[0].style.display === "";
    if (shouldOpen) {
        for (let i = 0; i < nodes.length; i++) {
            nodes[i].style.display = "block";
            byId("btAllSettingsPage").value = '-';
        }
    } else {
        for (let i = 0; i < nodes.length; i++) {
            nodes[i].style.display = "none";
            byId("btAllSettingsPage").value = '+';
            }
    }
    const nodes2 = document.getElementsByClassName("myToggleButton");
    if (!shouldOpen) {
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
    var reguexp = /[^a-fA-F0-9]/;
    if (valuestring.length > 0 && (reguexp.exec(valuestring) || valuestring.length !== 32)) {
        document.getElementById('sub').disabled = true;
        alert('Error!\\nUse only hex characters 0-9, A-F.\\nLeave empty to keep the existing key.');
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
    var reguexp = /[^\x20-\x7E]/;
    if (reguexp.exec(valuestring) || valuestring.length < 1 || valuestring.length > 30) {
        document.getElementById('sub').disabled = true;
        alert('Error!\nUse only printable characters.\nSSID Length 1-30');
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
    if (reguexp.exec(valuestring) || valuestring.length < 4 || valuestring.length > 20) {
        document.getElementById('sub').disabled = true;
        alert('Error!\nUse only a-z, A-Z, 0-9, -\nUsername Length 4-20');
    }
    else {
        document.getElementById('sub').disabled = false;
    }
}

function check_passwd(iname) {
    var valuestring = "";
    if (iname == "pagepasswd") { valuestring = document.SetForm.pagepasswd.value; }
    if (iname == "cpasswd1") { valuestring = document.SetForm.cpasswd1.value; }
    if (iname == "cpasswd2") { valuestring = document.SetForm.cpasswd2.value; }
    if (iname == "cpasswd3") { valuestring = document.SetForm.cpasswd3.value; }
    if (iname == "spasswd") { valuestring = document.SetForm.spasswd.value; }
    var reguexp = /[^\x20-\x7E]/;
    if (valuestring.length === 0) {
        document.getElementById('sub').disabled = false;
    }
    else if (reguexp.exec(valuestring) || valuestring.length < 8 || valuestring.length > 30) {
        document.getElementById('sub').disabled = true;
        alert('Error!\nUse only printable characters.\nPassword Length 8-30');
    }
    else {
        document.getElementById('sub').disabled = false;
    }
}

function check_mds_ota_url(iname) {
    var field = document.SetForm[iname];
    var value = field ? field.value.trim() : "";
    if (value.length > 0 && !value.startsWith("https://")) {
        document.getElementById('sub').disabled = true;
        alert('Error!\nThe MDS OTA endpoint must start with https://');
    }
    else {
        document.getElementById('sub').disabled = false;
    }
}

function check_alarmState(event) {
    var selectElement = event.target;
    var value = selectElement.value;
    if (value == "On") {
        requestJson("/getdata?data=alarm1")
            .then(function (response) {
                if (response.alarm1 == "0" || response.alarm1 === 0) {
                    alert("Standby mode will be saved. Note: the alarm input is currently inactive, so the device may enter deep sleep shortly after saving.");
                }
            })
            .catch(function () {
                alert("Could not verify the alarm input state.");
            });
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
    if (!fileInput || !fileInput.value.length) return;
    // Create a new FileReader() object
    let reader = new FileReader();
    // Setup the callback event to run when the file is read
    reader.onload = logFile;
    // Read the file
    reader.readAsText(fileInput.files[0]);
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
    if (!fileInput || !fileInput.value.length) return;
    // Create a new FileReader() object
    let reader = new FileReader();
    // Setup the callback event to run when the file is read
    reader.onload = logFile;
    // Read the file
    reader.readAsText(fileInput.files[0]);
}

/**
 * Log the uploaded file to the console
 * @param {event} Event The file loaded event
 */
function logFile (event) {
    const form = document.getElementById("form1");
    if (!form) return;

    let json;
    try {
        json = JSON.parse(event.target.result);
    } catch (error) {
        alert("Config file could not be read. Please select a valid JSON backup.");
        return;
    }

    Object.entries(json).forEach(function (entry) {
        const key = entry[0];
        const value = entry[1];
        if (key === "csrf") return;

        const fields = document.getElementsByName(key);
        if (!fields.length) return;

        const field = fields[0];
        if (field.tagName === "SELECT") {
            field.value = value;
        } else if (field.type !== "hidden") {
            field.value = value;
        }
    });

    if (typeof form.requestSubmit === "function") {
        form.requestSubmit();
    } else {
        saveSettings({ preventDefault: function () {}, currentTarget: form });
    }
}

function download(content, fileName, contentType) {
    var a = document.createElement("a");
    var blob = new Blob([content], {type: contentType});
    a.href = URL.createObjectURL(blob);
    a.download = fileName;
    a.click();
}

function logoutButton() {
    request("/logout").finally(function () {
        setTimeout(function () {
            window.location.assign("/logged-out");
        }, 1000);
    });
}

function setBusyState(message) {
    document.getElementById("loader").style.display = "block";
    document.getElementById("myDiv").style.display = "none";
    if (message) {
        console.log(message);
    }
}

function clearBusyState() {
    byId("loader").style.display = "none";
    byId("myDiv").style.display = "block";
}

async function waitForDeviceAndRedirect(targetUrl, retries) {
    if (retries <= 0) {
        clearBusyState();
        alert("Device did not come back online in time. Please reload the page manually.");
        return;
    }

    try {
        await requestJson("/staticdata.json?ts=" + Date.now());
        window.location.replace(targetUrl);
        return;
    } catch (error) {
    }

    await delay(2000);
    waitForDeviceAndRedirect(targetUrl, retries - 1);
}

async function restoreConfigBackup() {
    if (!confirm("Restore the last automatic OTA config backup from LittleFS and reboot the device?")) {
        return;
    }

    setBusyState("Restoring config backup...");

    try {
        await request("/restoreconfigbackup", { method: "POST" });
        setTimeout(function () {
            waitForDeviceAndRedirect("/settings.html", 45);
        }, 3000);
    } catch (error) {
        clearBusyState();
        var message = "Restore failed.";
        try {
            var response = JSON.parse(error.text);
            if (response.message) {
                message = response.message;
            }
        } catch (e) {
        }
        alert(message);
    }
}
