let otaProgressTimer = null;
let otaRebootWaitStarted = false;
let webFilesProgressTimer = null;
let otaProgressPollingActive = false;
let webFilesProgressPollingActive = false;
let lastOtaProgress = null;
let lastRenderedOtaPercent = 0;
let lastRenderedWebFilesPercent = 0;
let localUploadProgressActive = false;
let localUploadProgressPercent = 0;
const FIRMWARE_REQUEST_TIMEOUT_MS = 8000;
const FIRMWARE_DIAGNOSTICS_TIMEOUT_MS = 30000;

function firmwareById(id) {
    return typeof byId === 'function' ? byId(id) : document.getElementById(id);
}

function firmwareSetText(id, value) {
    if (typeof setText === 'function') {
        setText(id, value);
        return;
    }

    const element = firmwareById(id);
    if (!element) {
        return;
    }
    element.textContent = value == null ? '' : value;
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
    const controller = typeof AbortController !== 'undefined' ? new AbortController() : null;
    const incomingOptions = options || {};
    const timeoutMs = incomingOptions.timeoutMs || FIRMWARE_REQUEST_TIMEOUT_MS;
    const requestOptions = firmwareWithCsrf(Object.assign({}, incomingOptions));
    delete requestOptions.timeoutMs;
    requestOptions.cache = 'no-store';
    requestOptions.credentials = 'same-origin';
    let timeoutId = null;
    if (controller) {
        requestOptions.signal = controller.signal;
        timeoutId = setTimeout(function () {
            controller.abort();
        }, timeoutMs);
    }

    let response;
    try {
        response = await fetch(url, requestOptions);
    } catch (error) {
        if (timeoutId) {
            clearTimeout(timeoutId);
        }

        if (error && error.name === 'AbortError') {
            return {
                ok: false,
                status: 0,
                text: '',
                json: { message: 'Request timed out.' }
            };
        }
        throw error;
    }

    if (timeoutId) {
        clearTimeout(timeoutId);
    }

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
        localFileName: firmwareById('localFirmwareFileName'),
        localInstallButton: firmwareById('localFirmwareButton'),
        localWebBundleFileInput: firmwareById('localWebBundleFile'),
        localWebBundleFileName: firmwareById('localWebBundleFileName'),
        localWebBundleButton: firmwareById('localWebBundleButton'),
        releaseOtaInstallButton: firmwareById('releaseOtaFirmwareButton'),
        releaseOtaVersion: firmwareById('releaseOtaVersion'),
        releaseOtaStatus: firmwareById('releaseOtaStatus'),
        releaseOtaDetail: firmwareById('releaseOtaDetail'),
        betaOtaInstallButton: firmwareById('betaOtaFirmwareButton'),
        betaOtaVersion: firmwareById('betaOtaVersion'),
        betaOtaStatus: firmwareById('betaOtaStatus'),
        betaOtaDetail: firmwareById('betaOtaDetail'),
        mdsTestButton: firmwareById('mdsTestButton'),
        message: firmwareById('firmwareMessage'),
        webFilesUpdateButton: firmwareById('webFilesUpdateButton'),
        webFilesInstalledVersion: firmwareById('webFilesInstalledVersion'),
        webFilesFirmwareVersion: firmwareById('webFilesFirmwareVersion'),
        webFilesStatus: firmwareById('webFilesStatus'),
        webFilesProgressWrapper: firmwareById('webFilesProgressWrapper'),
        webFilesProgressBar: firmwareById('webFilesProgressBar'),
        webFilesProgressText: firmwareById('webFilesProgressText'),
        backButton: firmwareById('backToOverviewButton'),
        otaDiagnosticsButton: firmwareById('otaDiagnosticsButton'),
        otaDiagWifi: firmwareById('otaDiagWifi'),
        otaDiagTime: firmwareById('otaDiagTime'),
        otaDiagEndpoint: firmwareById('otaDiagEndpoint'),
        otaDiagBeta: firmwareById('otaDiagBeta'),
        otaDiagRelease: firmwareById('otaDiagRelease'),
        otaDiagManifest: firmwareById('otaDiagManifest'),
        otaDiagnosticsDetail: firmwareById('otaDiagnosticsDetail')
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
    otaProgressPollingActive = false;
    if (otaProgressTimer) {
        clearTimeout(otaProgressTimer);
    }
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

        if (!progress.active && progress.success && progress.phase === 'no-update') {
            stopOtaProgressPolling();
            otaRebootWaitStarted = false;
            setLoadingState(false);
            hideOtaProgressDialog();
            showFirmwareMessage('success', progress.message || 'The installed firmware is already current.');
            await refreshFirmwarePageStatus();
            return;
        }

        if (!progress.active && (progress.success || progress.phase === 'error')) {
            stopOtaProgressPolling();
        }
    } catch (error) {
    }
    if (otaProgressPollingActive) {
        otaProgressTimer = setTimeout(pollOtaProgress, 800);
    }
}

function startOtaProgressPolling() {
    stopOtaProgressPolling();
    otaProgressPollingActive = true;
    otaProgressTimer = setTimeout(pollOtaProgress, 0);
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

function getSelectedFileMeta(fileInput) {
    if (!fileInput) {
        return { name: '', extension: '' };
    }

    const file = fileInput.files && fileInput.files.length ? fileInput.files[0] : null;
    const fileName = file && file.name
        ? file.name
        : String(fileInput.value || '').split(/[/\\]/).pop();
    const parts = fileName.split('.');
    const extension = parts.length > 1 ? parts.pop().toLowerCase() : '';

    return {
        name: fileName,
        extension: extension
    };
}

function updateFileSelectionLabel(labelElement, fileName, emptyText) {
    if (!labelElement) {
        return;
    }

    labelElement.textContent = fileName || emptyText;
}

function updateFileButtonState(fileInput, button, labelElement, validatorMessage, allowedExtensions, emptyText) {
    if (!fileInput || !button) {
        return false;
    }

    const selection = getSelectedFileMeta(fileInput);
    const fileName = selection.name;
    const extension = selection.extension;
    const isAllowed = fileName && allowedExtensions.indexOf(extension) >= 0;

    updateFileSelectionLabel(labelElement, fileName, emptyText);
    button.disabled = !isAllowed;
    if (fileName && !isAllowed) {
        showFirmwareMessage('error', validatorMessage);
    } else if (isAllowed) {
        showFirmwareMessage('', '');
    }

    return Boolean(isAllowed);
}

function updateLocalFirmwareButtonState() {
    const elements = otaElements();
    return updateFileButtonState(
        elements.localFileInput,
        elements.localInstallButton,
        elements.localFileName,
        'Wrong file type. Please select a .bin or .bmb firmware file.',
        ['bin', 'bmb'],
        'No firmware file selected.'
    );
}

function updateLocalWebBundleButtonState() {
    const elements = otaElements();
    return updateFileButtonState(
        elements.localWebBundleFileInput,
        elements.localWebBundleButton,
        elements.localWebBundleFileName,
        'Wrong file type. Please select a web package .tar file.',
        ['tar'],
        'No web package selected.'
    );
}

function syncLocalFileButtonStates() {
    updateLocalFirmwareButtonState();
    updateLocalWebBundleButtonState();
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
    // The ESP validates the CSRF token while parsing the multipart stream,
    // so the token field must arrive before the uploaded file field.
    formData.append('csrf', firmwareCsrfToken());
    formData.append(fieldName, file, file.name);

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
    if (!updateLocalFirmwareButtonState()) {
        showFirmwareMessage('error', 'Please select a .bin or .bmb firmware file first.');
        return;
    }

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
    if (!updateLocalWebBundleButtonState()) {
        showFirmwareMessage('error', 'Please select a .tar web package first.');
        return;
    }

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

async function startRemoteUpdate(source, promptText, force) {
    if (!confirm(promptText)) {
        return;
    }

    setLoadingState(true, { showLoader: false });
    startOtaProgressPolling();

    try {
        const body = new URLSearchParams({
            source: source,
            csrf: firmwareCsrfToken(),
            force: force ? '1' : '0'
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
        showFirmwareMessage('info', response.message || 'MDS test upload is running.');

        for (let attempt = 0; attempt < 30; attempt += 1) {
            await firmwareDelay(1000);
            const statusResult = await firmwareRequest('/testMdsUploadStatus', { cache: 'no-store' });
            const status = statusResult.json || {};
            if (!statusResult.ok) {
                throw status.message || 'MDS test status request failed.';
            }
            if (status.complete) {
                if (!status.success) {
                    throw status.message || 'MDS test upload failed.';
                }
                showFirmwareMessage('success', status.message || 'MDS test upload sent successfully.');
                return;
            }
        }
        throw 'MDS test upload timed out.';
    } catch (error) {
        showFirmwareMessage('error', typeof error === 'string' ? error : 'MDS test request failed.');
    } finally {
        setLoadingState(false);
    }
}

function otaInfoTarget(channel) {
    const elements = otaElements();
    if (channel === 'release') {
        return {
            version: elements.releaseOtaVersion,
            status: elements.releaseOtaStatus,
            detail: elements.releaseOtaDetail,
            button: elements.releaseOtaInstallButton
        };
    }

    return {
        version: elements.betaOtaVersion,
        status: elements.betaOtaStatus,
        detail: elements.betaOtaDetail,
        button: elements.betaOtaInstallButton
    };
}

function otaChannelLabel(channel) {
    return channel === 'release' ? 'Release' : 'Beta';
}

function renderMdsOtaInfo(channel, info) {
    const target = otaInfoTarget(channel);
    if (!target.status || !info) {
        return;
    }

    const version = info.version || '-';
    if (target.version) {
        target.version.textContent = version || '-';
    }

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
    target.status.textContent = label;

    if (target.detail) {
        let detail = info.message || 'No OTA details available.';
        if (info.installedVersion && info.installedChannelLabel) {
            detail += ' Installed: ' + info.installedVersion + ' (' + info.installedChannelLabel + ').';
        }
        target.detail.textContent = detail;
    }

    if (target.button) {
        target.button.dataset.status = info.status || 'unknown';
        target.button.dataset.force = info.version === info.installedVersion &&
            info.requestedChannel !== info.installedChannel ? '1' : '0';
        target.button.disabled = info.status === 'not-configured' ||
            info.status === 'offline' ||
            info.status === 'forbidden' ||
            info.status === 'current';
        target.button.textContent = info.status === 'current'
            ? otaChannelLabel(channel) + ' Firmware Current'
            : 'Install ' + otaChannelLabel(channel) + ' Firmware';
    }
}

async function refreshMdsOtaInfo(channel) {
    const target = otaInfoTarget(channel);
    try {
        for (let attempt = 0; attempt < 45; attempt += 1) {
            const info = await firmwareRequestJson('/mdsotainfo?channel=' + encodeURIComponent(channel) + (attempt === 0 ? '&refresh=1' : '') + '&ts=' + Date.now(), {
                timeoutMs: FIRMWARE_REQUEST_TIMEOUT_MS
            });
            if (!info.running) {
                renderMdsOtaInfo(channel, info);
                return info;
            }
            if (target.detail) {
                target.detail.textContent = info.message || 'Checking MDS OTA metadata...';
            }
            await firmwareDelay(700);
        }
        throw new Error('MDS OTA metadata check timed out.');
    } catch (error) {
        if (target.version) {
            target.version.textContent = '-';
        }
        if (target.status) {
            target.status.textContent = 'Error';
        }
        if (target.detail) {
            const message = error && error.json && error.json.message
                ? error.json.message
                : 'MDS OTA status request failed.';
            target.detail.textContent = message;
        }
        if (target.button) {
            target.button.dataset.status = 'error';
            target.button.disabled = false;
        }
        return null;
    }
}

function renderWebFilesInfo(info) {
    const elements = otaElements();
    if (!info || !elements.webFilesStatus) {
        return;
    }

    const storedVersion = info.storedWebFilesVersion || '-';
    const storedChannel = info.storedWebFilesChannel ? ' (' + info.storedWebFilesChannel + ')' : '';
    const firmwareVersion = info.firmwareVersion || '-';
    const firmwareChannel = info.firmwareChannelLabel ? ' (' + info.firmwareChannelLabel + ')' : '';

    firmwareSetText('webFilesInstalledVersion', storedVersion + storedChannel);
    firmwareSetText('webFilesFirmwareVersion', firmwareVersion + firmwareChannel);
    firmwareSetText('webFilesStatus', info.error ? 'Error' : (info.retrying ? 'Retrying...' : (info.busy ? 'Updating...' : (info.upToDate ? 'Up to date' : 'Update available'))));

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
        const info = await firmwareRequestJson('/updatefilesinfo?ts=' + Date.now(), {
            timeoutMs: FIRMWARE_DIAGNOSTICS_TIMEOUT_MS
        });
        renderWebFilesInfo(info);
        return info;
    } catch (error) {
        firmwareSetText('webFilesStatus', 'Error');
        firmwareSetText('webFilesInstalledVersion', '-');
        firmwareSetText('webFilesFirmwareVersion', '-');
        renderWebFilesProgress({
            busy: false,
            percent: 0,
            message: error && error.json && error.json.message
                ? error.json.message
                : 'Web file status request failed.'
        });
        return null;
    }
}

async function refreshFirmwarePageStatus() {
    firmwareSetText('betaOtaStatus', 'Checking...');
    firmwareSetText('releaseOtaStatus', 'Checking...');
    firmwareSetText('webFilesStatus', 'Checking...');

    // The ESP performs HTTPS checks for these endpoints. Run them in a small
    // queue instead of parallelizing them from the browser, otherwise the first
    // page load can overwhelm TLS/heap and show false diagnostic errors.
    await refreshMdsOtaInfo('beta').catch(function () {});
    await firmwareDelay(250);
    await refreshMdsOtaInfo('release').catch(function () {});
    await firmwareDelay(250);
    await refreshWebFilesInfo().catch(function () {});
    await firmwareDelay(750);
    await refreshOtaDiagnostics({ retry: true }).catch(function () {});
}

function renderDiagnosticStatus(id, check) {
    const ok = check && check.ok;
    const label = check && check.statusLabel ? check.statusLabel : (ok ? 'OK' : 'Error');
    firmwareSetText(id, label);
}

function renderOtaDiagnostics(info) {
    if (!info) {
        return;
    }

    renderDiagnosticStatus('otaDiagWifi', info.wifi);
    renderDiagnosticStatus('otaDiagTime', info.time);
    renderDiagnosticStatus('otaDiagEndpoint', info.endpoint);
    renderDiagnosticStatus('otaDiagBeta', info.betaMetadata);
    renderDiagnosticStatus('otaDiagRelease', info.releaseMetadata);
    renderDiagnosticStatus('otaDiagManifest', info.manifest);

    const details = [];
    ['wifi', 'time', 'endpoint', 'betaMetadata', 'releaseMetadata', 'manifest'].forEach(function (key) {
        const item = info[key];
        if (!item) {
            return;
        }
        details.push((item.label || key) + ': ' + (item.message || (item.ok ? 'OK' : 'Error')));
    });
    firmwareSetText('otaDiagnosticsDetail', details.join(' | '));
}

async function refreshOtaDiagnostics(options) {
    const elements = otaElements();
    if (elements.otaDiagnosticsDetail) {
        elements.otaDiagnosticsDetail.textContent = 'Checking OTA update path...';
    }

    try {
        const refresh = !(options && options.pollOnly);
        for (let attempt = 0; attempt < 45; attempt += 1) {
            const info = await firmwareRequestJson('/otadiagnostics?' + (refresh && attempt === 0 ? 'refresh=1&' : '') + 'ts=' + Date.now(), {
                timeoutMs: FIRMWARE_REQUEST_TIMEOUT_MS
            });
            if (!info.running) {
                renderOtaDiagnostics(info);
                return info;
            }
            firmwareSetText('otaDiagnosticsDetail', info.message || 'OTA diagnostics are running...');
            await firmwareDelay(700);
        }
        throw new Error('OTA diagnostics timed out.');
    } catch (error) {
        if (options && options.retry) {
            if (elements.otaDiagnosticsDetail) {
                elements.otaDiagnosticsDetail.textContent = 'First diagnostic request timed out, retrying...';
            }
            await firmwareDelay(2500);
            return refreshOtaDiagnostics({ retry: false, pollOnly: true });
        }

        firmwareSetText('otaDiagWifi', 'Error');
        firmwareSetText('otaDiagTime', 'Error');
        firmwareSetText('otaDiagEndpoint', 'Error');
        firmwareSetText('otaDiagBeta', 'Error');
        firmwareSetText('otaDiagRelease', 'Error');
        firmwareSetText('otaDiagManifest', 'Error');
        firmwareSetText('otaDiagnosticsDetail', 'OTA diagnostics request failed.');
        return null;
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
    webFilesProgressPollingActive = false;
    if (webFilesProgressTimer) {
        clearTimeout(webFilesProgressTimer);
    }
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
    if (webFilesProgressPollingActive) {
        webFilesProgressTimer = setTimeout(pollWebFilesProgress, 700);
    }
}

function startWebFilesProgressPolling() {
    stopWebFilesProgressPolling();
    webFilesProgressPollingActive = true;
    webFilesProgressTimer = setTimeout(pollWebFilesProgress, 0);
}

async function startWebFilesUpdate() {
    const elements = otaElements();
    const buttonMode = elements.webFilesUpdateButton ? elements.webFilesUpdateButton.dataset.mode : '';

    if (buttonMode === 'local-package') {
        showFirmwareMessage('error', 'No matching web package is available on the update server. Use the separate "Manual Local Web Package" section if you want to install a local .tar file.');
        return;
    }

    if (!confirm('Download web interface files for the installed firmware from the MDS OTA web release?')) {
        return;
    }

    showFirmwareMessage('info', 'Starting web interface file download...');
    try {
        const body = new URLSearchParams({ csrf: firmwareCsrfToken(), force: '1' });
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
    if (typeof setHtmlPageAssetVersion === 'function') {
        setHtmlPageAssetVersion(document.body ? document.body.getAttribute('data-firmware-version') : '');
    }
    setLoadingState(false);

    if (elements.localFileInput) {
        elements.localFileInput.addEventListener('change', updateLocalFirmwareButtonState);
        elements.localFileInput.addEventListener('input', updateLocalFirmwareButtonState);
        elements.localFileInput.addEventListener('click', function () {
            setTimeout(updateLocalFirmwareButtonState, 250);
        });
    }
    if (elements.localInstallButton) {
        elements.localInstallButton.addEventListener('click', uploadLocalFirmware);
    }
    if (elements.localWebBundleFileInput) {
        elements.localWebBundleFileInput.addEventListener('change', updateLocalWebBundleButtonState);
        elements.localWebBundleFileInput.addEventListener('input', updateLocalWebBundleButtonState);
        elements.localWebBundleFileInput.addEventListener('click', function () {
            setTimeout(updateLocalWebBundleButtonState, 250);
        });
    }
    if (elements.localWebBundleButton) {
        elements.localWebBundleButton.addEventListener('click', uploadLocalWebBundle);
    }
    if (elements.releaseOtaInstallButton) {
        elements.releaseOtaInstallButton.addEventListener('click', function () {
            const force = elements.releaseOtaInstallButton.dataset.force === '1';
            startRemoteUpdate(
                'mds-release',
                'The device will download and install the current release firmware from MDS. Continue?',
                force
            );
        });
    }
    if (elements.betaOtaInstallButton) {
        elements.betaOtaInstallButton.addEventListener('click', function () {
            const force = elements.betaOtaInstallButton.dataset.force === '1';
            startRemoteUpdate(
                'mds-beta',
                'The device will download and install the current beta firmware from MDS. Continue?',
                force
            );
        });
    }
    if (elements.mdsTestButton) {
        elements.mdsTestButton.addEventListener('click', testMdsUpload);
    }
    if (elements.webFilesUpdateButton) {
        elements.webFilesUpdateButton.addEventListener('click', startWebFilesUpdate);
    }
    // Back to Overview is a plain link in the HTML, so it also works if JS is slow.
    if (elements.otaDiagnosticsButton) {
        elements.otaDiagnosticsButton.addEventListener('click', refreshOtaDiagnostics);
    }
    document.addEventListener('change', function (event) {
        if (event.target === elements.localFileInput || event.target === elements.localWebBundleFileInput) {
            syncLocalFileButtonStates();
        }
    }, true);
    window.addEventListener('focus', syncLocalFileButtonStates);
    window.addEventListener('pageshow', syncLocalFileButtonStates);
    setInterval(syncLocalFileButtonStates, 1200);

    syncLocalFileButtonStates();
    refreshFirmwarePageStatus();
});
