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

    function loadServiceIframe(iframeId, url, serviceName, setupHint) {
        var frame = document.getElementById(iframeId);
        if (!frame || frame.src) return;
        fetch(url, { mode: 'no-cors' })
            .then(function() { frame.src = url; })
            .catch(function() {
                frame.parentElement.innerHTML =
                    '<div class="service-offline">' +
                    '<h3>' + serviceName + ' is not running</h3>' +
                    '<p>' + setupHint + '</p></div>';
            });
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
                loadServiceIframe('chat-iframe', 'http://127.0.0.1:5173',
                    'Chat Interface', 'Run ./start-all.sh to launch all services including Open WebUI.');
            }
        });
        Dashboard.router.register('coding', {
            mount: function() {
                loadServiceIframe('coding-iframe', 'http://127.0.0.1:5174',
                    'Cloud Coding', 'Run ./start-all.sh to launch all services including opencode-web.');
            }
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
