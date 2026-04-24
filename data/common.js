function setElementValue(id, value) {
    var element = document.getElementById(id);
    if (!element) {
        return;
    }

    if (element.tagName === 'INPUT' || element.tagName === 'TEXTAREA' || element.tagName === 'SELECT') {
        element.value = value;
    } else {
        element.innerHTML = value;
    }
}

function fetchJson(url, onSuccess, onError) {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
        if (xhr.readyState !== 4) {
            return;
        }

        if (xhr.status >= 200 && xhr.status < 300) {
            try {
                onSuccess(JSON.parse(xhr.responseText));
            } catch (error) {
                if (onError) {
                    onError(error);
                }
            }
            return;
        }

        if (onError) {
            onError(xhr);
        }
    };
    xhr.open('GET', url, true);
    xhr.send();
}

function toggleClass(id, className, enabled) {
    var element = document.getElementById(id);
    if (!element) {
        return;
    }

    if (enabled) {
        element.classList.add(className);
    } else {
        element.classList.remove(className);
    }
}

function setElementHidden(id, hidden) {
    toggleClass(id, 'hidden', hidden);
}
