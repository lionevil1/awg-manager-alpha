(function () {
    var form = document.getElementById("f");
    var msg = document.getElementById("msg");
    var btn = document.getElementById("btn");

    if (!form || !msg || !btn) {
        return;
    }

    form.addEventListener("submit", async function (event) {
        var formData;
        var controller;
        var timer;

        event.preventDefault();
        msg.textContent = "Проверка...";
        btn.disabled = true;

        formData = new URLSearchParams(new FormData(form));
        controller = new AbortController();
        timer = setTimeout(function () {
            controller.abort();
        }, 10000);

        try {
            var response = await fetch("/api/login", {
                method: "POST",
                headers: {
                    "Content-Type": "application/x-www-form-urlencoded"
                },
                body: formData,
                signal: controller.signal
            });

            if (response.ok) {
                window.location.href = "/app";
                return;
            }

            msg.textContent = "Неверный логин или пароль.";
        } catch (_err) {
            msg.textContent = "Нет ответа от сервиса. Попробуйте снова.";
        } finally {
            clearTimeout(timer);
            btn.disabled = false;
        }
    });
})();
