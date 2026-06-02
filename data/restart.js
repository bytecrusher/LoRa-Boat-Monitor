let restartPollInterval = null;

function setRestartState(isRestarting) {
    toggleClass("loader", "hidden", !isRestarting);
    toggleClass("myDiv", "hidden", isRestarting);
}

async function restartDevice() {
    if (!window.confirm("Are you sure you want to restart the device?")) {
        return;
    }

    setRestartState(true);
    request("/restart", { method: "POST" }).catch(function () {
        // The device may reboot before the browser receives the response.
    });

    restartPollInterval = window.setInterval(function () {
        var statusLed = byId("myping");
        if (statusLed && statusLed.classList.contains("led-green")) {
            setRestartState(false);
            clearInterval(restartPollInterval);
            window.location.replace("/");
        }
    }, 900);
}

document.addEventListener("DOMContentLoaded", function () {
    var restartButton = byId("restartDeviceButton");
    if (restartButton) {
        restartButton.addEventListener("click", restartDevice);
    }

    var backButton = byId("restartBackButton");
    if (backButton) {
        backButton.addEventListener("click", function () {
            window.location.assign("/");
        });
    }
});
