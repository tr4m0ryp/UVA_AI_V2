/* Dashboard application init */
var Dashboard = Dashboard || {};

Dashboard.app = (function() {
    /* Detect standalone/installed app mode (Chrome --app or PWA install) */
    var isAppMode = window.matchMedia('(display-mode: standalone)').matches ||
                    window.navigator.standalone === true;

    function initAppMode() {
        if (!isAppMode) return;
        document.body.classList.add('app-mode');

        /* When the user closes the window (X button), shut down the proxy.
         * sendBeacon is reliable during page unload -- fetch is not. */
        window.addEventListener('beforeunload', function() {
            navigator.sendBeacon('/api/dashboard/shutdown');
        });
    }

    function loadWebUI(iframeId) {
        var frame = document.getElementById(iframeId);
        if (!frame || frame.src) return;
        var container = frame.parentElement;

        function checkStatus() {
            fetch('/api/dashboard/webui/status')
                .then(function(r) { return r.json(); })
                .then(function(data) {
                    if (data.ready) {
                        frame.src = 'http://127.0.0.1:' + data.port;
                    } else {
                        container.innerHTML =
                            '<div class="service-offline">' +
                            '<h3>Chat interface is starting up...</h3>' +
                            '<p>Open WebUI is loading. This page will refresh automatically.</p></div>';
                        setTimeout(function() {
                            container.innerHTML = '';
                            container.appendChild(frame);
                            checkStatus();
                        }, 3000);
                    }
                })
                .catch(function() {
                    container.innerHTML =
                        '<div class="service-offline">' +
                        '<h3>Chat interface is not available</h3>' +
                        '<p>Could not reach the proxy status endpoint.</p></div>';
                });
        }
        checkStatus();
    }

    function init() {
        Dashboard.router.init();
        Dashboard.sidebar.init();
        Dashboard.auth.init();
        initAppMode();

        /* Mobile sidebar toggle */
        var mobileBtn = document.getElementById('mobile-sidebar-toggle');
        if (mobileBtn) {
            mobileBtn.addEventListener('click', Dashboard.sidebar.toggle);
        }

        /* Register views */
        Dashboard.router.register('overview', {
            mount: Dashboard.overview.mount
        });
        Dashboard.router.register('keys', {
            mount: Dashboard.keys.mount,
            unmount: Dashboard.keys.unmount
        });
        Dashboard.router.register('grading', {
            mount: Dashboard.grading.mount,
            unmount: Dashboard.grading.unmount
        });
        Dashboard.router.register('chat', {
            mount: function() {
                loadWebUI('chat-iframe');
            }
        });
        Dashboard.router.register('coding', {
            mount: Dashboard.coding.mount,
            unmount: Dashboard.coding.unmount
        });

        /* Check for existing session */
        var token = Dashboard.api.getToken();
        if (token) {
            Dashboard.api.get('/api/dashboard/auth/me')
                .then(function(user) {
                    showDashboard(user.email, user.name);
                })
                .catch(function() {
                    showLogin();
                });
        } else {
            showLogin();
        }
    }

    function showLogin() {
        document.getElementById('app-shell').classList.add('hidden');
        document.getElementById('page-login').classList.remove('hidden');
        Dashboard.router.reset();
    }

    function showDashboard(email, name) {
        Dashboard.sidebar.setUser(name || email);
        document.getElementById('page-login').classList.add('hidden');
        document.getElementById('app-shell').classList.remove('hidden');
        Dashboard.router.start();
    }

    /* Init on DOM ready */
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }

    return {
        showLogin: showLogin,
        showDashboard: showDashboard
    };
})();
