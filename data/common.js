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
    var response = await fetch(url, withCsrf(options));
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
}

async function requestJson(url, options) {
    var result = await request(url, options);
    if (!result.ok) {
        throw result;
    }
    return result.json || {};
}

function fetchJson(url, onSuccess, onError) {
    requestJson(url)
        .then(onSuccess)
        .catch(function (error) {
            if (onError) {
                onError(error);
            }
        });
}
