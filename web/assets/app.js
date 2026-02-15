(function () {
    var outBtn = document.getElementById("out");
    var kernelStatus = document.getElementById("kernel-status");
    var titleNode = document.getElementById("view-title");
    var navButtons = Array.prototype.slice.call(document.querySelectorAll(".navbtn[data-view]"));
    var viewNodes = Array.prototype.slice.call(document.querySelectorAll("[data-view-content]"));
    var views = {};
    var viewTitles = {
        tunnels: "Туннели",
        routing: "Маршрутизация"
    };

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

        if (updateHash) {
            if (window.location.hash !== "#" + viewName) {
                window.location.hash = viewName;
            }
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

    initViews();
    refreshKernelState();
    setInterval(refreshKernelState, 5000);
})();
