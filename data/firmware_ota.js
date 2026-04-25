function otaToBoolean(value) {
    switch ((value || '').toLowerCase().trim()) {
        case 'true':
        case 'yes':
        case '1':
            return true;
        case 'false':
        case 'no':
        case '0':
            return false;
        default:
            return Boolean(value);
    }
}

function otaElement(id) {
    return document.getElementById(id);
}

var otaProgressPollTimer = null;

function disableAll() {
    var overlay = document.getElementById('overlay');
    if (overlay) {
        overlay.style.display = 'block';
    }
}

function enableAll() {
    var overlay = document.getElementById('overlay');
    if (overlay) {
        overlay.style.display = 'none';
    }
}

function waitForReboot(targetUrl, retries) {
    if (retries <= 0) {
        enableAll();
        otaElement('status').innerHTML = 'Upload finished, but the device did not come back online yet. Please refresh manually.';
        return;
    }

    var ajax = new XMLHttpRequest();
    ajax.open('GET', '/staticdata.json?ts=' + Date.now(), true);
    ajax.timeout = 2000;
    ajax.onreadystatechange = function () {
        if (ajax.readyState !== 4) {
            return;
        }

        if (ajax.status === 200) {
            window.location.replace(targetUrl);
            return;
        }

        setTimeout(function () {
            waitForReboot(targetUrl, retries - 1);
        }, 2000);
    };
    ajax.onerror = ajax.ontimeout = function () {
        setTimeout(function () {
            waitForReboot(targetUrl, retries - 1);
        }, 2000);
    };
    ajax.send();
}

function progressHandler(event) {
    otaElement('loaded_n_total').innerHTML = 'Uploaded ' + event.loaded + ' bytes of ' + event.total;
    var percent = Math.round((event.loaded / event.total) * 100);
    otaElement('progressBar').style.width = percent + '%';
    otaElement('status').innerHTML = percent + '% uploaded... please wait';
}

function applyOtaProgress(progress) {
    if (!progress) {
        return;
    }

    var percent = progress.percent || 0;
    otaElement('progressBar').style.width = percent + '%';
    if (progress.total > 0) {
        otaElement('loaded_n_total').innerHTML = percent + '% (' + progress.current + ' / ' + progress.total + ' bytes)';
    }
    if (progress.message) {
        otaElement('status').innerHTML = progress.message;
    }
}

function stopOtaProgressPolling() {
    if (otaProgressPollTimer) {
        clearInterval(otaProgressPollTimer);
        otaProgressPollTimer = null;
    }
}

function pollOtaProgress() {
    var ajax = new XMLHttpRequest();
    ajax.open('GET', '/otaprogress?ts=' + Date.now(), true);
    ajax.onreadystatechange = function () {
        if (ajax.readyState !== 4 || ajax.status !== 200) {
            return;
        }

        try {
            var progress = JSON.parse(ajax.responseText);
            applyOtaProgress(progress);
            if (!progress.active && (progress.success || progress.phase === 'error')) {
                stopOtaProgressPolling();
            }
        } catch (error) {
        }
    };
    ajax.send();
}

function startOtaProgressPolling() {
    stopOtaProgressPolling();
    pollOtaProgress();
    otaProgressPollTimer = setInterval(pollOtaProgress, 500);
}

function completeHandler(event) {
    var responseText = event.target.responseText || '';
    var response = null;
    try {
        response = JSON.parse(responseText);
    } catch (error) {
    }

    if (event.target.status >= 400) {
        enableAll();
        otaElement('status').innerHTML = response && response.message ? response.message : 'Upload failed';
        return;
    }

    otaElement('status').innerHTML = response && response.message ? response.message : 'Upload success. Waiting for device reboot...';
    otaElement('progressBar').value = 0;
    setTimeout(function () {
        waitForReboot('/', 45);
    }, 3000);
}

function startHandler() {
    disableAll();
    startOtaProgressPolling();
}

function errorHandler() {
    stopOtaProgressPolling();
    enableAll();
    otaElement('status').innerHTML = 'Upload failed';
}

function abortHandler() {
    stopOtaProgressPolling();
    enableAll();
    otaElement('status').innerHTML = 'Upload aborted';
}

function uploadFile() {
    var file = otaElement('file1').files[0];
    var formdata = new FormData();
    formdata.append(otaElement('file1').name, file, file.name);

    var ajax = new XMLHttpRequest();
    ajax.upload.addEventListener('progress', progressHandler, false);
    ajax.addEventListener('load', completeHandler, false);
    ajax.addEventListener('loadstart', startHandler, false);
    ajax.addEventListener('error', errorHandler, false);
    ajax.addEventListener('abort', abortHandler, false);
    ajax.open('POST', '/doUpdate');
    ajax.setRequestHeader('Access-Control-Allow-Headers', '*');
    ajax.setRequestHeader('Access-Control-Allow-Origin', '*');
    ajax.send(formdata);
}

function setUploadMode(mode) {
    var firmwareButton = otaElement('firmware-button');
    var filesystemButton = otaElement('filesystem-button');
    var isFilesystem = mode === 'filesystem';

    otaElement('file1').name = isFilesystem ? 'filesystem' : 'firmware';
    firmwareButton.classList.toggle('selected', !isFilesystem);
    filesystemButton.classList.toggle('selected', isFilesystem);
    otaElement('status').innerHTML = isFilesystem ? 'File system upload' : 'Firmware upload';
}

document.addEventListener('DOMContentLoaded', function () {
    var urlParams = new URLSearchParams(window.location.search);
    var onlyFirmware = urlParams.get('onlyFirmware');

    if (onlyFirmware && otaToBoolean(onlyFirmware) === true) {
        otaElement('switch-container').style.display = 'none';
    }

    otaElement('upload_form').addEventListener('submit', function (event) {
        event.preventDefault();
        uploadFile();
    });

    otaElement('firmware-button').addEventListener('click', function () {
        setUploadMode('firmware');
    });

    otaElement('filesystem-button').addEventListener('click', function () {
        setUploadMode('filesystem');
    });

    otaElement('file1').addEventListener('change', function () {
        var file = otaElement('file1').files[0];
    otaElement('button-send').disabled = !(file && file.name);
    otaElement('status').innerHTML = file && file.name ? 'Ready to upload ' + file.name : 'Firmware upload';
    otaElement('loaded_n_total').innerHTML = '';
    });

    setUploadMode('firmware');
});
