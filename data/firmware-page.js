let otaProgressTimer = null;
let otaRebootWaitStarted = false;
let webFilesProgressTimer = null;
let lastOtaProgress = null;
let lastRenderedOtaPercent = 0;
let lastRenderedWebFilesPercent = 0;
let localUploadProgressActive = false;
let localUploadProgressPercent = 0;

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

function clampPercent(value) {
    const number = Number(value);
    if (!Number.isFinite(number)) {
        return 0;
    }
    return Math.max(0, Math.min(100, Math.round(number)));
}

function firmwareCsrfToken() {
    if (typeof csrfToken === 'function') {
        return csrfToken();
    }

    const meta = document.querySelector('meta[name="csrf-token"]');
    return meta ? meta.getAttribute('content') : '';
}

function firmwareWithCsrf(options) {
    const requestOptions = options || {};
    const method = (requestOptions.method || 'GET').toUpperCase();
    if (method === 'GET' || method === 'HEAD' || method === 'OPTIONS') {
        return requestOptions;
    }

    const token = firmwareCsrfToken();
    if (!token) {
        return requestOptions;
    }

    requestOptions.headers = requestOptions.headers || {};
    requestOptions.headers['X-CSRF-Token'] = token;
    return requestOptions;
}

async function firmwareRequest(url, options) {
    if (typeof request === 'function') {
        return request(url, options);
    }

    const response = await fetch(url, firmwareWithCsrf(options));
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
        progressDialog: firmwareById('otaProgressDialog'),
        progressDialogBar: firmwareById('otaProgressDialogBar'),
        progressDialogText: firmwareById('otaProgressDialogText'),
        progressDialogPercent: firmwareById('otaProgressDialogPercent'),
        localFileInput: firmwareById('localFirmwareFile'),
        localInstallButton: firmwareById('localFirmwareButton'),
        localWebBundleFileInput: firmwareById('localWebBundleFile'),
        localWebBundleButton: firmwareById('localWebBundleButton'),
        mdsOtaInstallButton: firmwareById('mdsOtaFirmwareButton'),
        mdsOtaVersion: firmwareById('mdsOtaVersion'),
        mdsOtaStatus: firmwareById('mdsOtaStatus'),
        mdsOtaDetail: firmwareById('mdsOtaDetail'),
        mdsTestButton: firmwareById('mdsTestButton'),
        message: firmwareById('firmwareMessage'),
        webFilesUpdateButton: firmwareById('webFilesUpdateButton'),
        webFilesInstalledVersion: firmwareById('webFilesInstalledVersion'),
        webFilesFirmwareVersion: firmwareById('webFilesFirmwareVersion'),
        webFilesStatus: firmwareById('webFilesStatus'),
        webFilesProgressWrapper: firmwareById('webFilesProgressWrapper'),
        webFilesProgressBar: firmwareById('webFilesProgressBar'),
        webFilesProgressText: firmwareById('webFilesProgressText'),
        backButton: firmwareById('backToOverviewButton')
    };
}

function showFirmwareMessage(type, text) {
    const elements = otaElements();
    if (!elements.message) {
        if (type === 'error') {
            alert(text);
        }
        return;
    }

    elements.message.className = 'notice notice-' + (type || 'info');
    elements.message.textContent = text || '';
    elements.message.classList.toggle('hidden', !text);
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

    const shouldShow = progress.active || progress.percent > 0 || progress.message;
    if (shouldShow) {
        elements.progressWrapper.style.display = 'block';
    }

    let percent = clampPercent(progress.percent);
    const phase = progress.phase || '';
    const uploadLikePhase = phase.indexOf('upload-') === 0 || phase === 'queued' || phase === 'idle';

    if (localUploadProgressActive && uploadLikePhase) {
        percent = Math.max(percent, localUploadProgressPercent);
    }

    if (shouldShow && percent < lastRenderedOtaPercent && phase !== 'error' && phase !== 'complete') {
        percent = lastRenderedOtaPercent;
    }

    lastRenderedOtaPercent = shouldShow ? percent : 0;
    elements.progressBar.style.width = percent + '%';
    elements.progressText.textContent = progress.message
        ? progress.message + ' (' + percent + '%)'
        : percent + '%';

    if (elements.progressDialog && elements.progressDialogBar && elements.progressDialogText && elements.progressDialogPercent) {
        elements.progressDialog.classList.toggle('hidden', !shouldShow);
        elements.progressDialogBar.style.width = percent + '%';
        elements.progressDialogPercent.textContent = percent + '%';
        elements.progressDialogText.textContent = progress.message || 'Firmware update is running...';
    }
}

function hideOtaProgressDialog() {
    const elements = otaElements();
    if (elements.progressDialog) {
        elements.progressDialog.classList.add('hidden');
    }
    lastRenderedOtaPercent = 0;
    localUploadProgressActive = false;
    localUploadProgressPercent = 0;
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
        lastOtaProgress = progress;
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
            hideOtaProgressDialog();
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
    otaProgressTimer = setInterval(pollOtaProgress, 800);
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
        hideOtaProgressDialog();
        showFirmwareMessage('error', (response && response.message) || 'Update failed.');
        return;
    }

    otaRebootWaitStarted = false;
    if (response.rebooting) {
        beginWaitForRebootWithMessage(response.message || 'Firmware update complete. Device restarts now...');
    } else {
        setLoadingState(false);
        hideOtaProgressDialog();
        if (response && response.message) {
            showFirmwareMessage('success', response.message);
        }
        refreshWebFilesInfo();
        if (response && response.message && response.message.indexOf('Web package installed successfully') >= 0) {
            firmwareDelay(1200).then(function () {
                window.location.reload();
            });
        }
    }
}

function updateFileButtonState(fileInput, button, validatorMessage, allowedExtensions) {
    if (!fileInput || !button) {
        return;
    }

    const file = fileInput.files[0];
    const fileName = file ? file.name : '';
    const extension = fileName.split('.').pop().toLowerCase();
    const isAllowed = fileName && allowedExtensions.indexOf(extension) >= 0;

    button.disabled = !isAllowed;
    if (fileName && !isAllowed) {
        showFirmwareMessage('error', validatorMessage);
    }
}

function updateLocalFirmwareButtonState() {
    const elements = otaElements();
    updateFileButtonState(
        elements.localFileInput,
        elements.localInstallButton,
        'Wrong file type. Please select a .bin or .bmb firmware file.',
        ['bin', 'bmb']
    );
}

function updateLocalWebBundleButtonState() {
    const elements = otaElements();
    updateFileButtonState(
        elements.localWebBundleFileInput,
        elements.localWebBundleButton,
        'Wrong file type. Please select a web package .tar file.',
        ['tar']
    );
}

function uploadLocalPackage(options) {
    const elements = otaElements();
    const fileInput = options && options.fileInput;
    const fieldName = options && options.fieldName ? options.fieldName : 'firmware';
    const progressMessage = options && options.progressMessage ? options.progressMessage : 'Uploading update...';
    const confirmMessage = options && options.confirmMessage ? options.confirmMessage : 'Upload and install this update file?';
    const failureMessage = options && options.failureMessage ? options.failureMessage : 'Upload failed.';
    const endpoint = options && options.endpoint ? options.endpoint : '/doUpdate';
    const installMessage = options && options.installMessage ? options.installMessage : 'Installing update on device...';
    const file = fileInput ? fileInput.files[0] : null;
    let uploadFinished = false;
    if (!file) {
        return;
    }

    if (!confirm(confirmMessage)) {
        return;
    }

    const xhr = new XMLHttpRequest();
    const formData = new FormData();
    formData.append(fieldName, file, file.name);
    formData.append('csrf', firmwareCsrfToken());

    xhr.open('POST', endpoint, true);
    xhr.timeout = 180000;
    xhr.setRequestHeader('X-CSRF-Token', firmwareCsrfToken());
    xhr.upload.onprogress = function (event) {
        if (!event.lengthComputable) {
            return;
        }

        const percent = Math.round((event.loaded / event.total) * 100);
        localUploadProgressActive = true;
        localUploadProgressPercent = Math.max(localUploadProgressPercent, percent);
        renderOtaProgress({
            active: true,
            percent: percent,
            phase: 'upload-local',
            message: progressMessage
        });

        if (event.loaded >= event.total) {
            uploadFinished = true;
            localUploadProgressPercent = 100;
            renderOtaProgress({
                active: true,
                percent: 100,
                phase: 'upload-local',
                message: installMessage
            });
        }
    };
    xhr.upload.onload = function () {
        uploadFinished = true;
        localUploadProgressActive = true;
        localUploadProgressPercent = 100;
        renderOtaProgress({
            active: true,
            percent: 100,
            phase: 'upload-local',
            message: installMessage
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
    function handleInterruptedUpload() {
        const progress = lastOtaProgress || {};
        const likelyDeviceRestart =
            uploadFinished ||
            progress.phase === 'install-web-package' ||
            progress.phase === 'finalizing' ||
            (progress.success && progress.phase === 'complete');

        if (likelyDeviceRestart) {
            beginWaitForRebootWithMessage((progress && progress.message) || 'Update transferred. Waiting for device restart...');
            return;
        }

        stopOtaProgressPolling();
        otaRebootWaitStarted = false;
        setLoadingState(false);
        hideOtaProgressDialog();
        showFirmwareMessage('error', failureMessage);
    }
    xhr.onerror = handleInterruptedUpload;
    xhr.ontimeout = handleInterruptedUpload;
    xhr.onabort = handleInterruptedUpload;

    setLoadingState(true, { showLoader: false });
    startOtaProgressPolling();
    xhr.send(formData);
}

function uploadLocalFirmware() {
    const elements = otaElements();
    uploadLocalPackage({
        fileInput: elements.localFileInput,
        fieldName: 'firmware',
        progressMessage: 'Uploading firmware...',
        confirmMessage: 'Upload and install this firmware file?',
        failureMessage: 'Firmware upload failed.'
    });
}

function uploadLocalWebBundle() {
    const elements = otaElements();
    uploadLocalPackage({
        fileInput: elements.localWebBundleFileInput,
        fieldName: 'webbundle',
        progressMessage: 'Uploading web package...',
        installMessage: 'Installing web package on device...',
        confirmMessage: 'Upload and install this web package? Only known web files will be replaced.',
        failureMessage: 'Web package upload failed.',
        endpoint: '/uploadWebBundle'
    });
}

async function startRemoteUpdate(source, promptText) {
    if (!confirm(promptText)) {
        return;
    }

    setLoadingState(true, { showLoader: false });
    startOtaProgressPolling();

    try {
        const body = new URLSearchParams({
            source: source,
            csrf: firmwareCsrfToken()
        });
        const result = await firmwareRequest('/startRemoteUpdate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body.toString()
        });
        await handleOtaResponseResult(result);
    } catch (error) {
        stopOtaProgressPolling();
        otaRebootWaitStarted = false;
        setLoadingState(false);
        hideOtaProgressDialog();
        showFirmwareMessage('error', 'Remote update request failed.');
    }
}

async function testMdsUpload() {
    setLoadingState(true);
    try {
        const result = await firmwareRequest('/testMdsUpload', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams({ csrf: firmwareCsrfToken() }).toString()
        });
        const response = result.json || {};
        if (!result.ok) {
            throw response.message || 'MDS test request failed.';
        }
        showFirmwareMessage('success', response.message || 'MDS test upload sent successfully.');
    } catch (error) {
        showFirmwareMessage('error', typeof error === 'string' ? error : 'MDS test request failed.');
    } finally {
        setLoadingState(false);
    }
}

function renderMdsOtaInfo(info) {
    const elements = otaElements();
    if (!elements.mdsOtaStatus || !info) {
        return;
    }

    const version = info.version || '-';
    setText('mdsOtaVersion', version || '-');

    let label = 'Unknown';
    if (info.status === 'update-available') {
        label = 'Update available';
    } else if (info.status === 'current') {
        label = 'Up to date';
    } else if (info.status === 'not-configured') {
        label = 'Not configured';
    } else if (info.status === 'offline') {
        label = 'Offline';
    } else if (info.status === 'forbidden') {
        label = 'Rejected';
    } else if (info.status === 'error') {
        label = 'Error';
    }
    setText('mdsOtaStatus', label);

    if (elements.mdsOtaDetail) {
        let detail = info.message || 'The device asks MDS for an update and sends the configured OTA secret as a request header.';
        if (info.status === 'current' && !info.version && info.installedVersion) {
            detail += ' Installed version on device: ' + info.installedVersion + '.';
        }
        if (info.versionMismatchMessage) {
            detail += ' ' + info.versionMismatchMessage;
        } else if (info.versionSource === 'metadata' && info.headerVersion && info.headerVersion !== info.version) {
            detail += ' OTA header still reports ' + info.headerVersion + '.';
        }
        elements.mdsOtaDetail.textContent = detail;
    }

    if (elements.mdsOtaInstallButton) {
        elements.mdsOtaInstallButton.disabled = info.status !== 'update-available';
    }
}

async function refreshMdsOtaInfo() {
    try {
        const info = await firmwareRequestJson('/mdsotainfo?ts=' + Date.now());
        renderMdsOtaInfo(info);
    } catch (error) {
        setText('mdsOtaVersion', '-');
        setText('mdsOtaStatus', 'Unknown');
    }
}

function renderWebFilesInfo(info) {
    const elements = otaElements();
    if (!info || !elements.webFilesStatus) {
        return;
    }

    setText('webFilesInstalledVersion', info.storedWebFilesVersion || '-');
    setText('webFilesFirmwareVersion', info.firmwareVersion || '-');
    setText('webFilesStatus', info.error ? 'Error' : (info.retrying ? 'Retrying...' : (info.busy ? 'Updating...' : (info.upToDate ? 'Up to date' : 'Update available'))));

    if (elements.webFilesUpdateButton) {
        const serverUnavailableForVersion = info.configured && info.serverSupportsInstalledFirmware === false;
        const localPackageMode = serverUnavailableForVersion || !info.configured;
        elements.webFilesUpdateButton.disabled = info.busy;
        elements.webFilesUpdateButton.textContent = serverUnavailableForVersion
            ? 'Use Local Web Package'
            : (!info.configured
                ? 'MDS OTA Missing'
                : (info.error
                    ? 'Retry Web Files Download'
                    : (info.upToDate ? 'Reinstall Web Files' : 'Get Files from Server')));
        elements.webFilesUpdateButton.dataset.mode = localPackageMode ? 'local-package' : 'server-download';
    }
}

async function refreshWebFilesInfo() {
    try {
        const info = await firmwareRequestJson('/updatefilesinfo?ts=' + Date.now());
        renderWebFilesInfo(info);
    } catch (error) {
        setText('webFilesStatus', 'Unknown');
    }
}

function renderWebFilesProgress(progress) {
    const elements = otaElements();
    if (!elements.webFilesProgressWrapper || !elements.webFilesProgressBar || !elements.webFilesProgressText || !progress) {
        return;
    }

    let percent = clampPercent(progress.percent);
    if ((progress.busy || progress.retrying) && percent < lastRenderedWebFilesPercent) {
        percent = lastRenderedWebFilesPercent;
    }
    lastRenderedWebFilesPercent = (progress.busy || progress.retrying || percent > 0) ? percent : 0;
    elements.webFilesProgressWrapper.style.display = progress.busy || percent > 0 || progress.message ? 'block' : 'none';
    elements.webFilesProgressBar.style.width = percent + '%';
    const showPercent = Number(progress.total) > 0 && !progress.retrying;
    elements.webFilesProgressText.textContent = progress.message
        ? (showPercent ? progress.message + ' (' + percent + '%)' : progress.message)
        : (showPercent ? percent + '%' : '');
}

function stopWebFilesProgressPolling() {
    if (!webFilesProgressTimer) {
        return;
    }

    clearInterval(webFilesProgressTimer);
    webFilesProgressTimer = null;
    lastRenderedWebFilesPercent = 0;
}

async function pollWebFilesProgress() {
    try {
        const progress = await firmwareRequestJson('/updatefilesprogress?ts=' + Date.now());
        renderWebFilesProgress(progress);
        await refreshWebFilesInfo();

        if (!progress.busy && (progress.error || progress.percent >= 100)) {
            stopWebFilesProgressPolling();
            showFirmwareMessage(progress.error ? 'error' : 'success', progress.message || 'Web interface files updated.');
            if (!progress.error && progress.percent >= 100) {
                firmwareDelay(1200).then(function () {
                    window.location.reload();
                });
            }
        }
    } catch (error) {
    }
}

function startWebFilesProgressPolling() {
    stopWebFilesProgressPolling();
    pollWebFilesProgress();
    webFilesProgressTimer = setInterval(pollWebFilesProgress, 700);
}

async function startWebFilesUpdate() {
    const elements = otaElements();
    const buttonMode = elements.webFilesUpdateButton ? elements.webFilesUpdateButton.dataset.mode : '';

    if (buttonMode === 'local-package') {
        if (elements.localWebBundleFileInput) {
            showFirmwareMessage('info', 'Please choose the local web package (.tar) above and install it.');
            elements.localWebBundleFileInput.scrollIntoView({ behavior: 'smooth', block: 'center' });
            elements.localWebBundleFileInput.click();
        } else {
            showFirmwareMessage('error', 'Local web package upload is not available on this page.');
        }
        return;
    }

    if (!confirm('Download web interface files for the installed firmware from the MDS OTA web release?')) {
        return;
    }

    showFirmwareMessage('info', 'Starting web interface file download...');
    try {
        const body = new URLSearchParams({ csrf: firmwareCsrfToken() });
        const result = await firmwareRequest('/updatefiles', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body.toString()
        });
        if (!result.ok) {
            throw result;
        }
        startWebFilesProgressPolling();
    } catch (error) {
        const message = error && error.json && error.json.message ? error.json.message : 'Could not start web interface file download.';
        showFirmwareMessage('error', message);
    }
}

document.addEventListener('DOMContentLoaded', function () {
    const elements = otaElements();
    setLoadingState(false);

    if (elements.localFileInput) {
        elements.localFileInput.addEventListener('change', updateLocalFirmwareButtonState);
    }
    if (elements.localInstallButton) {
        elements.localInstallButton.addEventListener('click', uploadLocalFirmware);
    }
    if (elements.localWebBundleFileInput) {
        elements.localWebBundleFileInput.addEventListener('change', updateLocalWebBundleButtonState);
    }
    if (elements.localWebBundleButton) {
        elements.localWebBundleButton.addEventListener('click', uploadLocalWebBundle);
    }
    if (elements.mdsOtaInstallButton) {
        elements.mdsOtaInstallButton.addEventListener('click', function () {
            startRemoteUpdate('mds', 'The device will ask MDS for a firmware update using the configured OTA secret. Continue?');
        });
    }
    if (elements.mdsTestButton) {
        elements.mdsTestButton.addEventListener('click', testMdsUpload);
    }
    if (elements.webFilesUpdateButton) {
        elements.webFilesUpdateButton.addEventListener('click', startWebFilesUpdate);
    }
    if (elements.backButton) {
        elements.backButton.addEventListener('click', function () {
            window.open('/', '_self');
        });
    }
    refreshWebFilesInfo();
    refreshMdsOtaInfo();
});
