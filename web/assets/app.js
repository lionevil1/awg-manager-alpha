(function () {
    var outBtn = document.getElementById("out");
    var kernelStatus = document.getElementById("kernel-status");
    var titleNode = document.getElementById("view-title");
    var updateBtn = document.getElementById("update-btn");
    var versionNode = document.getElementById("app-version");
    var navButtons = Array.prototype.slice.call(document.querySelectorAll(".navbtn[data-view]"));
    var viewNodes = Array.prototype.slice.call(document.querySelectorAll("[data-view-content]"));
    var views = {};
    var viewTitles = {
        tunnels: "Туннели",
        routing: "Маршрутизация"
    };
    var updateState = {
        state: "idle",
        update_available: false,
        current_version: "unknown",
        latest_version: ""
    };
    var lastActivity = Date.now();
    var idleTimeout = 180000;

    function resetIdleTimer() {
        lastActivity = Date.now();
    }

    function checkIdle() {
        if (Date.now() - lastActivity > idleTimeout) {
            fetch("/api/logout", { method: "POST" }).catch(function() {});
            window.location.href = "/";
        }
    }

    function setKernelState(state) {
        if (!kernelStatus) {
            return;
        }

        kernelStatus.classList.remove("is-loaded", "is-unloaded", "is-unknown");

        if (state === "loaded") {
            kernelStatus.classList.add("is-loaded");
            kernelStatus.title = "AmneziaWG Kernel: загружен";
            return;
        }

        if (state === "unloaded") {
            kernelStatus.classList.add("is-unloaded");
            kernelStatus.title = "AmneziaWG Kernel: не загружен";
            return;
        }

        kernelStatus.classList.add("is-unknown");
        kernelStatus.title = "AmneziaWG Kernel: состояние неизвестно";
    }

    function isUpdateInProgress(state) {
        return state === "checking" || state === "downloading" || state === "installing";
    }

    function setUpdateButtonText(text) {
        if (updateBtn) {
            updateBtn.textContent = text;
        }
    }

    function setUpdateUi(status) {
        var state;
        var isAvailable;

        if (!updateBtn || !versionNode) {
            return;
        }

        state = status && status.state ? status.state : "idle";
        isAvailable = !!(status && status.update_available === true);

        versionNode.textContent = "Version: " + (status && status.current_version ? status.current_version : "unknown");

        updateBtn.classList.remove("is-ready");
        updateBtn.disabled = true;

        if (isUpdateInProgress(state)) {
            if (state === "checking") {
                setUpdateButtonText("Проверка...");
            } else if (state === "downloading") {
                setUpdateButtonText("Загрузка...");
            } else {
                setUpdateButtonText("Установка...");
            }
            return;
        }

        if (isAvailable) {
            updateBtn.disabled = false;
            updateBtn.classList.add("is-ready");
            setUpdateButtonText("Обновить");
            return;
        }

        if (state === "error") {
            updateBtn.disabled = false;
            setUpdateButtonText("Повторить");
            return;
        }

        if (state === "done") {
            setUpdateButtonText("Обновлено");
            return;
        }

        setUpdateButtonText("Актуально");
    }

    async function refreshKernelState() {
        var response;
        var payload;

        if (!kernelStatus) {
            return;
        }

        try {
            response = await fetch("/api/kernel-status", {
                method: "GET",
                cache: "no-store"
            });

            if (!response.ok) {
                setKernelState("unknown");
                return;
            }

            payload = await response.json();
            if (payload && payload.loaded === true) {
                setKernelState("loaded");
            } else if (payload && payload.loaded === false) {
                setKernelState("unloaded");
            } else {
                setKernelState("unknown");
            }
        } catch (_err) {
            setKernelState("unknown");
        }
    }

    async function refreshUpdateStatus() {
        var response;
        var payload;

        if (!updateBtn || !versionNode) {
            return;
        }

        try {
            response = await fetch("/api/update/status", {
                method: "GET",
                cache: "no-store"
            });

            if (!response.ok) {
                return;
            }

            payload = await response.json();
            if (!payload || typeof payload !== "object") {
                return;
            }

            updateState = payload;
            setUpdateUi(payload);
        } catch (_err) {
            /* noop */
        }
    }

    async function triggerUpdateCheck() {
        try {
            await fetch("/api/update/check", {
                method: "POST"
            });
        } catch (_err) {
            /* noop */
        }
    }

    async function triggerUpdateApply() {
        try {
            await fetch("/api/update/apply", {
                method: "POST"
            });
        } catch (_err) {
            /* noop */
        }
    }

    async function handleUpdateClick() {
        if (!updateBtn) {
            return;
        }

        updateBtn.disabled = true;

        if (updateState && updateState.update_available === true) {
            setUpdateButtonText("Запуск...");
            await triggerUpdateApply();
            setTimeout(refreshUpdateStatus, 1000);
            return;
        }

        setUpdateButtonText("Проверка...");
        await triggerUpdateCheck();
        setTimeout(refreshUpdateStatus, 1000);
    }

    function getViewFromHash() {
        var hash = window.location.hash || "";
        if (hash.indexOf("#") === 0) {
            hash = hash.slice(1);
        }
        if (!hash || !views[hash]) {
            return "tunnels";
        }
        return hash;
    }

    function setActiveView(viewName, updateHash) {
        var i;

        if (!views[viewName]) {
            viewName = "tunnels";
        }

        for (i = 0; i < navButtons.length; i++) {
            var btn = navButtons[i];
            var isActiveBtn = btn.getAttribute("data-view") === viewName;
            btn.classList.toggle("is-active", isActiveBtn);
            btn.setAttribute("aria-pressed", isActiveBtn ? "true" : "false");
        }

        for (i = 0; i < viewNodes.length; i++) {
            var node = viewNodes[i];
            var isActiveView = node.getAttribute("data-view-content") === viewName;
            node.classList.toggle("is-active", isActiveView);
        }

        if (titleNode && viewTitles[viewName]) {
            titleNode.textContent = viewTitles[viewName];
        }

        if (updateHash && window.location.hash !== "#" + viewName) {
            window.location.hash = viewName;
        }
    }

    function initViews() {
        var i;

        for (i = 0; i < viewNodes.length; i++) {
            var key = viewNodes[i].getAttribute("data-view-content");
            if (key) {
                views[key] = true;
            }
        }

        for (i = 0; i < navButtons.length; i++) {
            navButtons[i].addEventListener("click", function () {
                var targetView = this.getAttribute("data-view");
                setActiveView(targetView, true);
            });
        }

        window.addEventListener("hashchange", function () {
            setActiveView(getViewFromHash(), false);
        });

        setActiveView(getViewFromHash(), false);
    }

    if (outBtn) {
        outBtn.addEventListener("click", async function () {
            try {
                await fetch("/api/logout", { method: "POST" });
            } catch (_err) {
                /* noop */
            }
            window.location.href = "/";
        });
    }

    if (updateBtn) {
        updateBtn.addEventListener("click", handleUpdateClick);
    }

    initViews();
    refreshKernelState();
    triggerUpdateCheck();
    refreshUpdateStatus();
    setInterval(refreshKernelState, 5000);
    setInterval(refreshUpdateStatus, 5000);
    setInterval(function () {
        if (!isUpdateInProgress(updateState.state || "")) {
            triggerUpdateCheck();
        }
    }, 60000);
    setInterval(checkIdle, 10000);
    document.addEventListener("mousemove", resetIdleTimer);
    document.addEventListener("keydown", resetIdleTimer);
    document.addEventListener("click", resetIdleTimer);
    document.addEventListener("scroll", resetIdleTimer);
})();
