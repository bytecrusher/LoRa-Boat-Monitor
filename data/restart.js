let restartPollInterval = null;

function restartById(id) {
    return typeof byId === "function" ? byId(id) : document.getElementById(id);
}

function restartToggleClass(id, className, enabled) {
    var element = restartById(id);
    if (!element) {
        return;
    }

    if (typeof toggleClass === "function") {
        toggleClass(id, className, enabled);
    } else {
        element.classList.toggle(className, Boolean(enabled));
    }

    if (className === "hidden") {
        element.style.display = enabled ? "none" : "block";
    }
}

async function restartRequest(url, options) {
    if (typeof request === "function") {
        return request(url, options);
    }

    var requestOptions = options || {};
    if (typeof withCsrf === "function") {
        requestOptions = withCsrf(requestOptions);
    }

    var response = await fetch(url, requestOptions);
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

function setRestartState(isRestarting) {
    restartToggleClass("loader", "hidden", !isRestarting);
    restartToggleClass("myDiv", "hidden", isRestarting);
}

function restartResponseMessage(response, fallback) {
    if (response && response.json && response.json.message) {
        return response.json.message;
    }

    if (response && response.text) {
        return response.text;
    }

    if (response && response.status) {
        return fallback + " (HTTP " + response.status + ")";
    }

    return fallback;
}

function pollRestartUntilOnline() {
    var deviceWasOffline = false;

    async function checkDevice() {
        restartPollInterval = null;

        try {
            var response = await restartRequest("/health?ts=" + Date.now(), { cache: "no-store" });
            if (!response.ok) {
                deviceWasOffline = true;
            } else if (deviceWasOffline) {
                setRestartState(false);
                window.location.replace("/");
                return;
            }
        } catch (error) {
            deviceWasOffline = true;
        }

        restartPollInterval = window.setTimeout(checkDevice, 900);
    }

    restartPollInterval = window.setTimeout(checkDevice, 300);
}

async function restartDevice() {
    if (!window.confirm("Are you sure you want to restart the device?")) {
        return;
    }

    setRestartState(true);
    try {
        var response = await restartRequest("/restart", { method: "POST" });
        if (!response.ok) {
            throw new Error(restartResponseMessage(response, "Restart request failed."));
        }

        pollRestartUntilOnline();
    } catch (error) {
        setRestartState(false);
        window.alert(error && error.message ? error.message : "Restart request failed.");
    }
}

document.addEventListener("DOMContentLoaded", function () {
    setRestartState(false);

    var restartButton = restartById("restartDeviceButton");
    if (restartButton) {
        restartButton.addEventListener("click", restartDevice);
    }

    var backButton = restartById("restartBackButton");
    if (backButton) {
        backButton.addEventListener("click", function () {
            window.location.assign("/");
        });
    }
});
