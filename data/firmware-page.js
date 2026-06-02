let otaProgressTimer = null;
let otaRebootWaitStarted = false;

function firmwareById(id) {
    return typeof byId === 'function' ? byId(id) : document.getElementById(id);
}

function firmwareSetElementsDisabled(selector, disabled, root) {
    if (typeof setElementsDisabled === 'function') {
        setElementsDisabled(selector, disabled, root);
        return;
    }

    const scope = root || document;
    const nodes = scope.querySelectorAll(selector);
    for (let i = 0; i < nodes.length; i += 1) {
        nodes[i].disabled = disabled;
    }
}

function firmwareDelay(milliseconds) {
    if (typeof delay === 'function') {
        return delay(milliseconds);
    }

    return new Promise(function (resolve) {
        setTimeout(resolve, milliseconds);
    });
}

async function firmwareRequest(url, options) {
    if (typeof request === 'function') {
        return request(url, options);
    }

    const response = await fetch(url, options || {});
    const text = await response.text();
    let json = null;
    if (text) {
        try {
            json = JSON.parse(text);
        } catch (error) {
            json = null;
        }
    }

    return {
        ok: response.ok,
        status: response.status,
        text: text,
        json: json
    };
}

async function firmwareRequestJson(url, options) {
    if (typeof requestJson === 'function') {
        return requestJson(url, options);
    }

    const result = await firmwareRequest(url, options);
    if (!result.ok) {
        throw result;
    }
    return result.json || {};
}

function otaElements() {
    return {
        loader: firmwareById('loader'),
        content: firmwareById('myDiv'),
        progressWrapper: firmwareById('otaProgressWrapper'),
        progressBar: firmwareById('otaProgressBar'),
        progressText: firmwareById('otaProgressText'),
        localFileInput: firmwareById('localFirmwareFile'),
        localInstallButton: firmwareById('localFirmwareButton'),
        stableInstallButton: firmwareById('stableFirmwareButton'),
        betaInstallButton: firmwareById('betaFirmwareButton'),
        mdsTestButton: firmwareById('mdsTestButton'),
        backButton: firmwareById('backToOverviewButton')
    };
}

function setLoadingState(isLoading, options) {
    const elements = otaElements();
    if (!elements.loader || !elements.content) {
        return;
    }

    const showLoader = !options || options.showLoader !== false;
    elements.loader.classList.toggle('hidden', !isLoading);
    elements.loader.style.display = isLoading && showLoader ? 'block' : 'none';
    elements.content.style.display = 'block';
    elements.content.style.opacity = isLoading && showLoader ? '0.55' : '1';
    elements.content.style.pointerEvents = isLoading ? 'none' : 'auto';
    firmwareSetElementsDisabled('button, input, select', isLoading, elements.content);
}

function renderOtaProgress(progress) {
    const elements = otaElements();
    if (!elements.progressWrapper || !elements.progressBar || !elements.progressText || !progress) {
        return;
    }

    if (progress.active || progress.percent > 0 || progress.message) {
        elements.progressWrapper.style.display = 'block';
    }

    elements.progressBar.style.width = (progress.percent || 0) + '%';
    elements.progressText.textContent = progress.message
        ? progress.message + ' (' + (progress.percent || 0) + '%)'
        : (progress.percent || 0) + '%';
}

function stopOtaProgressPolling() {
    if (!otaProgressTimer) {
        return;
    }

    clearInterval(otaProgressTimer);
    otaProgressTimer = null;
}

async function pollOtaProgress() {
    try {
        const progress = await firmwareRequestJson('/otaprogress?ts=' + Date.now());
        renderOtaProgress(progress);

        if (!progress.active && progress.success && progress.phase === 'complete') {
            stopOtaProgressPolling();
            beginWaitForRebootWithMessage(progress.message || 'Firmware update complete. Device restarts now...');
            return;
        }

        if (!progress.active && progress.phase === 'error') {
            stopOtaProgressPolling();
            otaRebootWaitStarted = false;
            setLoadingState(false);
            return;
        }

        if (!progress.active && (progress.success || progress.phase === 'error')) {
            stopOtaProgressPolling();
        }
    } catch (error) {
    }
}

function startOtaProgressPolling() {
    stopOtaProgressPolling();
    pollOtaProgress();
    otaProgressTimer = setInterval(pollOtaProgress, 500);
}

async function waitForReboot() {
    for (let retries = 45; retries > 0; retries -= 1) {
        await firmwareDelay(3000);

        try {
            const result = await firmwareRequest('/staticdata.json?ts=' + Date.now());
            if (result.ok) {
                window.location.replace('/');
                return;
            }
        } catch (error) {
        }
    }

    setLoadingState(false);
}

function beginWaitForRebootWithMessage(message) {
    if (otaRebootWaitStarted) {
        return;
    }

    otaRebootWaitStarted = true;
    renderOtaProgress({
        active: true,
        percent: 100,
        message: message || 'Firmware download complete. Device restarts now...'
    });

    firmwareDelay(2000).then(waitForReboot);
}

async function handleOtaResponseResult(result) {
    const response = result.json || {};

    if (result.status === 202 || response.status === 'queued') {
        renderOtaProgress({
            active: true,
            percent: 0,
            message: response.message || 'Remote firmware update queued on device...'
        });
        return;
    }

    if (!result.ok) {
        stopOtaProgressPolling();
        otaRebootWaitStarted = false;
        setLoadingState(false);
        alert((response && response.message) || 'Update failed.');
        return;
    }

    otaRebootWaitStarted = false;
    if (response.rebooting) {
        beginWaitForRebootWithMessage(response.message || 'Firmware update complete. Device restarts now...');
    } else {
        setLoadingState(false);
    }
}

function updateLocalFileButtonState() {
    const elements = otaElements();
    if (!elements.localFileInput || !elements.localInstallButton) {
        return;
    }

    const file = elements.localFileInput.files[0];
    const fileName = file ? file.name : '';
    const extension = fileName.split('.').pop().toLowerCase();
    const isAllowed = fileName && (extension === 'bin' || extension === 'bmb');

    elements.localInstallButton.disabled = !isAllowed;
    if (fileName && !isAllowed) {
        alert('Wrong file type!');
    }
}

function uploadLocalFirmware() {
    const elements = otaElements();
    const file = elements.localFileInput ? elements.localFileInput.files[0] : null;
    if (!file) {
        return;
    }

    if (!confirm('Upload and install this firmware file?')) {
        return;
    }

    const xhr = new XMLHttpRequest();
    const formData = new FormData();
    formData.append('firmware', file, file.name);

    xhr.open('POST', '/doUpdate', true);
    xhr.upload.onprogress = function (event) {
        if (!event.lengthComputable) {
            return;
        }

        renderOtaProgress({
            active: true,
            percent: Math.round((event.loaded / event.total) * 100),
            message: 'Uploading firmware...'
        });
    };
    xhr.onload = function () {
        let responseJson = null;
        try {
            responseJson = xhr.responseText ? JSON.parse(xhr.responseText) : null;
        } catch (error) {
            responseJson = null;
        }
        handleOtaResponseResult({
            ok: xhr.status >= 200 && xhr.status < 300,
            status: xhr.status,
            json: responseJson
        });
    };
    xhr.onerror = function () {
        stopOtaProgressPolling();
        otaRebootWaitStarted = false;
        setLoadingState(false);
        alert('Firmware upload failed.');
    };

    setLoadingState(true, { showLoader: false });
    startOtaProgressPolling();
    xhr.send(formData);
}

async function startRemoteUpdate(source, promptText) {
    if (!confirm(promptText)) {
        return;
    }

    setLoadingState(true, { showLoader: false });
    startOtaProgressPolling();

    try {
        const result = await firmwareRequest('/startRemoteUpdate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams({ source: source }).toString()
        });
        await handleOtaResponseResult(result);
    } catch (error) {
        stopOtaProgressPolling();
        otaRebootWaitStarted = false;
        setLoadingState(false);
        alert('Remote update request failed.');
    }
}

async function testMdsUpload() {
    setLoadingState(true);
    try {
        const result = await firmwareRequest('/testMdsUpload', { method: 'POST' });
        const response = result.json || {};
        if (!result.ok) {
            throw response.message || 'MDS test request failed.';
        }
        alert(response.message || 'MDS test upload sent successfully.');
    } catch (error) {
        alert(typeof error === 'string' ? error : 'MDS test request failed.');
    } finally {
        setLoadingState(false);
    }
}

document.addEventListener('DOMContentLoaded', function () {
    const elements = otaElements();
    setLoadingState(false);

    if (elements.localFileInput) {
        elements.localFileInput.addEventListener('change', updateLocalFileButtonState);
    }
    if (elements.localInstallButton) {
        elements.localInstallButton.addEventListener('click', uploadLocalFirmware);
    }
    if (elements.stableInstallButton) {
        elements.stableInstallButton.addEventListener('click', function () {
            startRemoteUpdate('stable', 'The device will download and install the current firmware from the configured update server. Continue?');
        });
    }
    if (elements.betaInstallButton) {
        elements.betaInstallButton.addEventListener('click', function () {
            startRemoteUpdate('beta', 'Please download your config before updating the firmware. The device will download and install the beta firmware. Continue?');
        });
    }
    if (elements.mdsTestButton) {
        elements.mdsTestButton.addEventListener('click', testMdsUpload);
    }
    if (elements.backButton) {
        elements.backButton.addEventListener('click', function () {
            window.open('/', '_self');
        });
    }
});
