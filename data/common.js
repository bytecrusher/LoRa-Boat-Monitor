function byId(id) {
    return document.getElementById(id);
}

function setElementValue(id, value) {
    var element = byId(id);
    if (!element) {
        return;
    }

    if (element.tagName === 'INPUT' || element.tagName === 'TEXTAREA' || element.tagName === 'SELECT') {
        element.value = value;
        return;
    }

    element.textContent = value == null ? '' : value;
}

function setText(id, value) {
    var element = byId(id);
    if (!element) {
        return;
    }
    element.textContent = value;
}

function setHtmlPageAssetVersion(version) {
    var assetVersion = version || '';
    if (!assetVersion) {
        return;
    }

    var nodes = document.querySelectorAll('link[href], script[src]');
    for (var i = 0; i < nodes.length; i += 1) {
        var node = nodes[i];
        var attributeName = node.tagName === 'LINK' ? 'href' : 'src';
        var currentValue = node.getAttribute(attributeName);
        if (!currentValue || currentValue.indexOf('http') === 0 || currentValue.indexOf('data:') === 0) {
            continue;
        }
        if (currentValue.indexOf('?v=') >= 0) {
            continue;
        }
        node.setAttribute(attributeName, currentValue + (currentValue.indexOf('?') >= 0 ? '&' : '?') + 'v=' + encodeURIComponent(assetVersion));
    }
}

function toggleClass(id, className, enabled) {
    var element = byId(id);
    if (!element) {
        return;
    }

    element.classList.toggle(className, Boolean(enabled));
}

function setElementHidden(id, hidden) {
    toggleClass(id, 'hidden', hidden);
}

function setElementsDisabled(selector, disabled, root) {
    var scope = root || document;
    var nodes = scope.querySelectorAll(selector);
    for (var i = 0; i < nodes.length; i += 1) {
        nodes[i].disabled = disabled;
    }
}

function startVisiblePolling(callback, intervalMs) {
    var timer = null;
    var running = false;

    function run() {
        if (document.hidden || running) {
            return;
        }
        running = true;
        Promise.resolve(callback()).catch(function () {}).finally(function () {
            running = false;
            if (!document.hidden && timer !== null) {
                timer = window.setTimeout(run, intervalMs);
            }
        });
    }

    function start() {
        if (timer !== null) {
            return;
        }
        timer = window.setTimeout(run, 0);
    }

    function stop() {
        if (timer === null) {
            return;
        }
        window.clearTimeout(timer);
        timer = null;
    }

    document.addEventListener('visibilitychange', function () {
        if (document.hidden) {
            stop();
        } else {
            start();
        }
    });

    start();
    return stop;
}

function navigateTo(url, target) {
    if (target) {
        window.open(url, target);
        return;
    }
    window.location.assign(url);
}

function delay(milliseconds) {
    return new Promise(function (resolve) {
        setTimeout(resolve, milliseconds);
    });
}

function csrfToken() {
    var meta = document.querySelector('meta[name="csrf-token"]');
    return meta ? meta.getAttribute('content') : '';
}

function withCsrf(options) {
    var requestOptions = options || {};
    var method = (requestOptions.method || 'GET').toUpperCase();
    if (method === 'GET' || method === 'HEAD' || method === 'OPTIONS') {
        return requestOptions;
    }

    var token = csrfToken();
    if (!token) {
        return requestOptions;
    }

    requestOptions.headers = requestOptions.headers || {};
    requestOptions.headers['X-CSRF-Token'] = token;
    return requestOptions;
}

async function request(url, options) {
    var sourceOptions = options || {};
    var requestOptions = {};
    Object.keys(sourceOptions).forEach(function (key) {
        if (key !== 'timeoutMs') {
            requestOptions[key] = sourceOptions[key];
        }
    });
    var timeoutMs = Number(sourceOptions.timeoutMs || 10000);
    var controller = typeof AbortController !== 'undefined' ? new AbortController() : null;
    var timeout = null;
    if (controller && !requestOptions.signal) {
        requestOptions.signal = controller.signal;
        timeout = window.setTimeout(function () {
            controller.abort();
        }, timeoutMs);
    }

    try {
        var response = await fetch(url, withCsrf(requestOptions));
        var text = await response.text();
        var json = null;

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
    } finally {
        if (timeout !== null) {
            window.clearTimeout(timeout);
        }
    }
}

async function requestJson(url, options) {
    var result = await request(url, options);
    if (!result.ok) {
        throw result;
    }
    return result.json || {};
}

function fetchJson(url, onSuccess, onError) {
    return requestJson(url)
        .then(onSuccess)
        .catch(function (error) {
            if (onError) {
                onError(error);
            }
        });
}
