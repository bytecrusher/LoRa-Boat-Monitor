var webFilesProgressStop = null;

function setFileManagerBusy(busy) {
    toggleClass('loader', 'hidden', !busy);
    toggleClass('myDiv', 'hidden', busy);
}

function updateFilesystemUsage() {
    var section = document.querySelector('.file-manager-shell .page-section[data-used-bytes]');
    if (!section) return;
    var used = Number(section.dataset.usedBytes || 0);
    var total = Number(section.dataset.totalBytes || 0);
    var percent = total > 0 ? Math.min(100, Math.max(0, Math.round(used / total * 100))) : 0;
    var fill = byId('filesystemUsageBar');
    if (fill) fill.style.width = percent + '%';
    setText('filesystemUsageText', percent + '%');
}

function formatBytes(value) {
    var bytes = Number(value || 0);
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
}

function progressDescription(info) {
    if (info.currentFile && info.total > 0) {
        var values = info.progressUnit === 'bytes'
            ? formatBytes(info.completed) + ' / ' + formatBytes(info.total)
            : info.completed + '/' + info.total;
        return info.currentFile + ' (' + values + ')';
    }
    return info.total > 0 ? info.completed + ' of ' + info.total + ' files' : '';
}

function renderUpdateFilesInfo(info) {
    var button = byId('updatefilesbutton');
    var progressFill = byId('updatefilesprogressfill');
    if (!button || !info) return;

    var percent = Math.min(100, Math.max(0, Number(info.percent || 0)));
    var detail = progressDescription(info);
    button.disabled = Boolean(info.busy);
    toggleClass('updatefilesprogress', 'hidden', !info.busy);
    if (progressFill) progressFill.style.width = percent + '%';
    setText('updatefilesprogresstext', info.busy ? percent + '%' + (detail ? ' - ' + detail : '') : '');

    if (info.busy) {
        button.textContent = 'Downloading Files from Server...';
        setText('updatefilesinfo', info.message || 'Downloading web files for ' + info.firmwareVersion + '.');
    } else if (info.error) {
        button.textContent = 'Retry Download';
        setText('updatefilesinfo', info.message || 'Web files download failed. Please retry.');
    } else if (info.upToDate) {
        button.textContent = 'Reinstall Web Files';
        setText('updatefilesinfo', 'Web files are up to date for installed firmware ' + info.firmwareVersion + '.');
    } else {
        button.textContent = 'Get Files from Server';
        setText('updatefilesinfo', info.storedWebFilesVersion
            ? 'Installed firmware: ' + info.firmwareVersion + ' | Stored web files: ' + info.storedWebFilesVersion + ' | Download recommended.'
            : 'No stored web file version found. Download recommended for firmware ' + info.firmwareVersion + '.');
    }
}

async function refreshUpdateFilesInfo(progressEndpoint) {
    var endpoint = progressEndpoint ? '/updatefilesprogress' : '/updatefilesinfo';
    var info = await requestJson(endpoint + '?ts=' + Date.now());
    renderUpdateFilesInfo(info);
    return info;
}

function stopWebFilesProgress() {
    if (webFilesProgressStop) webFilesProgressStop();
    webFilesProgressStop = null;
}

function watchWebFilesProgress() {
    stopWebFilesProgress();
    webFilesProgressStop = startVisiblePolling(async function () {
        var info = await refreshUpdateFilesInfo(true);
        if (!info.busy) {
            stopWebFilesProgress();
            await refreshUpdateFilesInfo(false);
        }
    }, 700);
}

async function updateWebFiles() {
    try {
        var result = await request('/updatefiles', { method: 'POST' });
        if (!result.ok) throw result;
        renderUpdateFilesInfo({ busy: true, percent: 0, message: 'Starting web files download...' });
        watchWebFilesProgress();
    } catch (error) {
        alert('Unable to start web files download. (HTTP ' + (error.status || 0) + ')');
        await refreshUpdateFilesInfo(false).catch(function () {});
    }
}

async function formatFilesystem() {
    if (!confirm('Format the filesystem and delete all stored files?')) return;
    setFileManagerBusy(true);
    try {
        var result = await request('/formatfs', { method: 'POST' });
        if (!result.ok) throw result;
        alert('Formatting filesystem succeeded.');
        window.location.reload();
    } catch (error) {
        setFileManagerBusy(false);
        alert('Error while formatting filesystem. (HTTP ' + (error.status || 0) + ')');
    }
}

async function loadFileTable() {
    setFileManagerBusy(true);
    try {
        var result = await request('/gettable');
        if (!result.ok) throw result;
        var table = byId('table');
        if (table) table.innerHTML = result.text;
    } catch (error) {
        alert('Error while loading the file table. (HTTP ' + (error.status || 0) + ')');
    } finally {
        setFileManagerBusy(false);
    }
}

document.addEventListener('DOMContentLoaded', function () {
    setFileManagerBusy(false);
    updateFilesystemUsage();
    byId('showFilesButton').addEventListener('click', loadFileTable);
    byId('updatefilesbutton').addEventListener('click', updateWebFiles);
    byId('formatFilesystemButton').addEventListener('click', formatFilesystem);
    byId('filesystemOverviewButton').addEventListener('click', function () { navigateTo('/'); });
    byId('upload').addEventListener('change', function () {
        if (this.files && this.files.length > 0) byId('upload_form').submit();
    });
    refreshUpdateFilesInfo(false).then(function (info) {
        if (info.busy) watchWebFilesProgress();
    }).catch(function () {
        setText('updatefilesinfo', 'Unable to read web file status.');
    });
});
