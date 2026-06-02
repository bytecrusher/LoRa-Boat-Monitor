let restartPollInterval = null;

function restartById(id) {
    return typeof byId === "function" ? byId(id) : document.getElementById(id);
}

function restartToggleClass(id, className, enabled) {
    if (typeof toggleClass === "function") {
        toggleClass(id, className, enabled);
        return;
    }

    var element = restartById(id);
    if (element) {
        element.classList.toggle(className, Boolean(enabled));
    }
}

async function restartRequest(url, options) {
    if (typeof request === "function") {
        return request(url, options);
    }

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
