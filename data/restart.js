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

async function restartDevice() {
    if (!window.confirm("Are you sure you want to restart the device?")) {
        return;
    }

    setRestartState(true);
    restartRequest("/restart", { method: "POST" }).catch(function () {
        // The device may reboot before the browser receives the response.
    });

    restartPollInterval = window.setInterval(function () {
        var statusLed = restartById("myping");
        if (statusLed && statusLed.classList.contains("led-green")) {
            setRestartState(false);
            clearInterval(restartPollInterval);
            window.location.replace("/");
        }
    }, 900);
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
