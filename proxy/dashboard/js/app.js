/* Dashboard application init */
var Dashboard = Dashboard || {};

Dashboard.app = (function() {
    function init() {
        Dashboard.router.init();
        Dashboard.sidebar.init();
        Dashboard.auth.init();

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
                window.location.href = 'http://127.0.0.1:5173';
            }
        });
        Dashboard.router.register('coding', {
            mount: function() {
                window.location.href = 'http://127.0.0.1:5174';
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
