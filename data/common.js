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

    element.innerHTML = value;
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

function delay(milliseconds) {
    return new Promise(function (resolve) {
        setTimeout(resolve, milliseconds);
    });
}

async function request(url, options) {
    var response = await fetch(url, options || {});
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
