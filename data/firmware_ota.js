let otaProgressPollTimer = null;

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
    return byId(id);
}

function disableAll() {
    const overlay = otaElement('overlay');
    if (overlay) {
        overlay.style.display = 'block';
    }
}

function enableAll() {
    const overlay = otaElement('overlay');
    if (overlay) {
        overlay.style.display = 'none';
    }
}

async function waitForReboot(targetUrl, retries) {
    if (retries <= 0) {
        enableAll();
        setText('status', 'Upload finished, but the device did not come back online yet. Please refresh manually.');
        return;
    }

    try {
        const result = await request('/staticdata.json?ts=' + Date.now());
        if (result.ok) {
            window.location.replace(targetUrl);
            return;
        }
    } catch (error) {
    }

    await delay(2000);
    waitForReboot(targetUrl, retries - 1);
}

function applyOtaProgress(progress) {
    if (!progress) {
        return;
    }

    const percent = progress.percent || 0;
    otaElement('progressBar').style.width = percent + '%';
    if (progress.total > 0) {
        setText('loaded_n_total', percent + '% (' + progress.current + ' / ' + progress.total + ' bytes)');
    }
    if (progress.message) {
        setText('status', progress.message);
    }
}

function stopOtaProgressPolling() {
    if (!otaProgressPollTimer) {
        return;
    }

    clearInterval(otaProgressPollTimer);
    otaProgressPollTimer = null;
}

async function pollOtaProgress() {
    try {
        const progress = await requestJson('/otaprogress?ts=' + Date.now());
        applyOtaProgress(progress);
        if (!progress.active && (progress.success || progress.phase === 'error')) {
            stopOtaProgressPolling();
        }
    } catch (error) {
    }
}

function startOtaProgressPolling() {
    stopOtaProgressPolling();
    pollOtaProgress();
    otaProgressPollTimer = setInterval(pollOtaProgress, 500);
}

function handleUploadProgress(event) {
    if (!event.lengthComputable) {
        return;
    }

    const percent = Math.round((event.loaded / event.total) * 100);
    setText('loaded_n_total', 'Uploaded ' + event.loaded + ' bytes of ' + event.total);
    otaElement('progressBar').style.width = percent + '%';
    setText('status', percent + '% uploaded... please wait');
}

function handleUploadComplete(event) {
    let response = null;
    try {
        response = JSON.parse(event.target.responseText || '{}');
    } catch (error) {
        response = null;
    }

    if (event.target.status >= 400) {
        enableAll();
        setText('status', response && response.message ? response.message : 'Upload failed');
        return;
    }

    setText('status', response && response.message ? response.message : 'Upload success. Waiting for device reboot...');
    otaElement('progressBar').style.width = '100%';
    setTimeout(function () {
        waitForReboot('/', 45);
    }, 3000);
}

function handleUploadError() {
    stopOtaProgressPolling();
    enableAll();
    setText('status', 'Upload failed');
}

function handleUploadAbort() {
    stopOtaProgressPolling();
    enableAll();
    setText('status', 'Upload aborted');
}

function uploadFile() {
    const file = otaElement('file1').files[0];
    if (!file) {
        return;
    }

    const formData = new FormData();
    formData.append(otaElement('file1').name, file, file.name);

    const xhr = new XMLHttpRequest();
    xhr.upload.addEventListener('progress', handleUploadProgress, false);
    xhr.addEventListener('load', handleUploadComplete, false);
    xhr.addEventListener('loadstart', function () {
        disableAll();
        startOtaProgressPolling();
    }, false);
    xhr.addEventListener('error', handleUploadError, false);
    xhr.addEventListener('abort', handleUploadAbort, false);
    xhr.open('POST', '/doUpdate');
    xhr.send(formData);
}

function setUploadMode(mode) {
    const isFilesystem = mode === 'filesystem';

    otaElement('file1').name = isFilesystem ? 'filesystem' : 'firmware';
    otaElement('firmware-button').classList.toggle('selected', !isFilesystem);
    otaElement('filesystem-button').classList.toggle('selected', isFilesystem);
    setText('status', isFilesystem ? 'File system upload' : 'Firmware upload');
}

document.addEventListener('DOMContentLoaded', function () {
    const urlParams = new URLSearchParams(window.location.search);
    const onlyFirmware = urlParams.get('onlyFirmware');

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
        const file = otaElement('file1').files[0];
        otaElement('button-send').disabled = !(file && file.name);
        setText('status', file && file.name ? 'Ready to upload ' + file.name : 'Firmware upload');
        setText('loaded_n_total', '');
    });

    setUploadMode('firmware');
});
