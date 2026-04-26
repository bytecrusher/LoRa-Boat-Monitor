let headerOfflineTimer = 0;

function setHeaderStatus(isOnline) {
    var led = document.getElementById("myping");
    var text = document.getElementById("mypingtxt");
    if (!led || !text) {
        return;
    }

    led.classList.toggle("led-green", isOnline);
    led.classList.toggle("led-red", !isOnline);
    text.textContent = isOnline ? "Online" : "Offline";
}

function scheduleHeaderOfflineState() {
    if (headerOfflineTimer !== 0) {
        return;
    }

    headerOfflineTimer = window.setTimeout(function () {
        headerOfflineTimer = 0;
        setHeaderStatus(false);
    }, 1100);
}

async function pingHeaderStatus() {
    scheduleHeaderOfflineState();

    try {
        var response = await fetch("/getdata", { cache: "no-store" });
        if (!response.ok) {
            throw new Error("Request failed");
        }

        if (headerOfflineTimer !== 0) {
            clearTimeout(headerOfflineTimer);
            headerOfflineTimer = 0;
        }

        setHeaderStatus(true);
        window.setTimeout(function () {
            var led = document.getElementById("myping");
            if (led) {
                led.classList.remove("led-green");
            }
        }, 400);
    } catch (error) {
    }
}

window.setInterval(pingHeaderStatus, 1000);
pingHeaderStatus();
